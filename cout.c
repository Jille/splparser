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

static void convert_splctype(irexp *ir, splctype to, splctype from, FILE *fd);
static void irexp_to_c(irexp *ir, splctype how, FILE *fd);
static void irstm_to_c(irstm *ir, int prototype, int indent, FILE *fd);

static int
puts_fd(const char *s, FILE *fd) {
	fputs(s, fd);
	return fputs("\n", fd); // <-- WHY OH WHY, fputs?
}

static void
convert_splctype(irexp *ir, splctype to, splctype from, FILE *fd) {
	assert(to != from);
	if(from == C_UNION) {
		irexp_to_c(ir, from, fd);
		switch(to) {
			case C_INT:
				fprintf(fd, ".ival");
				return;
			case C_BOOL:
				fprintf(fd, ".bval");
				return;
			case C_LIST:
				fprintf(fd, ".lval");
				return;
			case C_TUPLE:
				fprintf(fd, ".uval");
				return;
			case C_RAW:
				return;
			NO_DEFAULT;
		}
	}
	switch(to) {
		case C_RAW:
			return irexp_to_c(ir, from, fd);
		case C_UNION:
			fprintf(fd, "(spltype)(");
			irexp_to_c(ir, from, fd);
			fprintf(fd, ")");
			break;
		NO_DEFAULT;
	}
}

static void
irexp_to_c(irexp *ir, splctype how, FILE *fd) {
	switch(ir->type) {
		case CONST:
			if(how == C_LIST && ir->value == 0) {
				fprintf(fd, "NULL");
				return;
			}
			if(how != C_INT && how != C_BOOL) {
				convert_splctype(ir, how, C_INT, fd);
				return;
			}
			fprintf(fd, "%d", ir->value);
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
				return convert_splctype(ir, how, otype, fd);
			}
			if(ltype == C_VOID) {
				ltype = otype;
			}
			if(rtype == C_VOID) {
				rtype = otype;
			}
			irexp_to_c(ir->binop.left, ltype, fd);
			switch(ir->binop.op) {
#define CONVERT_BINOP(x, y)	case x: fprintf(fd, #y); break
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
			irexp_to_c(ir->binop.right, rtype, fd);
			break;
		case LOCAL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "l%d", ir->local);
			break;
		case GLOBAL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "g%d", ir->global);
			break;
		case FARG:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "a%d", ir->farg);
			break;
		case CALL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "f%d(", ir->call.func);
			struct irexplist *args = ir->call.args;
			while(args != NULL) {
				irexp_to_c(args->exp, C_UNION, fd);
				args = args->next;
				if(args == NULL) {
					break;
				}
				fprintf(fd, ", ");
			}
			fprintf(fd, ")");
			break;
		case LISTEL:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "create_listel(");
			irexp_to_c(ir->listel.exp, C_UNION, fd);
			fprintf(fd, ", ");
			irexp_to_c(ir->listel.next, C_LIST, fd);
			fprintf(fd, ")");
			break;
		case TUPLE:
			if(how != C_UNION) {
				return convert_splctype(ir, how, C_UNION, fd);
			}
			fprintf(fd, "create_tuple(");
			irexp_to_c(ir->tuple.fst, C_UNION, fd);
			fprintf(fd, ", ");
			irexp_to_c(ir->tuple.fst, C_UNION, fd);
			fprintf(fd, ")");
			break;
		NO_DEFAULT;
	}
}

