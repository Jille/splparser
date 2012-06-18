#define _GNU_SOURCE
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "ir.h"

static irlabel labelctr = 0;
static irfunc funcctr = 0;

static const char *
irop_to_string(irop op, int shrt)
{
	switch(op) {
	case PLUS:    return shrt ? "+"  : "PLUS";
	case MINUS:   return shrt ? "-"  : "MINUS";
	case MUL:     return shrt ? "*"  : "MUL";
	case DIV:     return shrt ? "/"  : "DIV";
	case AND:     return shrt ? "&&" : "AND";
	case OR:      return shrt ? "||" : "OR";
	case LSHIFT:  return shrt ? "<<" : "LSHIFT";
	case RSHIFT:  return shrt ? ">>" : "RSHIFT";
	case XOR:     return shrt ? "^"  : "XOR";
	case EQ:      return shrt ? "==" : "EQ";
	case NE:      return shrt ? "!=" : "NE";
	case LT:      return shrt ? "<"  : "LT";
	case GT:      return shrt ? ">"  : "GT";
	case LE:      return shrt ? "<=" : "LE";
	case GE:      return shrt ? ">=" : "GE";
	case MOD:     return shrt ? "%"  : "MOD";
	default:      return "???";
	}
}

static char *
irlabel_to_string(irlabel label) {
	char *res;
	asprintf(&res, "LABEL(%d)", label);
	return res;
}

void
show_ir_tree(struct irunit *ir, int indent) {
	assert(ir != NULL);
	char *lbl;
	switch(ir->type) {
	case CONST:
		printf("CONST(%d)", ir->value);
		break;
	case LOCAL:
		printf("LOCAL(%d)", ir->local);
		break;
	case GLOBAL:
		printf("GLOBAL(%d)", ir->global);
		break;
	case FARG:
		printf("FARG(%d)", ir->farg);
		break;
	case BINOP:
		printf("BINOP(\n");
		++indent;
		print_indent(indent);
		show_ir_tree(ir->binop.left, indent);
		printf("\n");
		print_indent(indent);
		printf("%s\n", irop_to_string(ir->binop.op, 1));
		print_indent(indent);
		show_ir_tree(ir->binop.right, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case CALL:
		printf("CALL(\n");
		++indent;
		print_indent(indent);
		printf("FUNC(%d), \n", ir->call.func);
		print_indent(indent);
		printf("ExpList(");
		++indent;
		struct irexplist *args = ir->call.args;
		int has_args = args != NULL;
		if(has_args) {
			printf("\n");
		}
		while(args != NULL) {
			print_indent(indent);
			show_ir_tree(args->exp, indent);
			printf("\n");
			args = args->next;
		}
		--indent;
		if(has_args) {
			print_indent(indent);
		}
		printf(")\n");
		print_indent(indent - 1);
		printf(")");
		break;
	case ESEQ:
		printf("ESEQ(todo)");
		break;
	case LISTEL:
		puts("LISTEL(");
		indent++;
		print_indent(indent);
		show_ir_tree(ir->listel.exp, indent);
		printf(",\n");
		print_indent(indent);
		show_ir_tree(ir->listel.next, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case TUPLE:
		puts("TUPLE(");
		indent++;
		print_indent(indent);
		show_ir_tree(ir->tuple.fst, indent);
		printf(",\n");
		print_indent(indent);
		show_ir_tree(ir->tuple.snd, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case MOVE:
		puts("MOVE(");
		indent++;
		print_indent(indent);
		show_ir_tree(ir->move.dst, indent);
		printf(",\n");
		print_indent(indent);
		show_ir_tree(ir->move.src, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case EXP:
		printf("EXP(");
		show_ir_tree(ir->exp, indent);
		printf(")");
		break;
	case JUMP:
		lbl = irlabel_to_string(ir->name);
		printf("JUMP(%s)", lbl);
		free(lbl);
		break;
	case CJUMP:
		printf("CJUMP(\n");
		++indent;
		print_indent(indent);
		show_ir_tree(ir->cjump.cond, indent);
		printf(",\n");
		print_indent(indent);
		puts("then:");
		if(ir->cjump.iftrue != NULL) {
			++indent;
			print_indent(indent);
			show_ir_tree(ir->cjump.iftrue, indent);
			--indent;
			puts("");
		}
		print_indent(indent);
		puts("else:");
		if(ir->cjump.iffalse != NULL) {
			++indent;
			print_indent(indent);
			show_ir_tree(ir->cjump.iffalse, indent);
			--indent;
			puts("");
		}
		print_indent(indent);
		printf(")");
		break;
	case SEQ:
		printf("SEQ(\n");
		indent++;
		print_indent(indent);
		show_ir_tree(ir->seq.left, indent);
		printf(",\n");
		print_indent(indent);
		show_ir_tree(ir->seq.right, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case LABEL:
		lbl = irlabel_to_string(ir->label);
		printf("LABEL(%s)", lbl);
		free(lbl);
		break;
	case FUNC:
		printf("FUNC(%d, %d, %d)", ir->func.funcid, ir->func.args, ir->func.vars);
		break;
	case EXTFUNC:
		printf("EXTFUNC(%d, %s)", ir->extfunc.funcid, ir->extfunc.name);
		break;
	case RET:
		printf("RET(");
		show_ir_tree(ir->ret, indent);
		printf(")");
		break;
	case TRAP:
		printf("TRAP(%d, \n", ir->trap.syscall);
		++indent;
		print_indent(indent);
		show_ir_tree(ir->trap.arg, indent);
		printf("\n");
		indent--;
		print_indent(indent);
		printf(")");
		break;
	case SOFTHALT:
		printf("SOFTHALT");
		break;
	case HALT:
		printf("HALT");
		break;
	case GINIT:
		printf("GINIT(%d)", ir->nglobals);
		break;
	case SPAWN:
		printf("SPAWN(%d)", ir->threadfunc);
		break;
	case YIELD:
		printf("YIELD");
		break;
	default:
		assert(0 && "Unknown irtype");
	}
}

irlabel
getlabel() {
	return ++labelctr;
}

irfunc
getfunc() {
	return ++funcctr;
}

irstm *
mkirseq(irstm *left, irstm *right) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = SEQ;
	ret->seq.left = left;
	ret->seq.right = right;
	return ret;
}

irstm *
mkirseq_opt(irstm *left, irstm *right) {
	if(left == NULL) {
		return right;
	} else if(right == NULL) {
		return left;
	}
	return mkirseq(left, right);
}

irstm *
irconcat(irstm *stm, ...) {
	va_list ap;
	va_start(ap, stm);
	while(1) {
		irstm *next = va_arg(ap, irstm *);
		if(next == NULL) {
			break;
		}
		stm = mkirseq(stm, next);
	}
	va_end(ap);
	return stm;
}

irstm *
mkirmove(irexp *dst, irexp *src) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = MOVE;
	ret->move.dst = dst;
	ret->move.src = src;
	return ret;
}

irexp *
mkirlocal(irlocal num) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = LOCAL;
	ret->local = num;
	return ret;
}

irexp *
mkirglobal(irglobal num) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = GLOBAL;
	ret->global = num;
	return ret;
}

irexp *
mkirfarg(irfarg num) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = FARG;
	ret->farg = num;
	return ret;
}

