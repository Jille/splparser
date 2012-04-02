#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"
#include "types.h"

struct tc_globals {
	grammar *gram;
};

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
	tc_descend(&tg, get_id_for_rule(gram, "S"), t, NULL);
}
