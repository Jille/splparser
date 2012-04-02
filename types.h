#define GLOBAL_SYMBOL 0
#define CALL_SYMBOL 1
#define LOCAL_SYMBOL 2

struct type {
	char type; // T_VOID, T_NUMBER, T_BOOL, '[', '(' or T_WORD
	union {
		// if type == '[':
		struct type *list_type; // 0 if list type is unknown
		// if type == '(':
		struct {
			struct type *fst_type;
			struct type *snd_type;
		};
		// if type == T_WORD:
		char *pm_name;
	};
	struct type *next;
};

struct symbol {
	struct type *type;
	char *name;
};

void typechecker(synt_tree *t, grammar *gram);