irexp *
mkirconst(int num) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = CONST;
	ret->value = num;
	return ret;
}

irexp *mkirbinop(irop binop, irexp *left, irexp *right) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = BINOP;
	ret->binop.op = binop;
	ret->binop.left = left;
	ret->binop.right = right;
	return ret;
}

irexp *mkircall(irfunc func, struct irexplist *args) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = CALL;
	ret->call.func = func;
	ret->call.args = args;
	return ret;
}
irexp *mkireseq(irstm *stm, irexp *exp) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = ESEQ;
	ret->eseq.stm = stm;
	ret->eseq.exp = exp;
	return ret;
}
irexp *mkirlistel(irexp *exp, irexp *next) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = LISTEL;
	ret->listel.exp = exp;
	ret->listel.next = next;
	return ret;
}
irexp *mkirtuple(irexp *fst, irexp *snd) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = TUPLE;
	ret->tuple.fst = fst;
	ret->tuple.snd = snd;
	return ret;
}

irstm *mkirexp(irexp *exp) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = EXP;
	ret->exp = exp;
	return ret;
}
irstm *mkirjump(irlabel label) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = JUMP;
	ret->jump = label;
	return ret;
}
irstm *mkircjump(irexp *cond, irstm *iftrue, irstm *iffalse) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = CJUMP;
	ret->cjump.cond = cond;
	ret->cjump.iffalse = iffalse;
	ret->cjump.iftrue = iftrue;
	return ret;
}
irstm *mkirlabel(irlabel label) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = LABEL;
	ret->label = label;
	return ret;
}
irstm *mkirret(irexp *exp) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = RET;
	ret->ret = exp;
	return ret;
}
irstm *mkirfunc(irfunc f, int nargs, int nvars) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = FUNC;
	ret->func.funcid = f;
	ret->func.args = nargs;
	ret->func.vars = nvars;
	return ret;
}
irstm *mkirextfunc(irfunc f, char *name, splctype returntype, int nargs, struct splctypelist *args) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = EXTFUNC;
	ret->extfunc.funcid = f;
	ret->extfunc.name = name;
	ret->extfunc.returntype = returntype;
	ret->extfunc.nargs = nargs;
	ret->extfunc.args = args;
	return ret;
}
irstm *mkirtrap(int syscall, irexp *arg) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = TRAP;
	ret->trap.syscall = syscall;
	ret->trap.arg = arg;
	return ret;
}
irstm *mkirsofthalt() {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = SOFTHALT;
	return ret;
}
irstm *mkirhalt() {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = HALT;
	return ret;
}
irstm *mkirginit(int globals) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = GINIT;
	ret->nglobals = globals;
	return ret;
}
irstm *mkirspawn(irfunc func) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = SPAWN;
	ret->threadfunc = func;
	return ret;
}
irstm *mkiryield() {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = YIELD;
	return ret;
}
