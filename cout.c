#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "parser.h"
#include "grammar.h"
#include "ir.h"
#include "types.h"
#include "ssm.h"
#include "builtins.h"

#define NO_DEFAULT default: assert(!"reached")

typedef enum { C_UNION, C_RAW, C_INT, C_BOOL, C_LIST, C_TUPLE } splctype;

static void convert_splctype(irexp *ir, splctype to, splctype from);
static void irexp_to_c(irexp *ir, splctype how);
static void irstm_to_c(irstm *ir, int prototype);
void ir_to_c(irstm *ir);

static void
convert_splctype(irexp *ir, splctype to, splctype from) {
	assert(to != from);
	if(from == C_UNION) {
		irexp_to_c(ir, from);
		switch(to) {
			case C_INT:
				printf(".ival");
				return;
			case C_BOOL:
				printf(".bval");
				return;
			case C_LIST:
				printf(".lval");
				return;
			case C_TUPLE:
				printf(".uval");
				return;
			case C_RAW:
				return;
			NO_DEFAULT;
		}
	}
	switch(to) {
		case C_RAW:
			return irexp_to_c(ir, from);
		case C_UNION:
			printf("(spltype)(");
			irexp_to_c(ir, from);
			printf(")");
			break;
		NO_DEFAULT;
	}
}

static void
irexp_to_c(irexp *ir, splctype how) {
	switch(ir->type) {
		case CONST:
			if(how == C_LIST && ir->value == 0) {
				printf("NULL");
				return;
			}
			if(how != C_INT && how != C_BOOL) {
				return convert_splctype(ir, how, C_INT);
			}
			printf("%d", ir->value);
			break;
		case BINOP: ;
			splctype optype;
			switch(ir->binop.op) {
				case EQ:
				case NE:
				case AND:
				case OR:
				case XOR:
					optype = C_BOOL;
					break;
				default:
					optype = C_INT;
			}
			if(optype != how) {
				return convert_splctype(ir, how, optype);
			}
			irexp_to_c(ir->binop.left, optype);
			switch(ir->binop.op) {
#define CONVERT_BINOP(x, y)	case x: printf(#y); break
				CONVERT_BINOP(PLUS, +);
				CONVERT_BINOP(MINUS, -);
				CONVERT_BINOP(EQ, ==);
				CONVERT_BINOP(MUL, *);
				CONVERT_BINOP(DIV, /);
				CONVERT_BINOP(MOD, %%); // Dubbel om printf te escapen
				CONVERT_BINOP(AND, &&);
				CONVERT_BINOP(OR, ||);
				CONVERT_BINOP(XOR, ^);
				CONVERT_BINOP(NE, !=);
				CONVERT_BINOP(LT, <);
				CONVERT_BINOP(GT, >);
				CONVERT_BINOP(GE, >=);
				CONVERT_BINOP(LE, <=);
				CONVERT_BINOP(LSHIFT, <<);
				CONVERT_BINOP(RSHIFT, >>);
				case ARSHIFT:
					assert(!"yet implemented");
				NO_DEFAULT;
			}
			irexp_to_c(ir->binop.right, optype);
			break;
		case LOCAL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("l%d", ir->local);
			break;
		case GLOBAL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("g%d", ir->global);
			break;
		case FARG:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("a%d", ir->farg - 1); // Bah, ze beginnen bij 1 te tellen. My bad
			break;
		case CALL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("f%d(", ir->call.func);
			struct irexplist *args = ir->call.args;
			while(args != NULL) {
				irexp_to_c(args->exp, C_UNION);
				args = args->next;
				if(args == NULL) {
					break;
				}
				printf(", ");
			}
			printf(")");
			break;
		case LISTEL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("create_listel(");
			irexp_to_c(ir->listel.exp, C_UNION);
			printf(", ");
			irexp_to_c(ir->listel.next, C_LIST);
			printf(")");
			break;
		case TUPLE:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION);
			}
			printf("create_tuple(");
			irexp_to_c(ir->tuple.fst, C_UNION);
			printf(", ");
			irexp_to_c(ir->tuple.fst, C_UNION);
			printf(")");
			break;
		NO_DEFAULT;
	}
}

