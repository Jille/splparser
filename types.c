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

struct vardecl {
	char *name;
	struct type *type;
	synt_tree *initial;
	struct vardecl *next;
};
struct func_arg {
	char *name;
	struct type *type;
	struct func_arg *next;
};
struct tc_func {
	struct type *returntype;
	char *name;
	struct func_arg *args;
	struct vardecl *decls;
	synt_tree *stmts;
	struct func_arg **args_last;
	struct vardecl **decls_last;
	synt_tree **stmts_last;
	struct tc_func *next;
};
struct tc_globals {
	grammar *gram;
	struct tc_func *funcs;
	struct tc_func **funcs_last;
	struct vardecl *decls;
	struct vardecl **decls_last;
};

#define DESCEND_ARGS struct tc_globals *tg, synt_tree *t, void *arg
#define DESCEND_FUNC(name) void tc_descend_ ## name(DESCEND_ARGS)
typedef void (*descend_ft)(DESCEND_ARGS);

descend_ft *rule_handlers;

void
tc_descend(struct tc_globals *tg, int rule, synt_tree *t, void *arg) {
	assert(t->type == 1);
	assert(rule == t->rule); // XXX argument rule kan weg.
	printf("Enter %s\n", tg->gram->rules[rule].name);
	rule_handlers[rule](tg, t, arg);
	printf("Leave %s\n", tg->gram->rules[rule].name);
}

DESCEND_FUNC(simple) {
	synt_tree *chld;

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
	struct type **typepp = arg;
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
	struct vardecl *vd = malloc(sizeof(struct vardecl));
	synt_tree *chld = t->fst_child;

	vd->next = NULL;
	tc_descend(tg, chld->rule, chld, &vd->type);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	vd->name = chld->token->value.sval;
	chld = chld->next;
	assert(chld->token->type == '=');
	chld = chld->next;
	vd->initial = chld;
	chld = chld->next;
	assert(chld->token->type == ';');

	if(arg != NULL) {
		struct tc_func *fdata = arg;
		*fdata->decls_last = vd;
		fdata->decls_last = &vd->next;
	} else {
		*tg->decls_last = vd;
		tg->decls_last = &vd->next;
	}
}

DESCEND_FUNC(fargs) {
	struct tc_func *fdata = arg;
	struct func_arg *fa = malloc(sizeof(struct vardecl));
	synt_tree *chld = t->fst_child;

	fa->next = NULL;
	tc_descend(tg, chld->rule, chld, &fa->type);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	fa->name = chld->token->value.sval;
	chld = chld->next;
	if(chld != NULL) {
		chld = chld->next;
		tc_descend(tg, chld->rule, chld, arg);
	}

	*fdata->args_last = fa;
	fdata->args_last = &fa->next;
}

DESCEND_FUNC(rettype) {
	struct type **typepp = arg;

	if(t->fst_child->type == 1) {
		tc_descend(tg, t->fst_child->rule, t->fst_child, arg);
	} else {
		assert(t->fst_child->token->type == T_VOID);
		*typepp = malloc(sizeof(struct type));
		(*typepp)->type = t->fst_child->token->type;
	}
}

DESCEND_FUNC(fundecl) {
	struct tc_func *fdata = calloc(1, sizeof(struct tc_func));
	synt_tree *chld = t->fst_child;

	fdata->args_last = &fdata->args;
	fdata->decls_last = &fdata->decls;
	fdata->stmts_last = &fdata->stmts;

	tc_descend(tg, chld->rule, chld, &fdata->returntype);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	fdata->name = chld->token->value.sval;
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	if(chld->type == 1) {
		tc_descend(tg, chld->rule, chld, fdata);
		chld = chld->next;
	}
	assert(chld->token->type == ')');
	chld = chld->next;
	assert(chld->token->type == '{');
	chld = chld->next;
	do {
		// vardecls en stmts
		tc_descend(tg, chld->rule, chld, fdata);
		chld = chld->next;
	} while(chld->token->type != '}');

	*tg->funcs_last = fdata;
	tg->funcs_last = &fdata->next;
}

void
typechecker(synt_tree *t, grammar *gram) {
	int i;
	struct tc_globals tg;
	tg.gram = gram;
	tg.funcs_last = &tg.funcs;
	rule_handlers = malloc((gram->lastrule + 1) * sizeof(descend_ft));
	for(i = 0; gram->lastrule >= i; i++) {
		rule_handlers[i] = tc_descend_simple;
	}
#define SET_RULE_HANDLER(rulename, func)	rule_handlers[get_id_for_rule(gram, #rulename)] = tc_descend_ ## func;
	SET_RULE_HANDLER(Decl, parallel); // XXX
	SET_RULE_HANDLER(Type, type);
	SET_RULE_HANDLER(VarDecl, vardecl);
	SET_RULE_HANDLER(RetType, rettype);
	SET_RULE_HANDLER(RetType, rettype);
	SET_RULE_HANDLER(FunDecl, fundecl);
	SET_RULE_HANDLER(FArgs, fargs);
	tc_descend(&tg, get_id_for_rule(gram, "S"), t, NULL);
}
