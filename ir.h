typedef enum { CONST, NAME, TEMP, BINOP, MEM, CALL, ESEQ, MOVE, EXP, JUMP, CJUMP, SEQ, LABEL, FUNC, RET } irtype;
typedef enum { PLUS, MINUS, MUL, DIV, AND, OR, LSHIFT, RSHIFT, ARSHIFT, XOR, EQ, NE, LT, GT, LE, GE } irop;

struct irunit;
typedef struct irunit irexp;
typedef struct irunit irstm;
typedef int irlabel;
typedef int irfunc;
typedef int irtemp;

void show_ir_tree(struct irunit *ir, int indent);

struct irunit {
	irtype type;
	union {
		// Expressions
		union {
			int value;
			irlabel label;
			irtemp temp;
			struct {
				irop op;
				irexp *left;
				irexp *right;
			} binop;
			irexp *mem;
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
			irfunc func;
			irexp *ret;
		};
	};
};

struct irexplist {
	irexp *exp;
	struct irexplist *next;
};
