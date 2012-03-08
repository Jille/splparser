#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"

int parser(generator*, grammar*, synt_tree**, synt_error*);
int ruleparser(generator *gen, grammar *gram, struct attempt *a, synt_error *e);

struct attempt *att_head;

int
main(int argc, char **argv) {
	generator *g  = generator_create(gen_tokens, "test.spl", 0);
	grammar *gram = parse_grammar("grammar.g");

	synt_error *e = create_synt_error();
	synt_tree  *t;
	int trees     = parser(g, gram, &t, e);

	if(trees == 0) {
		fprintf(stderr, "Unification failed on line %d, character %d: %s\n", e->row, e->col, e->error);
		return 1;
	}

	printf("Syntax tree:\n");
	show_synt_tree(t);

	free(t);

	return 0;
}

/**
 * Returns the number of possible trees after parsing. 0 means the document
 * could not be parsed.
 * Don't forget to free() the returned trees.
 */
int
parser(generator *gen, grammar *gram, synt_tree **trees, synt_error *e)
{
	struct rule *S = &gram->rules[ get_id_for_rule(gram, "S") ];

	// seed the attempts queue
	if(S->branches[1] != NULL) {
		fprintf(stderr, "More than 1 branch in S, this is illegal\n");
		return 0;
	}

	struct attempt *a = malloc(sizeof(struct attempt));
	*trees      = malloc(sizeof(synt_tree));
	a->tree     = trees;
	a->branch   = S->branches[0];
	a->parent   = NULL;
	a->parentbranch = NULL;
	a->next     = NULL;

	att_head = a;

	// if att_head becomes 0, all has failed, return syntax error
	// if an attempt has succeeded parsing all input, return its tree
	while(att_head != NULL) {
		if(att_head->parent == NULL && att_head->branch == NULL && generator_eof(gen)) {
			// we're done here.
			return 1;
		}
		struct attempt *crt = att_head;
		att_head = crt->next;
		crt->next = NULL;
		ruleparser(gen, gram, crt, e);
	}
	return 1;
}

/**
 * Try unifying the given rulepart to the next tokens in the generator.
 * Returns 0 if unification failed and updates synt_error.
 * Returns the number of succeeded unifications returned by reference.
 */
int
ruleparser(generator *gen, grammar *gram, struct attempt *a, synt_error *e)
{
	synt_tree *tree = malloc(sizeof(synt_tree));
	synt_tree **nextleaf = &tree->fst_child;
	assert(a->branch != NULL);

	do {
		if(a->branch->is_literal) {
			struct token *t = generator_shift(gen);
#ifndef NDEBUG
			fprintf(stderr, "parser(): > expected literal %s, read %s\n", token_to_string(a->branch->token), token_to_string(t->type));
#endif
			if(a->branch->token != t->type) {
				// TODO better error
				// TODO: unshift generator
				update_synt_error(e, "Expected literal, read other literal", t->lineno, 0);
				generator_unshift(gen, t);
				free(a);
				return 0;
			}
			*nextleaf = malloc(sizeof(synt_tree));
			(*nextleaf)->type = 0;
			(*nextleaf)->token = t;
			nextleaf = &(*nextleaf)->next;
		} else {
			struct rule *rule = &gram->rules[a->branch->rule];
			int i;
#ifndef NDEBUG
			fprintf(stderr, "parser(): > trying to unify rule %d (%s)\n", a->branch->rule, rule->name);
#endif
			for(i = 0; rule->branches[i] != NULL; ++i) {
				struct attempt *new_attempt = malloc(sizeof(struct attempt));
				new_attempt->tree     = nextleaf;
				new_attempt->branch   = rule->branches[i];
				new_attempt->parent   = a;
				new_attempt->parentbranch = a->branch;
				new_attempt->next     = att_head;
				att_head = new_attempt;
			}
			return;
		}
		a->branch = a->branch->next;
	} while(a->branch != NULL);

	if(a->branch == NULL) {
		*a->tree = tree;
		if(a->parent == NULL) {
			// Wij zijn S
			return;
		}
		a->parent->branch = a->parentbranch;
		a->parent->next = att_head;
		att_head = a->parent;
		free(a);
	}
}

/*
while(!generator_eof(g)) {
	struct token *t = generator_shift(g);
	printf("Token: %s\n", token_to_string(t->type));
		switch(t->type) {
		case T_WORD:
			printf("Value: '%s'\n", t->value.sval);
			free(t->value.sval);
			break;
		case T_NUMBER:
			printf("Value: %d\n", t->value.ival);
			break;
	}
	free(t);
}

}
*/

void
update_synt_error(synt_error *e, const char *error, int row, int col)
{
	// only update if new error is not earlier in the file
	if(row > e->row || (row == e->row && col >= e->col)) {
		free(e->error);
		e->error = strdup(error);
		e->row = row;
		e->col = col;
	}
}

synt_error*
create_synt_error()
{
	synt_error *e = malloc(sizeof(synt_error));
	e->error = 0;
	e->row = 0;
	e->col = 0;
	return e;
}

void
show_synt_tree(synt_tree *t)
{
	// TODO
}
