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
struct variable {
	char *name;
	struct type *type;
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
	struct type *types[2000];
	struct tc_func *funcs;
	struct tc_func **funcs_last;
	struct vardecl *decls;
	struct vardecl **decls_last;

	// Temporary hack
	struct tc_func *curfunc;
};

#define PARSING_FAIL(x) do { fprintf(stderr, "Error while parsing: %s\n", x); abort(); } while(0)

#define DESCEND_ARGS struct tc_globals *tg, synt_tree *t, void *arg
#define DESCEND_FUNC(name) void tc_descend_ ## name(DESCEND_ARGS)
typedef void (*descend_ft)(DESCEND_ARGS);

descend_ft *rule_handlers;

void
show_type(int indent, struct type *t)
{
	print_indent(indent);
	printf("Type: ");
	switch(t->type) {
	case '[':
		if(t->list_type == 0) {
			printf("Empty list of any type\n");
		} else {
			printf("List of type:\n");
			show_type(indent + 1, t->list_type);
		}
		return;
	case '(':
		printf("Tuple of types:\n");
		show_type(indent + 1, t->fst_type);
		show_type(indent + 1, t->snd_type);
		return;
	case 'a':
		printf("Anonymous type with scope %s\n", t->scope);
		return;
	case T_INT:
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
	assert(t->type == 1);
	assert(rule == t->rule); // XXX argument rule kan weg.
	rule_handlers[rule](tg, t, arg);
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
			tc_descend_type(tg, fc->next->next->next, &(*typepp)->snd_type);
			assert(fc->next->next->next->next->type == 0 && fc->next->next->next->next->token->type == ')');
			break;
		case '[':
			tc_descend_type(tg, fc->next, &(*typepp)->list_type);
			assert(fc->next->next->type == 0 && fc->next->next->token->type == ']');
			break;
		default:
			fprintf(stderr, "Unexpected token in type declaration\n");
			abort();
	}

	int i;
	for(i = 0; tg->types[i] != NULL; i++) {
		assert(i < sizeof(tg->types) / sizeof(struct type *) /* static allocated buffer overflow */);
		if((*typepp)->type != tg->types[i]->type) {
			continue;
		}
		switch(tg->types[i]->type) {
			case T_INT:
			case T_BOOL:
				goto match;
			case '[':
				if((*typepp)->list_type == tg->types[i]->list_type) {
					goto match;
				}
				break;
			case '(':
				if((*typepp)->fst_type == tg->types[i]->fst_type && (*typepp)->snd_type == tg->types[i]->snd_type) {
					goto match;
				}
				break;
			default:
				abort();
		}
	}
	tg->types[i] = *typepp;
	return;

match:
	*typepp = tg->types[i];
}

void
unify_types(struct tc_globals *tg, struct type *store, struct type *data) {
	if(store->type == data->type) {
		switch(store->type) {
			case '[':
				if(data->list_type == NULL) {
					return;
				}
				unify_types(tg, store->list_type, data->list_type);
				break;
			case '(':
				unify_types(tg, store->fst_type, data->fst_type);
				unify_types(tg, store->snd_type, data->snd_type);
		}
		return;
	}
	fprintf(stderr, "Type unification failed\n");
	abort();
}

struct variable *
lookup_variable(struct tc_globals *tg, struct tc_func *cf, const char *name) {
	struct vardecl *vd;
	struct func_arg *fa;
	struct variable *var = malloc(sizeof(struct variable));

	vd = cf->decls;
	while(vd != NULL) {
		if(strcmp(vd->name, name) == 0) {
			var->name = vd->name;
			var->type = vd->type;
			return var;
		}
		vd = vd->next;
	}

	fa = cf->args;
	while(fa != NULL) {
		if(strcmp(fa->name, name) == 0) {
			var->name = fa->name;
			var->type = fa->type;
			return var;
		}
		fa = fa->next;
	}

	vd = tg->decls;
	while(vd != NULL) {
		if(strcmp(vd->name, name) == 0) {
			var->name = vd->name;
			var->type = vd->type;
			return var;
		}
		vd = vd->next;
	}

	fprintf(stderr, "Variable %s not found\n", name);
	abort();
	free(var);
	return NULL;
}

struct tc_func *
lookup_function(struct tc_globals *tg, const char *name) {
	struct tc_func *f;

	f = tg->funcs;
	while(f != NULL) {
		if(strcmp(f->name, name) == 0) {
			return f;
		}
		f = f->next;
	}

	fprintf(stderr, "Function %s not found\n", name);
	abort();
	return NULL;
}

DESCEND_FUNC(vardecl) {
	struct vardecl *vd = malloc(sizeof(struct vardecl));
	synt_tree *chld = t->fst_child;
	struct type datatype;

	vd->next = NULL;
	tc_descend(tg, chld->rule, chld, &vd->type);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	vd->name = chld->token->value.sval;
	chld = chld->next;
	assert(chld->token->type == '=');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, &datatype);
	unify_types(tg, vd->type, &datatype);
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
	tg->curfunc = fdata;

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

	*tg->funcs_last = fdata;
	tg->funcs_last = &fdata->next;

	do {
		// vardecls en stmts
		tc_descend(tg, chld->rule, chld, fdata);
		chld = chld->next;
	} while(chld->token->type != '}');

	tg->curfunc = NULL;
}

