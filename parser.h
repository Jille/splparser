#include "tokens.h"
#include "grammar.h"

typedef struct _synt_error synt_error;
typedef struct _synt_tree  synt_tree;

struct _synt_error {
	char *error;
	int row;
	int col;
};

struct _synt_tree {
	int rule;
	char type; // 0 => LITERAL or 1 => NODE
	synt_tree *next;
	union {
		struct token *token;
		synt_tree *fst_child;
	};
};

struct attempt {
	synt_tree **tree;
	struct rulepart *branch;
	struct attempt *parent;
	struct rulepart *parentbranch;
	synt_tree **parenttree;
	struct attempt *next;
	int inputidx;
};

struct pretty_print_state {
	int indent;
	int had_text;
};

synt_error *create_synt_error();

void update_synt_error(synt_error*, const char *error, int row, int col);

void print_indent(int indent);
void show_synt_tree(synt_tree*, int indent, grammar *gram);
void pretty_print(synt_tree *t, struct pretty_print_state *state, grammar *gram);
