#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"
#include "types.h"

struct tc_globals {
	grammar *gram;
};

#define PARSING_FAIL(x) do { fprintf(stderr, "Error while parsing: %s\n", x); abort(); } while(0)

#define DESCEND_ARGS struct tc_globals *tg, synt_tree *t, void *arg
#define DESCEND_FUNC(name) void tc_descend_ ## name(DESCEND_ARGS)
typedef void (*descend_ft)(DESCEND_ARGS);

descend_ft *rule_handlers;

void
show_type(struct tc_globals *tg, int indent, struct type *t)
{
	print_indent(indent);
	printf("Type: ");
	switch(t->type) {
	case '[':
		if(t->list_type == 0) {
			printf("Empty list of any type\n");
		} else {
			printf("List of type:\n");
			show_type(tg, indent + 1, t->list_type);
		}
		return;
	case '(':
		printf("Tuple of types:\n");
		show_type(tg, indent + 1, t->fst_type);
		show_type(tg, indent + 1, t->snd_type);
		return;
	case 'a':
		printf("Anonymous type with scope %s\n", t->scope);
		return;
	case T_NUMBER:
		printf("Integer\n");
		return;
	case T_BOOL:
		printf("Boolean\n");
		return;
	default:
		printf("Unknown type %d\n", t->type);
	}
}

void
tc_descend(struct tc_globals *tg, int rule, synt_tree *t, void *arg) {
	rule_handlers[rule](tg, t, arg);
}

DESCEND_FUNC(simple) {
	synt_tree *chld;
	assert(t->type == 1);

	chld = t->fst_child;
	while(chld != NULL) {
		if(chld->type == 1) {
			tc_descend(tg, chld->rule, chld, arg);
		}
		chld = chld->next;
	}
}

DESCEND_FUNC(parallel) {
	// XXX
	tc_descend_simple(tg, t, arg);
}

DESCEND_FUNC(type) {
	struct type **typepp = (struct type **)arg;
	synt_tree *fc = t->fst_child;
	assert(t->type == 1);
	assert(fc->type == 0);
	*typepp = malloc(sizeof(struct type));
	(*typepp)->type = fc->token->type;
	switch(fc->token->type) {
		case T_INT:
		case T_BOOL:
			assert(fc->next == NULL);
			break;
		case '(':
			tc_descend_type(tg, fc->next, &(*typepp)->fst_type);
			assert(fc->next->next->type == 0 && fc->next->next->token->type == ',');
			tc_descend_simple(tg, fc->next->next->next, &(*typepp)->snd_type);
			assert(fc->next->next->next->next->type == 0 && fc->next->next->next->next->token->type == ')');
			break;
		case '[':
			tc_descend_type(tg, fc->next, &(*typepp)->fst_type);
			assert(fc->next->next->type == 0 && fc->next->next->token->type == ']');
			break;
	}
}

DESCEND_FUNC(vardecl) {
	struct type *type;

	tc_descend(tg, t->fst_child->rule, t->fst_child, &type);
}

// Always returns an char in arg
DESCEND_FUNC(expression_simple) {
	assert(t->type == 0);
	assert(arg != 0);

	struct type *res = (struct type*)arg;

	switch(t->token->type) {
	case T_WORD:
		// TODO: ask symbol table what the type of this variable is
		fprintf(stderr, "TODO: ask symbol table what type of this variable is\n");
		res->type = T_NUMBER;
		return;
	case T_NUMBER:
		res->type = T_NUMBER;
		return;
	case T_FALSE:
	case T_TRUE:
		res->type = T_BOOL;
		return;
	default:
		fprintf(stderr, "Unexpected token type in Expression: %s\n", token_to_string(t->token->type));
		abort();
	}
}

