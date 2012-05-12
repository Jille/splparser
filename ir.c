#define _GNU_SOURCE
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "ir.h"

static irlabel labelctr = 0;
static irfunc funcctr = 0;
static irtemp tempctr = 0;

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
	case ARSHIFT: return shrt ? "<<<": "ARSHIFT";
	case XOR:     return shrt ? "^"  : "XOR";
	case EQ:      return shrt ? "==" : "EQ";
	case NE:      return shrt ? "!=" : "NE";
	case LT:      return shrt ? "<"  : "LT";
	case GT:      return shrt ? ">"  : "GT";
	case LE:      return shrt ? "<=" : "LE";
	case GE:      return shrt ? ">=" : "GE";
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
	char *lbl;
	switch(ir->type) {
	case CONST:
		printf("CONST(%d)", ir->value);
		break;
	case NAME:
		lbl = irlabel_to_string(ir->label);
		printf("NAME(%s)", lbl);
		free(lbl);
		break;
	case TEMP:
		printf("TEMP(%d)", ir->temp);
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
		print_indent(indent);
		printf(")");
		break;
	case MEM:
		printf("MEM(");
		show_ir_tree(ir->mem, indent);
		printf(")");
		break;
	case CALL:
		printf("CALL(\n");
		++indent;
		print_indent(indent);
		printf("FUNC(%d), \n", ir->call.func);
		print_indent(indent);
		printf("ExpList(");
		struct irexplist *args = ir->call.args;
		if(args != NULL) {
			printf("\n");
			print_indent(indent);
		}
		while(args != NULL) {
			show_ir_tree(args->exp, indent);
			printf("\n");
			print_indent(indent);
			args = args->next;
		}
		printf(")\n");
		print_indent(indent - 1);
		printf(")");
		break;
	case ESEQ:
		printf("ESEQ(todo)");
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
		printf("JUMP(todo)");
		break;
	case CJUMP:
		printf("CJUMP(todo)");
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
		printf("FUNC(%d)", ir->func);
		break;
	case RET:
		printf("RET(");
		show_ir_tree(ir->ret, indent);
		printf(")");
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

irtemp
gettemp() {
	return ++tempctr;
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
mkirtemp(irtemp num) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = TEMP;
	ret->temp = num;
	return ret;
}

irexp *
mkirconst(int num) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = CONST;
	ret->value = num;
	return ret;
}

irexp *mkirname(irlabel label) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = NAME;
	ret->label = label;
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

irexp *mkirmem(irexp *a) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = MEM;
	ret->mem = a;
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
irstm *mkirexp(irexp *exp) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = EXP;
	ret->exp = exp;
	return ret;
}
irstm *mkirjump(irexp *addr /*, labellist targets */) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = JUMP;
	ret->exp = addr;
	return ret;
}
irstm *mkircjump(irop relop, irexp *left, irexp *right, irlabel t, irlabel f) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = CJUMP;
	ret->cjump.op = relop;
	ret->cjump.left = left;
	ret->cjump.right = right;
	ret->cjump.iftrue = t;
	ret->cjump.iffalse = f;
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
irstm *mkirfunc(irfunc f) {
	irexp *ret = malloc(sizeof(struct irunit));
	ret->type = FUNC;
	ret->func = f;
	return ret;
}
