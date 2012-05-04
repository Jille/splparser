typedef enum { CONST, NAME, TEMP, BINOP, MEM, CALL, ESEQ, MOVE, EXP, JUMP, CJUMP, SEQ, LABEL } irtype;

struct irunit;
typedef struct irunit irexp;
typedef struct irunit irstm;
typedef int irlabel;

struct irunit {
	irtype type;
	union {
		union {
			int value;
			irlabel *label;
			int temp;
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
				irlabel *iftrue;
				irlabel *iffalse;
			} cjump;
			struct {
				irstm *left;
				irstm *right;
			} seq;
			irlabel *label;
		};
	};
};

struct irexplist {
	struct irexp *exp;
	struct irexplist *next;
};
