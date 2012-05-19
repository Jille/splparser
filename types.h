#include "ir.h"

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
		struct {
			char *pm_name;
			char accept_empty_list;
		};
	};
	struct type *next;
};

struct symbol {
	struct type *type;
	char *name;
};

struct vardecl {
	char *name;
	struct type *type;
	union {
		irlocal local;
		irglobal global;
	};
	struct vardecl *next;
};
struct func_arg {
	char *name;
	struct type *type;
	irfarg farg;
	struct func_arg *next;
};
struct pm_bind {
	struct type *pm;
	struct type *bound;
	struct pm_bind *next;
};
struct variable {
	char *name;
	struct type *type;
	irtype irtype;
	union {
		irlocal local;
		irglobal global;
		irfarg farg;
	};
};
struct tc_func {
	struct type *returntype;
	char *name;
	struct func_arg *args;
	struct vardecl *decls;
	struct vardecl **decls_last;
	struct type *pm_types;
	irfunc func;
	int nargs;
	int nlocals;
	struct tc_func *next;
};
struct tc_globals {
	grammar *gram;
	int funcall_rule;
	struct type *types;
	struct tc_func *funcs;
	struct tc_func **funcs_last;
	struct vardecl *decls;
	struct vardecl **decls_last;
	int nglobals;

	// Temporary hack
	struct tc_func *curfunc;
};


struct irunit *typechecker(synt_tree *t, grammar *gram);
