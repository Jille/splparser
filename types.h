#define GLOBAL_SYMBOL 0
#define CALL_SYMBOL 1
#define LOCAL_SYMBOL 2

struct type {
	char type; // T_VOID, T_NUMBER, T_BOOL, '[' or '('
	union {
		struct type *list_type;
		struct {
			struct type *fst_type;
			struct type *snd_type;
		};
	};
};

struct symbol {
	struct type *type;
	char *name;
};
