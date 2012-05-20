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
#include "cout.h"

#define NO_DEFAULT default: assert(!"reached")

static void convert_splctype(irexp *ir, splctype to, splctype from);
static void irexp_to_c(irexp *ir, splctype how);
static void irstm_to_c(irstm *ir, int prototype);

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
			splctype ltype = C_VOID, rtype = C_VOID, otype;
			// C_VOID means it will inherit the otype.
			switch(ir->binop.op) {
				case EQ:
				case NE:
				case AND:
					otype = C_BOOL;
					ltype = rtype = C_RAW;
					break;
				case OR:
				case XOR:
					otype = C_BOOL;
					break;
				case LT:
				case GT:
				case GE:
				case LE:
					otype = C_BOOL;
					ltype = rtype = C_INT;
					break;
				default:
					otype = C_INT;
			}
			if(otype != how) {
				return convert_splctype(ir, how, otype);
			}
			if(ltype == C_VOID) {
				ltype = otype;
			}
			if(rtype == C_VOID) {
				rtype = otype;
			}
			irexp_to_c(ir->binop.left, ltype);
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
				NO_DEFAULT;
			}
			irexp_to_c(ir->binop.right, rtype);
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
tail_recurse:
	if(prototype && ir->type != SEQ && ir->type != FUNC && ir->type != GINIT) {
		return;
	}
	switch(ir->type) {
		case SEQ:
			irstm_to_c(ir->seq.left, prototype);
			// irstm_to_c(ir->seq.right, prototype);
			ir = ir->seq.right;
			goto tail_recurse;
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
		case EXTFUNC:
			if(!prototype) {
				puts("}\n");
			}
			printf("spltype f%d(", ir->extfunc.funcid);
			if(ir->extfunc.nargs > 0) {
				printf("spltype a0");
				int i;
				for(i = 1; ir->extfunc.nargs > i; i++) {
					printf(", spltype a%d", i);
				}
			}
			if(prototype) {
				puts(");");
			} else {
				puts(") {");
				printf("\t");
				if(ir->extfunc.returntype != C_VOID) {
					printf("return ");
					if(ir->extfunc.returntype != C_UNION) {
						printf("(spltype)");
					}
				}
				printf("%s(", ir->extfunc.name);
				struct splctypelist *args = ir->extfunc.args;
				int n = 0;
				while(args != NULL) {
					switch(args->type) {
						case C_INT:
							printf("a%d.ival", n);
							break;
						case C_BOOL:
							printf("a%d.bval", n);
							break;
						case C_UNION:
							printf("a%d", n);
							break;
						case C_LIST:
							printf("a%d.lval", n);
							break;
						case C_TUPLE:
							printf("a%d.tval", n);
							break;
						NO_DEFAULT;
					}
					n++;
					args = args->next;
				}
				puts(");");
				if(ir->extfunc.returntype == C_VOID) {
					puts("\treturn (spltype)0;");
				}
			}
			break;
		case GINIT:
			if(prototype && ir->nglobals > 0) {
				printf("spltype g0");
				int i;
				for(i = 1; ir->nglobals > i; i++) {
					printf(", g%d", i);
				}
				puts(";");
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
	puts("	int bval;");
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
