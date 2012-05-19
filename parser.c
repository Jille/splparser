#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lazyarray.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"
#include "types.h"
#include "ir.h"
#include "ssm.h"
#include "separate.h"

// #define VERBOSE_PARSER_DEBUG

static synt_tree *parse_input_files(grammar *gram, char **args);
int parser(lazyarray *, grammar*, synt_tree**, synt_error*);
int ruleparser(lazyarray *gen, grammar *gram, struct attempt *a, synt_error *e);

struct attempt *att_head;

static int usestdlib = 1;
static int parseonly = 0;

int
main(int argc, char **argv) {
	int opt;
	char *assembly = (void *)-1;
	char *ccode = (void *)-1;
	char *gramfile = "grammar.g";

	while((opt = getopt(argc, argv, "pcs::g:L")) != -1) {
		switch(opt) {
			case 'p':
				parseonly = 1;
				break;
			case 's':
				assembly = optarg;
				break;
			case 'c':
				// XXX file laten accepteren zodra ir_to_c() dat kan
				ccode = NULL;
				break;
			case 'g':
				gramfile = optarg;
				break;
			case 'L':
				usestdlib = 0;
				break;
			default:
				fprintf(stderr, "Usage: %s [-pL] [-s [out.ssm]] [-c] [-g grammar.g]\n", argv[0]);
				return 1;
		}
	}

	// XXX load the grammar in another thread
	grammar *gram = parse_grammar(gramfile);
	synt_tree *t = parse_input_files(gram, argv + optind);

	if(usestdlib) {
		synt_tree *stdlib = read_synt_tree("stdlib.ast");
		t = merge_synt_trees(gram, stdlib, t);
	}
	printf("Syntax tree:\n");
	show_synt_tree(t, 0, gram);

	if(parseonly) {
		return 0;
	}

	struct irunit *ir = typechecker(t, gram);
	show_ir_tree(ir, 0);
	printf("\n");

	if(assembly != (void *)-1) {
		struct ssmline *ssm = ir_to_ssm(ir);
		if(assembly == NULL) {
			show_ssm(ssm);
		} else {
			FILE *fh = fopen(assembly, "w");
			write_ssm(ssm, fh);
			fclose(fh);
		}
	}

	if(ccode != (void *)-1) {
		// XXX support voor writen naar een file
		ir_to_c(ir);
	}

	free(t);
	return 0;
}