void
irstm_to_c(irstm *ir, int prototype) {
	if(prototype && ir->type != SEQ && ir->type != FUNC) {
		return;
	}
	switch(ir->type) {
		case SEQ:
			irstm_to_c(ir->seq.left, prototype);
			irstm_to_c(ir->seq.right, prototype);
			break;
		case MOVE:
			printf("\t");
			irexp_to_c(ir->move.dst, C_RAW);
			printf(" = ");
			irexp_to_c(ir->move.src, C_UNION);
			puts(";");
			break;
		case EXP:
			printf("\t");
			irexp_to_c(ir->exp, C_RAW);
			puts(";");
			break;
		case JUMP:
			printf("\tgoto lbl%04d;\n", get_ssmlabel_from_irlabel(ir->jump));
			break;
		case CJUMP:
			printf("\tif(!");
			irexp_to_c(ir->cjump.exp, C_BOOL);
			printf(")\n\t\tgoto lbl%04d;\n", get_ssmlabel_from_irlabel(ir->cjump.iffalse));
			break;
		case LABEL:
			printf("lbl%04d: \n", get_ssmlabel_from_irlabel(ir->label));
			break;
		case RET:
			printf("\treturn ");
			irexp_to_c(ir->ret, C_UNION);
			puts(";");
			break;
		case TRAP:
			switch(ir->trap.syscall) {
				case 0:
					printf("\tprintf(\"%%d\\n\", ");
					irexp_to_c(ir->trap.arg, C_INT);
					puts(");");
					break;
				NO_DEFAULT;
			}
			break;
		case HALT:
			puts("\texit(0);");
			break;
		case FUNC:
			if(!prototype) {
				puts("}\n");
			}
			printf("spltype f%d(", ir->func.funcid);
			if(ir->func.args > 0) {
				printf("spltype a0");
				int i;
				for(i = 1; ir->func.args > i; i++) {
					printf(", spltype a%d", i);
				}
			}
			if(prototype) {
				puts(");");
			} else {
				puts(") {");
				if(ir->func.vars > 0) {
					printf("\tspltype l0");
					int i;
					for(i = 1; ir->func.vars > i; i++) {
						printf(", l%d", i);
					}
					puts(";");
				}
			}
			break;
		NO_DEFAULT;
	}
}

void
ir_to_c(irstm *ir) {
	puts("#include <stdio.h>");
	puts("#include <stdlib.h>");
	puts("struct _spllist;");
	puts("struct _spltuple;");
	puts("typedef union _spltype {");
	puts("	int ival;");
	puts("	char bval;");
	puts("	struct _spllist *lval;");
	puts("	struct _spltuple *tval;");
	puts("} spltype;");
	puts("typedef struct _spllist {");
	puts("	spltype value;");
	puts("	struct _spllist *next;");
	puts("} spllist;");
	puts("typedef struct _spltuple {");
	puts("	spltype fst;");
	puts("	spltype snd;");
	puts("} spltuple;");
	puts("");
	puts("/* prototypes */");
	irstm_to_c(ir, 1);
	puts("");
	puts("spltype create_listel(spltype el, spllist *list) {");
	puts("	spltype ret;");
	puts("	ret.lval = malloc(sizeof(spllist));");
	puts("	ret.lval->value = el;");
	puts("	ret.lval->next = list;");
	puts("	return ret;");
	puts("}");
	puts("spltype create_tuple(spltype fst, spltype snd) {");
	puts("	spltype ret;");
	puts("	ret.tval = malloc(sizeof(spltuple));");
	puts("	ret.tval->fst = fst;");
	puts("	ret.tval->snd = snd;");
	puts("	return ret;");
	puts("}");
	puts("");
	c_builtin_functions();
	puts("");
	puts("int main(int argc, char **argv) {");
	irstm_to_c(ir, 0);
	puts("}");
}
