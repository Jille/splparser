#ifndef IR_H
#define IR_H

typedef enum { CONST, NAME, TEMP, BINOP, MEM, CALL, ESEQ, MOVE, EXP, JUMP, CJUMP, SEQ, LABEL, FUNC, RET } irtype;
typedef enum { PLUS, MINUS, MUL, DIV, MOD, AND, OR, LSHIFT, RSHIFT, ARSHIFT, XOR, EQ, NE, LT, GT, LE, GE } irop;

struct irunit;
typedef struct irunit irexp;
typedef struct irunit irstm;
typedef int irlabel;
typedef int irfunc;
typedef int irtemp;

struct irunit {
	irtype type;
	union {
		// Expressions
		union {
			int value;
			irlabel name;
			irtemp temp;
			struct {
				irop op;
				irexp *left;
				irexp *right;
			} binop;
			irexp *mem;
			struct {
				irfunc func;
				struct irexplist *args;
			} call;
			struct {
				irstm *stm;
				irexp *exp;
			} eseq;
		};
		// Statements
		union {
			struct {
				irexp *dst;
				irexp *src;
			} move;
			irexp *exp;
			struct {
				irexp *exp;
				// LabelList targets ..?
			} jump;
			struct {
				irop op;
				irexp *left;
				irexp *right;
				irlabel iftrue;
				irlabel iffalse;
			} cjump;
			struct {
				irstm *left;
				irstm *right;
			} seq;
			irlabel label;
			struct {
				irfunc funcid;
				int vars;
			} func;
			irexp *ret;
		};
	};
};

struct irexplist {
	irexp *exp;
	struct irexplist *next;
};

/* ir.c */
void show_ir_tree(struct irunit *ir, int indent);
irlabel getlabel(void);
irfunc getfunc(void);
irtemp gettemp(void);
irstm *mkirseq(irstm *left, irstm *right);
irstm *mkirseq_opt(irstm *left, irstm *right);
irstm *irconcat(irstm *stm, ...);
irstm *mkirmove(irexp *dst, irexp *src);
irexp *mkirtemp(irtemp num);
irexp *mkirconst(int num);
irexp *mkirname(irlabel label);
irexp *mkirbinop(irop binop, irexp *left, irexp *right);
irexp *mkirmem(irexp *a);
irexp *mkircall(irfunc func, struct irexplist *args);
irexp *mkireseq(irstm *stm, irexp *exp);
irstm *mkirexp(irexp *dst);
irstm *mkirjump(irexp *addr);
irstm *mkircjump(irop relop, irexp *left, irexp *right, irlabel t, irlabel f);
irstm *mkirlabel(irlabel label);
irstm *mkirret(irexp *ret);
irstm *mkirfunc(irfunc f, int nvars);

#endif
