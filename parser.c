#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "lazyarray.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"

int parser(lazyarray *, grammar*, synt_tree**, synt_error*);
int ruleparser(lazyarray *gen, grammar *gram, struct attempt *a, synt_error *e);

struct attempt *att_head;

int
main(int argc, char **argv) {
	lazyarray *g  = lazyarray_create(gen_tokens, "test.spl", 0);
	grammar *gram = parse_grammar("grammar.g");

	synt_error *e = create_synt_error();
	synt_tree *t;
	if(!parser(g, gram, &t, e)) {
		return 1;
	}
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
parser(lazyarray *gen, grammar *gram, synt_tree **res_tree, synt_error *e)
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
	a->tree     = &t->fst_child;
	a->branch   = S->branches[0];
	a->parent   = NULL;
	a->parentbranch = NULL;
	a->parenttree = NULL;
	a->next     = NULL;
	a->inputidx = 0;

	att_head = a;

	printf("[att_queue] Queued %p (S)\n", a);

	// if att_head becomes 0, all has failed, return syntax error
	// if an attempt has succeeded parsing all input, return its tree
	while(att_head != NULL) {
		struct attempt *crt = att_head;
		att_head = crt->next;
		crt->next = NULL;
		if(ruleparser(gen, gram, crt, e)) {
			return 1;
		}
	}
	printf("No valid parsing found\n");
	return 0;
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
ruleparser(lazyarray *gen, grammar *gram, struct attempt *a, synt_error *e)
{
	printf("[att_queue] Working on %p\n", a);
	if(*a->tree != NULL) {
		printf("[%p] Cleaning tree\n", a);
		// We moeten deze tree opruimen
		synt_tree *graveyard = NULL, *next;
		do {
			next = (*a->tree)->next;
			show_synt_tree(*a->tree, 2);
			kill_tree(*a->tree, &graveyard);
			*a->tree = next;
		} while(*a->tree != NULL);
		while(graveyard != NULL) {
			next = graveyard->next;
			// printf("Unshifted %s\n", token_to_string(graveyard->token->type));
			// generator_unshift(gen, graveyard->token);
			free(graveyard);
			graveyard = next;
		}
		*a->tree = NULL;
	}
	printf("[%p] Upcoming input: \n", a);
	peek_upcoming_input(gen, a->inputidx, 50);

	while(a->branch != NULL) {
		if(a->branch->is_literal) {
			if(!lazyarray_exists(gen, a->inputidx)) {
				fprintf(stderr, "Hit EOF while expecting %s\n", token_to_string(a->branch->token));
				return 0;
			}
			struct token *t = lazyarray_get(gen, a->inputidx++);
			printf("Shifted %s\n", token_to_string(t->type));
			// fprintf(stderr, "parser(): > expected literal %s, read %s\n", token_to_string(a->branch->token), token_to_string(t->type));
			*a->tree = malloc(sizeof(synt_tree));
			(*a->tree)->type = 0;
			(*a->tree)->token = t;
			(*a->tree)->next = NULL;
			a->tree = &(*a->tree)->next;
			if(a->branch->token != t->type) {
				// TODO better error
				// TODO: unshift generator
				update_synt_error(e, "Expected literal, read other literal", t->lineno, 0);
				a->inputidx--;
				// printf("Unshifted %s\n", token_to_string(t->type));
				// generator_unshift(gen, t);
				return 0;
			}
		} else {
			struct rule *rule = &gram->rules[a->branch->rule];
			int i;
#ifndef NDEBUG
			fprintf(stderr, "parser(): > trying to unify rule %d (%s)\n", a->branch->rule, rule->name);
#endif
			synt_tree *tree = malloc(sizeof(synt_tree));
			tree->type = 1;
			tree->next = NULL;
			tree->fst_child = NULL;
			*a->tree = tree;
			a->tree = &(*a->tree)->next;
			for(i = 0; rule->branches[i] != NULL; ++i) {
				struct attempt *new_attempt = malloc(sizeof(struct attempt));
				new_attempt->tree     = &tree->fst_child;
				new_attempt->branch   = rule->branches[i];
				new_attempt->parent   = a;
				new_attempt->parentbranch = a->branch->next;
				new_attempt->parenttree = a->tree;
				new_attempt->next     = att_head;
				new_attempt->inputidx     = a->inputidx;
				att_head = new_attempt;
				printf("[%p] Queued %p (%s)\n", a, new_attempt, rule->name);
			}
			a->branch = NULL;
			return 0;
		}
		a->branch = a->branch->next;
	}

	printf("[%p] Completed\n", a);

	if(a->branch == NULL) {
		if(a->parent == NULL) {
			// Wij zijn S
			if(lazyarray_exists(gen, a->inputidx)) {
				puts("Complete, but not at EOF");
				return 0;
			}
			puts("Great success");
			return 1;
		}
		a->parent->branch = a->parentbranch;
		a->parent->tree = a->parenttree;
		a->parent->next = att_head;
		a->parent->inputidx = a->inputidx;
		att_head = a->parent;
		printf("[%p] Requeuing parent %p\n", a, a->parent);
		free(a);
	}
	return 0;
}

void
update_synt_error(synt_error *e, const char *error, int row, int col)
{
	// only update if new error is not earlier in the file
	if(row > e->row || (row == e->row && col >= e->col)) {
		if(e->error)
			free(e->error);
		e->error = strdup(error);
		e->row = row;
		e->col = col;
	}
}

synt_error *
create_synt_error()
{
	synt_error *e = calloc(1, sizeof(synt_error));
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
	printf("ptr: %p (type %d)\n", t, t->type);
	if(t->type == 1) {
		synt_tree *el = t->fst_child;
		while(el != NULL) {
			show_synt_tree(el, indent + 1);
			el = el->next;
		}
	} else {
		print_indent(indent);
		printf("Token: %s\n", token_to_string(t->token->type));
	}
}

void
peek_upcoming_input(lazyarray *g, int base, int num) {
	struct token *buf[num];
	int i;
	for(i = 0; num > i && lazyarray_exists(g, base + i); i++) {
		buf[i] = lazyarray_get(g, base + i);
		printf("Token: %s", token_to_string(buf[i]->type));
		switch(buf[i]->type) {
			case T_WORD:
				printf("('%s')\n", buf[i]->value.sval);
				break;
			case T_NUMBER:
				printf("(%d)\n", buf[i]->value.ival);
				break;
			default:
				printf("\n");
		}
	}
	for(i = i-1; 0 <= i; i--) {
		// generator_unshift(g, buf[i]);
	}
}