void
irstm_to_c(irstm *ir, int prototype, int indent, FILE *fd) {
tail_recurse:
	if(prototype && ir->type != SEQ && ir->type != FUNC && ir->type != EXTFUNC && ir->type != GINIT) {
		return;
	}
	switch(ir->type) {
		case SEQ:
			irstm_to_c(ir->seq.left, prototype, indent, fd);
			// irstm_to_c(ir->seq.right, prototype, indent);
			ir = ir->seq.right;
			goto tail_recurse;
			break;
		case MOVE:
			print_indent_fd(fd, indent);
			irexp_to_c(ir->move.dst, C_RAW, fd);
			fprintf(fd, " = ");
			irexp_to_c(ir->move.src, C_UNION, fd);
			puts_fd(";", fd);
			break;
		case EXP:
			print_indent_fd(fd, indent);
			irexp_to_c(ir->exp, C_RAW, fd);
			puts_fd(";", fd);
			break;
		case JUMP:
			print_indent_fd(fd, indent);
			fprintf(fd, "goto lbl%04d;\n", get_ssmlabel_from_irlabel(ir->jump));
			break;
		case CJUMP: {
			irstm *tbody = ir->cjump.iftrue, *fbody = ir->cjump.iffalse;
			print_indent_fd(fd, indent);
			if(tbody == NULL && fbody == NULL) {
				return irexp_to_c(ir->cjump.cond, C_RAW, fd);
			}
			fprintf(fd, "if(");
			if(tbody == NULL) {
				fprintf(fd, "!");
				tbody = fbody;
				fbody = NULL;
			}
			irexp_to_c(ir->cjump.cond, C_BOOL, fd);
			puts_fd(") {", fd);
			irstm_to_c(tbody, prototype, indent+1, fd);
			if(fbody != NULL) {
				print_indent_fd(fd, indent);
				puts_fd("} else {", fd);
				irstm_to_c(fbody, prototype, indent+1, fd);
			}
			print_indent_fd(fd, indent);
			puts_fd("}", fd);
			break;
		}
		case LABEL:
			fprintf(fd, "lbl%04d: \n", get_ssmlabel_from_irlabel(ir->label));
			break;
		case RET:
			print_indent_fd(fd, indent);
			fprintf(fd, "return ");
			irexp_to_c(ir->ret, C_UNION, fd);
			puts_fd(";", fd);
			break;
		case TRAP:
			print_indent_fd(fd, indent);
			switch(ir->trap.syscall) {
				case 0:
					fprintf(fd, "printf(\"%%d\\n\", ");
					irexp_to_c(ir->trap.arg, C_INT, fd);
					puts_fd(");", fd);
					break;
				NO_DEFAULT;
			}
			break;
		case HALT:
			print_indent_fd(fd, indent);
			puts_fd("exit(0);", fd);
			break;
		case FUNC:
			if(!prototype) {
				print_indent_fd(fd, indent-1);
				puts_fd("}\n", fd);
			}
			print_indent_fd(fd, indent-1);
			fprintf(fd, "spltype f%d(", ir->func.funcid);
			if(ir->func.args > 0) {
				fprintf(fd, "spltype a0");
				int i;
				for(i = 1; ir->func.args > i; i++) {
					fprintf(fd, ", spltype a%d", i);
				}
			}
			if(prototype) {
				puts_fd(");", fd);
			} else {
				puts_fd(") {", fd);
				if(ir->func.vars > 0) {
					print_indent_fd(fd, indent);
					fprintf(fd, "spltype l0");
					int i;
					for(i = 1; ir->func.vars > i; i++) {
						fprintf(fd, ", l%d", i);
					}
					puts_fd(";", fd);
				}
			}
			break;
		case EXTFUNC:
			if(!prototype) {
				print_indent_fd(fd, indent-1);
				puts_fd("}\n", fd);
			}
			print_indent_fd(fd, indent-1);
			fprintf(fd, "spltype f%d(", ir->extfunc.funcid);
			if(ir->extfunc.nargs > 0) {
				fprintf(fd, "spltype a0");
				int i;
				for(i = 1; ir->extfunc.nargs > i; i++) {
					fprintf(fd, ", spltype a%d", i);
				}
			}
			if(prototype) {
				puts_fd(");", fd);
			} else {
				puts_fd(") {", fd);
				print_indent_fd(fd, indent);
				if(ir->extfunc.returntype != C_VOID) {
					fprintf(fd, "return ");
					if(ir->extfunc.returntype != C_UNION) {
						fprintf(fd, "(spltype)");
					}
				}
				fprintf(fd, "%s(", ir->extfunc.name);
				struct splctypelist *args = ir->extfunc.args;
				int n = 0;
				while(args != NULL) {
					switch(args->type) {
						case C_INT:
							fprintf(fd, "a%d.ival", n);
							break;
						case C_BOOL:
							fprintf(fd, "a%d.bval", n);
							break;
						case C_UNION:
							fprintf(fd, "a%d", n);
							break;
						case C_LIST:
							fprintf(fd, "a%d.lval", n);
							break;
						case C_TUPLE:
							fprintf(fd, "a%d.tval", n);
							break;
						NO_DEFAULT;
					}
					n++;
					args = args->next;
				}
				puts_fd(");", fd);
				if(ir->extfunc.returntype == C_VOID) {
					print_indent_fd(fd, indent);
					puts_fd("return (spltype)0;", fd);
				}
			}
			break;
		case GINIT:
			if(prototype && ir->nglobals > 0) {
				print_indent_fd(fd, indent);
				fprintf(fd, "spltype g0");
				int i;
				for(i = 1; ir->nglobals > i; i++) {
					fprintf(fd, ", g%d", i);
				}
				puts_fd(";", fd);
			}
			break;
		NO_DEFAULT;
	}
}

void
ir_to_c(irstm *ir, FILE *fd) {
	puts_fd("#include <stdio.h>", fd);
	puts_fd("#include <stdlib.h>", fd);
	puts_fd("#include <unistd.h>", fd);
	puts_fd("struct _spllist;", fd);
	puts_fd("struct _spltuple;", fd);
	puts_fd("typedef union _spltype {", fd);
	puts_fd("	int ival;", fd);
	puts_fd("	int bval;", fd);
	puts_fd("	struct _spllist *lval;", fd);
	puts_fd("	struct _spltuple *tval;", fd);
	puts_fd("} spltype;", fd);
	puts_fd("typedef struct _spllist {", fd);
	puts_fd("	spltype value;", fd);
	puts_fd("	struct _spllist *next;", fd);
	puts_fd("} spllist;", fd);
	puts_fd("typedef struct _spltuple {", fd);
	puts_fd("	spltype fst;", fd);
	puts_fd("	spltype snd;", fd);
	puts_fd("} spltuple;", fd);
	puts_fd("", fd);
	irstm_to_c(ir, 1, 0, fd);
	puts_fd("", fd);
	puts_fd("spltype create_listel(spltype el, spllist *list) {", fd);
	puts_fd("	spltype ret;", fd);
	puts_fd("	ret.lval = malloc(sizeof(spllist));", fd);
	puts_fd("	ret.lval->value = el;", fd);
	puts_fd("	ret.lval->next = list;", fd);
	puts_fd("	return ret;", fd);
	puts_fd("}", fd);
	puts_fd("spltype create_tuple(spltype fst, spltype snd) {", fd);
	puts_fd("	spltype ret;", fd);
	puts_fd("	ret.tval = malloc(sizeof(spltuple));", fd);
	puts_fd("	ret.tval->fst = fst;", fd);
	puts_fd("	ret.tval->snd = snd;", fd);
	puts_fd("	return ret;", fd);
	puts_fd("}", fd);
	puts_fd("", fd);
	c_builtin_functions(fd);
	puts_fd("", fd);
	puts_fd("int main(int argc, char **argv) {", fd);
	irstm_to_c(ir, 0, 1, fd);
	puts_fd("}", fd);
}
