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
	synt_tree *t;
	parser(g, gram, &t, e);

	printf("Syntax tree:\n");
	show_synt_tree(t, 0);

	free(t);

	return 0;
}

/**
 * Returns the number of possible trees after parsing. 0 means the document
 * could not be parsed.
 * Don't forget to free() the returned trees.
 */
int
parser(generator *gen, grammar *gram, synt_tree **res_tree, synt_error *e)
{
	synt_tree *t = malloc(sizeof(synt_tree));
	*res_tree = t;
	t->rule = get_id_for_rule(gram, "S");
	t->type = 1;
	t->next = NULL;
	t->fst_child = NULL;

	struct rule *S = &gram->rules[ t->rule ];

	// seed the attempts queue
	if(S->branches[1] != NULL) {
		fprintf(stderr, "More than 1 branch in S, this is not supported\n");
		return 0;
	}

	struct attempt *a = malloc(sizeof(struct attempt));
	a->tree     = res_tree;
	a->branch   = S->branches[0];
	a->parent   = NULL;
	a->parentbranch = NULL;
	a->next     = NULL;

	att_head = a;

	// if att_head becomes 0, all has failed, return syntax error
	// if an attempt has succeeded parsing all input, return its tree
	while(att_head != NULL) {
		struct attempt *crt = att_head;
		att_head = crt->next;
		crt->next = NULL;
		ruleparser(gen, gram, crt, e);
	}
	return 1;
}

static void
kill_tree(synt_tree *t, synt_tree **zombies) {
	if(t->type == 0) {
		t->next = *zombies;
		*zombies = t;
		return;
	}
	synt_tree *next, *el = t->fst_child;
	while(el != NULL) {
		next = el->next;
		kill_tree(el, zombies);
		el = next;
	}
	t->next = (void *)0xdeadc0de;
	free(t);
}

/**
 * Try unifying the given rulepart to the next tokens in the generator.
 * Returns 0 if unification failed and updates synt_error.
 * Returns the number of succeeded unifications returned by reference.
 */
int
ruleparser(generator *gen, grammar *gram, struct attempt *a, synt_error *e)
{
	synt_tree **nextleaf = &(*a->tree)->fst_child;
	while(*nextleaf != NULL) {
		nextleaf = &(*nextleaf)->next;
	}

	while(a->branch != NULL) {
		if(a->branch->is_literal) {
			struct token *t = generator_shift(gen);
			printf("Shifted %s\n", token_to_string(t->type));
			// fprintf(stderr, "parser(): > expected literal %s, read %s\n", token_to_string(a->branch->token), token_to_string(t->type));
			if(a->branch->token != t->type) {
				// TODO better error
				// TODO: unshift generator
				update_synt_error(e, "Expected literal, read other literal", t->lineno, 0);
				printf("Unshifted %s\n", token_to_string(t->type));
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
				synt_tree *tree = malloc(sizeof(synt_tree));
				tree->type = 1;
				tree->next = NULL;
				tree->fst_child = NULL;
				*nextleaf = tree;
				struct attempt *new_attempt = malloc(sizeof(struct attempt));
				new_attempt->tree     = nextleaf;
				new_attempt->branch   = rule->branches[i];
				new_attempt->parent   = a;
				new_attempt->parentbranch = a->branch;
				new_attempt->next     = att_head;
				att_head = new_attempt;
			}
			return 1;
		}
		a->branch = a->branch->next;
	}

	if(a->branch == NULL) {
		// XXX we weten niet of *a->tree een tree voor ons is of dat die van attempt 1 was. (Dit is al eerder een probleem.)
		if(*a->tree != NULL) {
			// We moeten deze tree opruimen
			synt_tree *graveyard = NULL, *next;
			kill_tree(*a->tree, &graveyard);
			while(graveyard != NULL) {
				next = graveyard->next;
				printf("Unshifted %s\n", token_to_string(graveyard->token->type));
				generator_unshift(gen, graveyard->token);
				free(graveyard);
				graveyard = next;
			}
		}
		if(a->parent == NULL) {
			// Wij zijn S
			return 1;
		}
		a->parent->branch = a->parentbranch;
		a->parent->next = att_head;
		att_head = a->parent;
		free(a);
	}
	return 0;
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

static void
print_indent(int indent) {
	int i;
	for(i = 0; indent > i; i++) {
		printf("  ");
	}
}

void
show_synt_tree(synt_tree *t, int indent)
{
	print_indent(indent);
	printf("ptr: %p\n", t);
	if(t->type == 1) {
		synt_tree *el = t->fst_child;
		while(el != NULL) {
			show_synt_tree(el, indent + 1);
		}
	} else {
		print_indent(indent);
		printf("Token: %s\n", token_to_string(t->token->type));
	}
}