static synt_tree *
parse_input_files(grammar *gram, char **args) {
	char *def[] = { "test.spl" };
	synt_tree *master = NULL;
	if(*args == NULL) {
		args = def;
	}
	while(*args != NULL) {
		synt_tree *t;
		if(strstr(*args, ".ast") != NULL) {
			t = read_synt_tree(*args);
		} else {
			lazyarray *g  = lazyarray_create(gen_tokens, *args, 1);
			synt_error *e = create_synt_error();
			if(!parser(g, gram, &t, e)) {
				printf("Failed to parse %s, error on line %d: %s\n", *args, e->row, e->error);
				exit(1);
			}
			lazyarray_destroy(g);
			if(parseonly) {
				char *astfile, *tmp;
				asprintf(&astfile, "%s.ast", *args);
				if((tmp = strstr(astfile, ".spl")) != NULL) {
					strcpy(tmp, ".ast");
				}
				write_synt_tree(astfile, t);
			}
			if(strcmp(*args, "stdlib.spl") == 0) {
				usestdlib = 0;
			}
		}
		t->next = master;
		master = t;
		args++;
	}
	while(master->next != NULL) {
		synt_tree *next = master->next;
		master->next = NULL;
		master = merge_synt_trees(gram, next, master);
	}
	return master;
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

#ifdef VERBOSE_PARSER_DEBUG
	printf("[att_queue] Queued %p (S)\n", a);
#endif

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
#ifdef VERBOSE_PARSER_DEBUG
	printf("No valid parsing found\n");
#endif
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
#ifdef VERBOSE_PARSER_DEBUG
	printf("[att_queue] Working on %p\n", a);
#endif
	if(*a->tree != NULL) {
#ifdef VERBOSE_PARSER_DEBUG
		printf("[%p] Cleaning tree\n", a);
#endif
		// We moeten deze tree opruimen
		synt_tree *graveyard = NULL, *next;
		do {
			next = (*a->tree)->next;
#ifdef VERBOSE_PARSER_DEBUG
			show_synt_tree(*a->tree, 2, gram);
#endif
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
#ifdef VERBOSE_PARSER_DEBUG
	printf("[%p] Upcoming input: \n", a);
	peek_upcoming_input(gen, a->inputidx, 50);
#endif

	while(a->branch != NULL) {
		if(a->branch->is_literal) {
			if(!lazyarray_exists(gen, a->inputidx)) {
#ifdef VERBOSE_PARSER_DEBUG
				fprintf(stderr, "Hit EOF while expecting %s\n", token_to_string(a->branch->token));
#endif
				return 0;
			}
			struct token *t = lazyarray_get(gen, a->inputidx++);
#ifdef VERBOSE_PARSER_DEBUG
			printf("Shifted %s\n", token_to_string(t->type));
#endif
			*a->tree = malloc(sizeof(synt_tree));
			(*a->tree)->type = 0;
			(*a->tree)->token = t;
			(*a->tree)->next = NULL;
			a->tree = &(*a->tree)->next;
			if(a->branch->token != t->type) {
				char *buf;
				asprintf(&buf, "parser(%d): expected literal %s, read %s\n", t->lineno, token_to_string(a->branch->token), token_to_string(t->type));
				update_synt_error(e, buf, t->lineno, 0);
				a->inputidx--;
				// printf("Unshifted %s\n", token_to_string(t->type));
				// generator_unshift(gen, t);
				return 0;
			}
		} else {
			struct rule *rule = &gram->rules[a->branch->rule];
			int i;
#ifdef VERBOSE_PARSER_DEBUG
			fprintf(stderr, "parser(): > trying to unify rule %d (%s)\n", a->branch->rule, rule->name);
#endif
			synt_tree *tree = malloc(sizeof(synt_tree));
			tree->type = 1;
			tree->next = NULL;
			tree->fst_child = NULL;
			tree->rule = a->branch->rule;
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
#ifdef VERBOSE_PARSER_DEBUG
				printf("[%p] Queued %p (%s)\n", a, new_attempt, rule->name);
#endif
			}
			a->branch = NULL;
			return 0;
		}
		a->branch = a->branch->next;
	}

#ifdef VERBOSE_PARSER_DEBUG
	printf("[%p] Completed\n", a);
#endif

	if(a->branch == NULL) {
		if(a->parent == NULL) {
			// Wij zijn S
			if(lazyarray_exists(gen, a->inputidx)) {
#ifdef VERBOSE_PARSER_DEBUG
				puts("Complete, but not at EOF");
#endif
				return 0;
			}
#ifdef VERBOSE_PARSER_DEBUG
			puts("Great success");
#endif
			return 1;
		}
		a->parent->branch = a->parentbranch;
		a->parent->tree = a->parenttree;
		a->parent->next = att_head;
		a->parent->inputidx = a->inputidx;
		att_head = a->parent;
#ifdef VERBOSE_PARSER_DEBUG
		printf("[%p] Requeuing parent %p\n", a, a->parent);
#endif
		// TODO: only free if there are no childs
		//free(a);
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

void
print_indent(int indent) {
	int i;
	for(i = 0; indent > i; i++) {
		printf("  ");
	}
}

void
show_synt_tree(synt_tree *t, int indent, grammar *gram)
{
	assert(t->type == 0 || t->type == 1);
	if(t->type == 1) {
		struct rule *rule = &gram->rules[t->rule];
		print_indent(indent);
		printf("%s\n", rule->name);
		synt_tree *el = t->fst_child;
		while(el != NULL) {
			show_synt_tree(el, indent + 1, gram);
			el = el->next;
		}
	} else {
		print_indent(indent);
		printf("Token: %s\n", token_to_string(t->token->type));
		switch(t->token->type) {
		case T_WORD:
			print_indent(indent);
			printf("Value: %s\n", t->token->value.sval);
			break;
		case T_NUMBER:
			print_indent(indent);
			printf("Value: %d\n", t->token->value.ival);
		}
	}
}

void
pretty_print(synt_tree *t, struct pretty_print_state *state, grammar *gram)
{
	assert(state != NULL);
#define WHITESPACE_TEXT_BEGIN if(state->had_text) { printf(" "); }

	if(t->type == 1) {
		synt_tree *el = t->fst_child;
		while(el != NULL) {
			pretty_print(el, state, gram);
			if(el->type == 1) {
				struct rule *subrule = &gram->rules[el->rule];
				if(strcmp(subrule->name, "FunDecl") == 0
				|| strcmp(subrule->name, "VarDecl") == 0
				|| strcmp(subrule->name, "RetType") == 0
				|| strcmp(subrule->name, "Stmt")    == 0)
				{
					printf("\n");
					print_indent(state->indent);
					state->had_text = 0;
				}
			}
			el = el->next;
		}
	} else {
		switch(t->token->type) {
		case T_INT:
			WHITESPACE_TEXT_BEGIN;
			printf("Int");
			state->had_text = 1;
			break;
		case T_BOOL:
			WHITESPACE_TEXT_BEGIN;
			printf("Bool");
			state->had_text = 1;
			break;
		case T_VOID:
			WHITESPACE_TEXT_BEGIN;
			printf("Void");
			state->had_text = 1;
			break;
		case T_WORD:
			WHITESPACE_TEXT_BEGIN;
			printf("%s", t->token->value.sval);
			state->had_text = 1;
			break;
		case T_NUMBER:
			WHITESPACE_TEXT_BEGIN;
			printf("%d", t->token->value.ival);
			state->had_text = 1;
			break;
		case T_IF:
			WHITESPACE_TEXT_BEGIN;
			printf("if");
			state->had_text = 1;
			break;
		case T_WHILE:
			WHITESPACE_TEXT_BEGIN;
			printf("while");
			state->had_text = 1;
			break;
		case T_ELSE:
			WHITESPACE_TEXT_BEGIN;
			printf("else");
			state->had_text = 1;
			break;
		case T_RETURN:
			WHITESPACE_TEXT_BEGIN;
			printf("return");
			state->had_text = 1;
			break;
		case T_TRUE:
			WHITESPACE_TEXT_BEGIN;
			printf("True");
			state->had_text = 1;
			break;
		case T_FALSE:
			WHITESPACE_TEXT_BEGIN;
			printf("False");
			state->had_text = 1;
			break;
		case T_AND:
			printf(" && ");
			state->had_text = 0;
			break;
		case T_OR:
			printf(" || ");
			state->had_text = 0;
			break;
		case T_EQ:
			printf(" == ");
			state->had_text = 0;
			break;
		case T_NE:
			printf(" != ");
			state->had_text = 0;
			break;
		case T_GTE:
			printf(" >= ");
			state->had_text = 0;
			break;
		case T_LTE:
			printf(" <= ");
			state->had_text = 0;
			break;
		case '{':
			printf("{\n");
			(state->indent)++;
			print_indent(state->indent);
			state->had_text = 0;
			break;
		case '}':
			assert(state->indent > 0);
			printf("\b\b}\n");
			(state->indent)--;
			print_indent(state->indent);
			state->had_text = 0;
			break;
		case ',':
			printf(", ");
			state->had_text = 0;
			break;
		case ']':
			printf("]");
			state->had_text = 1;
			break;
		case '=':
			printf(" = ");
			state->had_text = 0;
			break;
		case ')':
			printf(")");
			state->had_text = 1;
			break;
		default:
			printf("%c", t->token->type);
			state->had_text = 0;
		}
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
