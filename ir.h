typedef enum { CONST, NAME, TEMP, BINOP, MEM, CALL, ESEQ, MOVE, EXP, JUMP, CJUMP, SEQ, LABEL, FUNC, RET } irtype;

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
			irlabel label;
			irtemp temp;
			struct {
				char op;
				irexp *left;
				irexp *right;
			} binop;
			irexp *exp;
			struct {
				irexp *func;
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
				char op;
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
			irfunc func;
			irexp *ret;
		};
	};
};

struct irexplist {
	irexp *exp;
	struct irexplist *next;
};