DESCEND_FUNC(if) {
	struct type datatype;
	struct type booltype;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_IF);
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, &datatype);
	booltype.type = T_BOOL;
	unify_types(tg, &booltype, &datatype);
	chld = chld->next;
	assert(chld->token->type == ')');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, arg);
	chld = chld->next;
	if(chld != NULL) {
		assert(chld->token->type == T_ELSE);
		chld = chld->next;
		tc_descend(tg, chld->rule, chld, arg);
	}
}

DESCEND_FUNC(while) {
	struct type datatype;
	struct type booltype;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_WHILE);
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, &datatype);
	booltype.type = T_BOOL;
	unify_types(tg, &booltype, &datatype);
	chld = chld->next;
	assert(chld->token->type == ')');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, arg);
}

DESCEND_FUNC(assignment) {
	struct variable *var;
	struct type datatype;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_WORD);
	var = lookup_variable(tg, arg, chld->token->value.sval);
	chld = chld->next;
	assert(chld->token->type == '=');
	chld = chld->next;
	tc_descend(tg, chld->rule, chld, &datatype);
	unify_types(tg, var->type, &datatype);
	free(var);
}

DESCEND_FUNC(funcall) {
	struct tc_func *f;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_WORD);
	f = lookup_function(tg, chld->token->value.sval);
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	if(chld->type == 1) {
		synt_tree *henk = chld->fst_child;
		struct func_arg *fa = f->args;

		while(fa != NULL) {
			struct type datatype;
			tc_descend(tg, henk->rule, henk, &datatype);
			unify_types(tg, fa->type, &datatype);
			fa = fa->next;
			if(henk->next == NULL) {
				henk = NULL;
				break;
			}
			assert(henk->next->token->type == ',');
			henk = henk->next->next->fst_child;
		}
		if(fa != NULL) {
			fprintf(stderr, "Not enough arguments for function %s\n", f->name);
			abort();
		}
		if(henk != NULL) {
			fprintf(stderr, "Too many arguments for function %s\n", f->name);
			abort();
		}

		chld = chld->next;
	}
	assert(chld->token->type == ')');
	if(arg != NULL && arg != tg->curfunc) {
		memcpy(arg, f->returntype, sizeof(struct type));
	}
}

// Always returns an char in arg
DESCEND_FUNC(expression_simple) {
	assert(t->type == 0);
	assert(arg != 0);

	struct type *res = (struct type*)arg;

	switch(t->token->type) {
	case T_WORD: {
		struct variable *var = lookup_variable(tg, tg->curfunc, t->token->value.sval);
		memcpy(res, var->type, sizeof(struct type));
		free(var);
		return;
	}
	case T_NUMBER:
		res->type = T_INT;
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
	// XXX split this function into some smaller parts for Exp[23456]
	assert(t->type == 1);
	assert(t->fst_child != 0);
	assert(arg != 0);

	struct type *res = (struct type*)arg;

	// If we have only one child, simply return its type
	if(t->fst_child->next == 0) {
		if(t->fst_child->type == 0)
			tc_descend_expression_simple(tg, t->fst_child, arg);
		else {
			// If this is a FunCall, type is the function type
			struct rule *rule = &tg->gram->rules[t->fst_child->rule];
			if(strcmp(rule->name, "FunCall") == 0)
				tc_descend_funcall(tg, t->fst_child, arg);
			else
				tc_descend_expression(tg, t->fst_child, arg);
		}
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
				res->fst_type = calloc(1, sizeof(struct type));
				res->snd_type = calloc(1, sizeof(struct type));
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

DESCEND_FUNC(return) {
	assert(arg != 0);
	assert(t->fst_child->type == 0 && t->fst_child->token->type == T_RETURN);
	assert(t->fst_child->next->next == 0 || t->fst_child->next->next->next == 0);

	struct tc_func *tc = arg;
	struct type newtype;
	if(t->fst_child->next->next == 0) {
		assert(t->fst_child->next->type == 0 && t->fst_child->next->token->type == ';');
		newtype.type = T_VOID;
	} else {
		assert(t->fst_child->next->next->type == 0 && t->fst_child->next->next->token->type == ';');
		tc_descend_expression(tg, t->fst_child->next, &newtype);
	}
	unify_types(tg, tc->returntype, &newtype);
}


void
typechecker(synt_tree *t, grammar *gram) {
	int i;
	struct tc_globals tg;
	memset(&tg, 0, sizeof(struct tc_globals));
	tg.gram = gram;
	tg.funcs_last = &tg.funcs;
	rule_handlers = malloc((gram->lastrule + 1) * sizeof(descend_ft));
	for(i = 0; gram->lastrule >= i; i++) {
		rule_handlers[i] = tc_descend_simple;
	}
#define SET_RULE_HANDLER(rulename, func)	rule_handlers[get_id_for_rule(gram, #rulename)] = tc_descend_ ## func;
	SET_RULE_HANDLER(Decl, parallel); // XXX
	SET_RULE_HANDLER(Exp, expression);
	SET_RULE_HANDLER(Type, type);
	SET_RULE_HANDLER(VarDecl, vardecl);
	SET_RULE_HANDLER(RetType, rettype);
	SET_RULE_HANDLER(FunDecl, fundecl);
	SET_RULE_HANDLER(FArgs, fargs);
	SET_RULE_HANDLER(If, if);
	SET_RULE_HANDLER(While, while);
	SET_RULE_HANDLER(Assignment, assignment);
	SET_RULE_HANDLER(FunCall, funcall);
	SET_RULE_HANDLER(Return, return);
	tc_descend(&tg, get_id_for_rule(gram, "S"), t, NULL);
}
