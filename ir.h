#ifndef IR_H
#define IR_H

typedef enum { CONST, LOCAL, FARG, GLOBAL, BINOP, CALL, ESEQ, MOVE, EXP, JUMP, CJUMP, SEQ, LABEL, FUNC, RET, TRAP, HALT, LISTEL, TUPLE } irtype;
typedef enum { PLUS, MINUS, MUL, DIV, MOD, AND, OR, LSHIFT, RSHIFT, ARSHIFT, XOR, EQ, NE, LT, GT, LE, GE } irop;

struct irunit;
typedef struct irunit irexp;
typedef struct irunit irstm;
typedef int irlabel;
typedef int irfunc;
typedef int irlocal;
typedef int irglobal;
typedef int irfarg;

struct irunit {
	irtype type;
	union {
		// Expressions
		union {
			int value;
			irlabel name;
			irlocal local;
			irfarg farg;
			irglobal global;
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
			struct {
				irexp *exp;
				irexp *next;
			} listel;
			struct {
				irexp *fst;
				irexp *snd;
			} tuple;
		};
		// Statements
		union {
			struct {
				irexp *dst;
				irexp *src;
			} move;
			irexp *exp;
			irlabel jump;
			struct {
				irexp *exp;
				irlabel iffalse;
			} cjump;
			struct {
				irstm *left;
				irstm *right;
			} seq;
			irlabel label;
			struct {
				irfunc funcid;
				int args;
				int vars;
			} func;
			irexp *ret;
			struct {
				int syscall;
				irexp *arg;
			} trap;
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
irstm *mkirseq(irstm *left, irstm *right);
irstm *mkirseq_opt(irstm *left, irstm *right);
irstm *irconcat(irstm *stm, ...);
irstm *mkirmove(irexp *dst, irexp *src);
irexp *mkirlocal(irlocal num);
irexp *mkirglobal(irglobal num);
irexp *mkirfarg(irfarg num);
irexp *mkirconst(int num);
irexp *mkirbinop(irop binop, irexp *left, irexp *right);
irexp *mkircall(irfunc func, struct irexplist *args);
irexp *mkireseq(irstm *stm, irexp *exp);
irexp *mkirlistel(irexp *exp, irexp *next);
irexp *mkirtuple(irexp *fst, irexp *snd);
irstm *mkirexp(irexp *exp);
irstm *mkirjump(irlabel label);
irstm *mkircjump(irexp *exp, irlabel f);
irstm *mkirlabel(irlabel label);
irstm *mkirret(irexp *exp);
irstm *mkirfunc(irfunc f, int nargs, int nvars);
irstm *mkirtrap(int syscall, irexp *arg);
irstm *mkirhalt(void);

#endif