// Always returns a struct type* in arg
DESCEND_FUNC(expression) {
	assert(t->type == 1);
	assert(t->fst_child != 0);
	assert(arg != 0);

	struct type *res = (struct type*)arg;

	// If we have only one child, simply return its type
	if(t->fst_child->next == 0) {
		if(t->fst_child->type == 0) {
			tc_descend_expression_simple(tg, t->fst_child, &res->type);
		} else
			tc_descend_expression(tg, t->fst_child, arg);
		return;
	}

	// Otherwise, we are a derived type; see if we can see the type from
	// the first literal
	if(t->fst_child->type == 0) {
		switch(t->fst_child->token->type) {
		case '!': // Boolean negation: Bool -> Bool
			assert(t->fst_child->next->next == 0);
			tc_descend_expression(tg, t->fst_child->next, arg);
			if(res->type != T_BOOL)
				PARSING_FAIL("Boolean negation (!) works only on booleans");
			res->type = T_BOOL;
			return;
		case '-': // Numeric negation: Int -> Int
			assert(t->fst_child->next->next == 0);
			tc_descend_expression(tg, t->fst_child->next, arg);
			if(res->type != T_INT)
				PARSING_FAIL("Numeric negation (-) works only on integers");
			res->type = T_INT;
			return;
		case '[': // Empty list
			assert(t->fst_child->next->type == 0 && t->fst_child->next->token->type == ']');
			res->type = '[';
			res->list_type = 0;
			return;
		case '(':
			if(t->fst_child->next->next->next == 0) { // Exp within braces
				assert(t->fst_child->next->next->token->type == ')');
				tc_descend_expression(tg, t->fst_child->next, arg);
			} else { // Tuple
				assert(t->fst_child->next->next->token->type == ',');
				assert(t->fst_child->next->next->next->next->token->type == ')');
				res->type = '(';
				res->fst_type = calloc(1, sizeof(struct type*));
				res->snd_type = calloc(1, sizeof(struct type*));
				tc_descend_expression(tg, t->fst_child->next, res->fst_type);
				tc_descend_expression(tg, t->fst_child->next->next->next, res->snd_type);
			}
			return;
		default:
			// it's something simple, or all has failed
			tc_descend_expression_simple(tg, t->fst_child, arg);
			return;
		}
	}

	// If first literal is a FunCall, type is the function type
	struct rule *rule = &tg->gram->rules[t->fst_child->rule];
	if(strcmp(rule->name, "FunCall")) {
		// TODO: ask symbol table what the type of this function is
		fprintf(stderr, "TODO: ask symbol table what type of this function is\n");
		res->type = T_NUMBER;
		return;
	}

	// First literal is an Expression. From now on, types are dictated
	// by the operators in between; check them here.
	assert(t->fst_child->next->type == 0);
	assert(t->fst_child->next->next->next == 0);
	struct type fst, snd;
	tc_descend_expression(tg, t->fst_child, &fst);
	tc_descend_expression(tg, t->fst_child->next->next, &snd);

	switch(t->fst_child->next->token->type) {
	// Numeric comparison operators
	case '<': case '>': case T_LTE: case T_GTE:
		if(fst.type != T_INT || snd.type != T_INT)
			PARSING_FAIL("Numeric comparison (<, >, <=, >=) works only on integers");
		res->type = T_BOOL;
		return;
	// Binary composition operators
	case T_AND:
	case T_OR:
		if(fst.type != T_BOOL || snd.type != T_BOOL)
			PARSING_FAIL("Binary composition (&&, ||) works only on booleans");
		res->type = T_BOOL;
		return;
	// Binary or numeric equality operators
	case T_EQ: case T_NE:
		if((fst.type != T_INT && fst.type != T_BOOL)
		|| (snd.type != T_INT && snd.type != T_BOOL))
			PARSING_FAIL("Binary or numeric equality (==, !=) works only on booleans and integers");
		if(fst.type != snd.type)
			PARSING_FAIL("Binary or numeric equality (==, !=) can only work on two equal types");
		res->type = T_BOOL;
	// Numeric mathematical operators
	case '+': case '-': case '*': case '/': case '%':
		if(fst.type != T_INT || snd.type != T_INT)
			PARSING_FAIL("Numeric mathematics (+, -, *, /, %) work only on integers");
		res->type = T_INT;
		return;
	default:
		fprintf(stderr, "Unexpected operator in Expression type checking\n");
		abort();
	}
}

void
typechecker(synt_tree *t, grammar *gram) {
	int i;
	struct tc_globals tg;
	tg.gram = gram;
	rule_handlers = malloc(gram->lastrule * sizeof(descend_ft));
	for(i = 0; gram->lastrule >= i; i++) {
		rule_handlers[i] = tc_descend_simple;
	}
#define SET_RULE_HANDLER(rulename, func)	rule_handlers[get_id_for_rule(gram, #rulename)] = tc_descend_ ## func;
	SET_RULE_HANDLER(Decl, parallel); // XXX
	SET_RULE_HANDLER(Exp, expression);
	tc_descend(&tg, get_id_for_rule(gram, "S"), t, NULL);
}
