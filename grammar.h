#include "tokens.h"

typedef struct _grammar grammar;

struct rule {
	int id;
	char *name;
	struct rulepart *branches[20];
};

struct rulepart {
	int is_literal;
	union {
		int rule;
		int token;
	};
	struct rulepart *next;
};

struct _grammar {
	int lastrule;
	struct rule rules[2000];
};

int
get_id_for_rule(grammar *g, char *rule);

struct rulepart *
parse_branch(grammar *g, char *str);

grammar *
parse_grammar(char *file);

void
show_grammar(grammar *g);
