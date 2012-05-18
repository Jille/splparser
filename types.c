#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"
#include "grammar.h"
#include "parser.h"
#include "ir.h"
#include "types.h"

struct vardecl {
	char *name;
	struct type *type;
	union {
		irlocal local;
		irglobal global;
	};
	struct vardecl *next;
};
struct func_arg {
	char *name;
	struct type *type;
	irfarg farg;
	struct func_arg *next;
};
struct pm_bind {
	struct type *pm;
	struct type *bound;
	struct pm_bind *next;
};
struct variable {
	char *name;
	struct type *type;
	irtype irtype;
	union {
		irlocal local;
		irglobal global;
		irfarg farg;
	};
};
struct tc_func {
	struct type *returntype;
	char *name;
	struct func_arg *args;
	struct vardecl *decls;
	synt_tree *stmts;
	struct vardecl **decls_last;
	synt_tree **stmts_last;
	struct type *pm_types;
	irfunc func;
	int nargs;
	int nlocals;
	struct tc_func *next;
};
struct tc_globals {
	grammar *gram;
	int funcall_rule;
	struct type *types;
	struct tc_func *funcs;
	struct tc_func **funcs_last;
	struct vardecl *decls;
	struct vardecl **decls_last;
	int nglobals;

	// Temporary hack
	struct tc_func *curfunc;
};

#define PARSING_FAIL(x) do { fprintf(stderr, "Error while parsing: %s\n", x); abort(); } while(0)

#define DESCEND_ARGS struct tc_globals *tg, synt_tree *t, void *arg
#define DESCEND_FUNC(name) struct irunit *tc_descend_ ## name(DESCEND_ARGS)
typedef struct irunit *(*descend_ft)(DESCEND_ARGS);

descend_ft *rule_handlers;

void
show_type(int indent, struct type *t)
{
	print_indent(indent);
	printf("Type: ");
	switch(t->type) {
	case '[':
		if(t->list_type == NULL) {
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
	case T_WORD:
		if(t->accept_empty_list) {
			printf("Anonymous list type with name %s\n", t->pm_name);
		} else {
			printf("Anonymous type with name %s\n", t->pm_name);
		}
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
unify_types(struct tc_globals *tg, struct type *store, struct type *data, struct pm_bind **bindspp) {
	if(store->type == data->type) {
		switch(store->type) {
			case '[':
				if(data->list_type == NULL) {
					return;
				}
				unify_types(tg, store->list_type, data->list_type, bindspp);
				return;
			case '(':
				unify_types(tg, store->fst_type, data->fst_type, bindspp);
				unify_types(tg, store->snd_type, data->snd_type, bindspp);
				return;
			case T_WORD:
				if(strcmp(store->pm_name, data->pm_name) == 0) {
					return;
				}
				break;
			default:
				return;
		}
	}
	if(bindspp != NULL && store->type == T_WORD) {
		struct pm_bind *bind = *bindspp;
		while(bind != NULL) {
			if(bind->pm == store) {
				unify_types(tg, bind->bound, data, bindspp);
				return;
			}
			bind = bind->next;
		}

		if(data->type == '[' && data->list_type == NULL) {
			store->accept_empty_list = 1;
			return;
		}
		if(!store->accept_empty_list) {
			assert(bind == NULL);
			bind = malloc(sizeof(struct pm_bind));
			bind->pm = store;
			bind->bound = malloc(sizeof(struct type));
			*bind->bound = *data;
			bind->next = *bindspp;
			*bindspp = bind;
			return;
		}
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
			var->irtype = LOCAL;
			var->local = vd->local;
			return var;
		}
		vd = vd->next;
	}

	fa = cf->args;
	while(fa != NULL) {
		if(strcmp(fa->name, name) == 0) {
			var->name = fa->name;
			var->type = fa->type;
			var->irtype = FARG;
			var->farg = fa->farg;
			return var;
		}
		fa = fa->next;
	}

	vd = tg->decls;
	while(vd != NULL) {
		if(strcmp(vd->name, name) == 0) {
			var->name = vd->name;
			var->type = vd->type;
			var->irtype = GLOBAL;
			var->global = vd->global;
			return var;
		}
		vd = vd->next;
	}

	fprintf(stderr, "Variable %s not found\n", name);
	abort();
	free(var);
	return NULL;
}

static irexp *
variable_to_irexp(struct variable *var) {
	switch(var->irtype) {
		case GLOBAL:
			return mkirglobal(var->global);
		case LOCAL:
			return mkirlocal(var->local);
		case FARG:
			return mkirfarg(var->farg);
		default:
			assert(!"reached");
	}
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

struct type *
resolve_pm_types(struct type *in, struct pm_bind **binds) {
	struct type *new = malloc(sizeof(struct type));
	new->type = in->type;
	switch(in->type) {
		case '[':
			new->list_type = resolve_pm_types(in->list_type, binds);
			break;
		case '(':
			new->fst_type = resolve_pm_types(in->fst_type, binds);
			new->snd_type = resolve_pm_types(in->snd_type, binds);
			break;
		case T_WORD: {
			struct pm_bind *bind = *binds;
			while(bind != NULL) {
				if(strcmp(bind->pm->pm_name, in->pm_name) == 0) {
					free(new);
					return resolve_pm_types(bind->bound, binds);
				}
				bind = bind->next;
			}
			} // FALL THROUGH
		default:
			free(new);
			new = in;
	}
	return new;
}

struct irunit *
tc_descend(struct tc_globals *tg, synt_tree *t, void *arg) {
	assert(t->type == 1);
	return rule_handlers[t->rule](tg, t, arg);
}

DESCEND_FUNC(simple) {
	synt_tree *chld;
	struct irunit *ret = NULL;

	chld = t->fst_child;
	while(chld != NULL) {
		if(chld->type == 1) {
			struct irunit *res = tc_descend(tg, chld, arg);
			if(res != NULL) {
				if(ret == NULL) {
					ret = res;
				} else {
					fprintf(stderr, "Warning: Simple descender got two irunits (rule %s)\n", tg->gram->rules[t->rule].name);
				}
			}
		}
		chld = chld->next;
	}
	return ret;
}

DESCEND_FUNC(simple_seq) {
	synt_tree *chld;
	irstm *ret = NULL;

	chld = t->fst_child;
	while(chld != NULL) {
		if(chld->type == 1) {
			irstm *res = tc_descend(tg, chld, arg);
			if(res != NULL) {
				if(ret == NULL) {
					ret = res;
				} else {
					ret = mkirseq(ret, res);
				}
			}
		}
		chld = chld->next;
	}
	return ret;
}

DESCEND_FUNC(init) {
	struct irunit *program = tc_descend(tg, t->fst_child, arg);
	struct tc_func *f = lookup_function(tg, "main");
	// XXX check whether main's (return)type is correct
	return irconcat(mkirexp(mkircall(f->func, NULL)), mkirtrap(0), mkirhalt(), program, NULL);
}

DESCEND_FUNC(rettype) {
	struct type **typepp = arg;

	if(t->fst_child->type == 1) {
		return tc_descend(tg, t->fst_child, arg);
	} else {
		assert(t->fst_child->token->type == T_VOID);
		*typepp = malloc(sizeof(struct type));
		(*typepp)->type = t->fst_child->token->type;
	}
	return NULL;
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
		case T_WORD:
			(*typepp)->pm_name = fc->token->value.sval;
			(*typepp)->accept_empty_list = 0;
			break;
		default:
			fprintf(stderr, "Unexpected token in type declaration\n");
			abort();
	}

	struct type *ts;
	if(fc->token->type == T_WORD) {
		struct tc_func *fdata = tg->curfunc;
		ts = fdata->pm_types;
		while(ts != NULL) {
			assert(ts->type == T_WORD);
			if(strcmp(ts->pm_name, (*typepp)->pm_name) == 0) {
				goto match;
			}
			ts = ts->next;
		}
		(*typepp)->next = fdata->pm_types;
		fdata->pm_types = *typepp;
	} else {
		ts = tg->types;
		while(ts != NULL) {
			if((*typepp)->type == ts->type) {
				switch(ts->type) {
					case T_INT:
					case T_BOOL:
						goto match;
					case '[':
						if((*typepp)->list_type == ts->list_type) {
							goto match;
						}
						break;
					case '(':
						if((*typepp)->fst_type == ts->fst_type && (*typepp)->snd_type == ts->snd_type) {
							goto match;
						}
						break;
					default:
						abort();
				}
			}
			ts = ts->next;
		}
		(*typepp)->next = tg->types;
		tg->types = *typepp;
	}
	return NULL;

match:
	free(*typepp);
	*typepp = ts;
	return NULL;
}

DESCEND_FUNC(vardecl) {
	struct vardecl *vd = malloc(sizeof(struct vardecl));
	synt_tree *chld = t->fst_child;
	struct type datatype;
	struct irunit *initexp;

	vd->next = NULL;
	tc_descend(tg, chld, &vd->type);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	vd->name = chld->token->value.sval;
	chld = chld->next;
	assert(chld->token->type == '=');
	chld = chld->next;
	initexp = tc_descend(tg, chld, &datatype);
	unify_types(tg, vd->type, &datatype, NULL);
	chld = chld->next;
	assert(chld->token->type == ';');

	if(arg != NULL) {
		struct tc_func *fdata = arg;
		*fdata->decls_last = vd;
		fdata->decls_last = &vd->next;
		vd->local = ++fdata->nlocals;

		return mkirmove(mkirlocal(vd->local), initexp);
	} else {
		*tg->decls_last = vd;
		tg->decls_last = &vd->next;

		vd->global = ++tg->nglobals;
		return mkirmove(mkirglobal(vd->global), initexp);
	}
	return NULL;
}

DESCEND_FUNC(fargs) {
	struct tc_func *fdata = arg;
	struct func_arg *fa = malloc(sizeof(struct vardecl));
	synt_tree *chld = t->fst_child;

	fa->farg = ++fdata->nargs;
	fa->next = NULL;
	tc_descend(tg, chld, &fa->type);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	fa->name = chld->token->value.sval;
	chld = chld->next;
	if(chld != NULL) {
		chld = chld->next;
		tc_descend(tg, chld, arg);
	}

	fa->next = fdata->args;
	fdata->args = fa;
	return NULL;
}

DESCEND_FUNC(fundecl) {
	struct tc_func *fdata = calloc(1, sizeof(struct tc_func));
	synt_tree *chld = t->fst_child;
	tg->curfunc = fdata;
	irstm *body = NULL;

	fdata->decls_last = &fdata->decls;
	fdata->stmts_last = &fdata->stmts;

	tc_descend(tg, chld, &fdata->returntype);
	chld = chld->next;
	assert(chld->token->type == T_WORD);
	fdata->name = chld->token->value.sval;
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	if(chld->type == 1) {
		tc_descend(tg, chld, fdata);
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
		struct irunit *res = tc_descend(tg, chld, fdata);
		if(res != NULL) {
			if(body == NULL) {
				body = res;
			} else {
				body = mkirseq(body, res);
			}
		}
		chld = chld->next;
	} while(chld->token->type != '}');

	tg->curfunc = NULL;

	fdata->func = getfunc();
	return mkirseq(mkirfunc(fdata->func, fdata->nargs, fdata->nlocals), body);
}

DESCEND_FUNC(if) {
	struct type datatype;
	struct type booltype;
	irexp *cond;
	irstm *iftrue, *iffalse = NULL;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_IF);
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	cond = tc_descend(tg, chld, &datatype);
	booltype.type = T_BOOL;
	unify_types(tg, &booltype, &datatype, NULL);
	chld = chld->next;
	assert(chld->token->type == ')');
	chld = chld->next;
	iftrue = tc_descend(tg, chld, arg);
	chld = chld->next;
	if(chld != NULL) {
		assert(chld->token->type == T_ELSE);
		chld = chld->next;
		iffalse = tc_descend(tg, chld, arg);
	}
	irlabel false = getlabel();
	return irconcat(mkircjump(cond, false), mkirseq_opt(iftrue, mkirseq_opt(mkirlabel(false), iffalse)), NULL);
}

DESCEND_FUNC(while) {
	struct type datatype;
	struct type booltype;
	irexp *cond;
	irstm *body;
	irlabel start, done;
	synt_tree *chld = t->fst_child;

	assert(chld->token->type == T_WHILE);
	chld = chld->next;
	assert(chld->token->type == '(');
	chld = chld->next;
	cond = tc_descend(tg, chld, &datatype);
	booltype.type = T_BOOL;
	unify_types(tg, &booltype, &datatype, NULL);
	chld = chld->next;
	assert(chld->token->type == ')');
	chld = chld->next;
	body = tc_descend(tg, chld, arg);

	start = getlabel();
	done = getlabel();

	return irconcat(mkirlabel(start), mkircjump(cond, done), mkirseq_opt(body, mkirjump(mkirname(start))), mkirlabel(done), NULL);
}

DESCEND_FUNC(assignment) {
	struct variable *var;
	struct type datatype;
	irexp *val;
	synt_tree *chld = t->fst_child;
	struct irunit *ret;

	assert(chld->token->type == T_WORD);
	var = lookup_variable(tg, arg, chld->token->value.sval);
	chld = chld->next;
	assert(chld->token->type == '=');
	chld = chld->next;
	val = tc_descend(tg, chld, &datatype);
	unify_types(tg, var->type, &datatype, NULL);

	ret = mkirmove(variable_to_irexp(var), val);
	free(var);
	return ret;
}

DESCEND_FUNC(funcall) {
	struct tc_func *f;
	synt_tree *chld = t->fst_child;
	struct pm_bind *binds = NULL;
	struct irexplist *args = NULL;
	struct irexplist **args_last = &args;

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
			*args_last = calloc(1, sizeof(struct irexplist));
			(*args_last)->exp = tc_descend(tg, henk, &datatype);
			unify_types(tg, fa->type, &datatype, &binds);
			args_last = &(*args_last)->next;
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
	if(arg != NULL) {
		struct type *type = resolve_pm_types(f->returntype, &binds);
		*(struct type *)arg = *type;
	}
	while(binds != NULL) {
		struct pm_bind *next = binds->next;
		free(binds);
		binds = next;
	}
	return mkircall(f->func, args);
}

DESCEND_FUNC(stmt) {
	if(t->fst_child->type == 1 && t->fst_child->rule == tg->funcall_rule) {
		return mkirexp(tc_descend_simple(tg, t, NULL));
	} else {
		return tc_descend_simple(tg, t, arg);
	}
}

// Always returns an char in arg
DESCEND_FUNC(expression_simple) {
	assert(t->type == 0);
	assert(arg != NULL);

	struct type *res = (struct type*)arg;

	switch(t->token->type) {
	case T_WORD: {
		struct variable *var = lookup_variable(tg, tg->curfunc, t->token->value.sval);
		memcpy(res, var->type, sizeof(struct type));
		irexp *ret = variable_to_irexp(var);
		free(var);
		return ret;
	}
	case T_NUMBER:
		res->type = T_INT;
		return mkirconst(t->token->value.ival);
	case T_FALSE:
	case T_TRUE:
		res->type = T_BOOL;
		return mkirconst(t->token->type == T_TRUE);
	default:
		fprintf(stderr, "Unexpected token type in Expression: %s\n", token_to_string(t->token->type));
		abort();
	}
}

// Always returns a struct type* in arg
DESCEND_FUNC(expression) {
	// XXX split this function into some smaller parts for Exp[23456]
	assert(t->type == 1);
	assert(t->fst_child != NULL);
	assert(arg != NULL);

	struct type *res = (struct type*)arg;

	// If we have only one child, simply return its type
	if(t->fst_child->next == NULL) {
		if(t->fst_child->type == 0)
			return tc_descend_expression_simple(tg, t->fst_child, arg);
		else {
			// If this is a FunCall, type is the function type
			if(t->fst_child->rule == tg->funcall_rule) {
				return tc_descend(tg, t->fst_child, arg);
			} else {
				return tc_descend_expression(tg, t->fst_child, arg);
			}
		}
	}

	// Otherwise, we are a derived type; see if we can see the type from
	// the first literal
	if(t->fst_child->type == 0) {
		irexp *val;
		switch(t->fst_child->token->type) {
		case '!': // Boolean negation: Bool -> Bool
			assert(t->fst_child->next->next == NULL);
			val = tc_descend_expression(tg, t->fst_child->next, arg);
			if(res->type != T_BOOL)
				PARSING_FAIL("Boolean negation (!) works only on booleans");
			res->type = T_BOOL;
			return mkirbinop(XOR, val, mkirconst(1));
		case '-': // Numeric negation: Int -> Int
			assert(t->fst_child->next->next == NULL);
			val = tc_descend_expression(tg, t->fst_child->next, arg);
			if(res->type != T_INT)
				PARSING_FAIL("Numeric negation (-) works only on integers");
			res->type = T_INT;
			return mkirbinop(MINUS, mkirconst(0), val);
		case '[': // Empty list
			assert(t->fst_child->next->type == 0 && t->fst_child->next->token->type == ']');
			res->type = '[';
			res->list_type = NULL;
			return mkirconst(0); // XXX
		case '(':
			if(t->fst_child->next->next->next == NULL) { // Exp within braces
				assert(t->fst_child->next->next->token->type == ')');
				return tc_descend_expression(tg, t->fst_child->next, arg);
			} else { // Tuple
				assert(t->fst_child->next->next->token->type == ',');
				assert(t->fst_child->next->next->next->next->token->type == ')');
				res->type = '(';
				res->fst_type = calloc(1, sizeof(struct type));
				res->snd_type = calloc(1, sizeof(struct type));
				tc_descend_expression(tg, t->fst_child->next, res->fst_type);
				tc_descend_expression(tg, t->fst_child->next->next->next, res->snd_type);
			}
			return mkirconst(0); // XXX
		default:
			// it's something simple, or all has failed
			return tc_descend_expression_simple(tg, t->fst_child, arg);
		}
	}

	// First literal is an Expression. From now on, types are dictated
	// by the operators in between; check them here.
	assert(t->fst_child->next->type == 0 || t->fst_child->next->fst_child->type == 0);
	assert(t->fst_child->next->next->next == 0);
	struct type fst, snd;
	irexp *lhs = tc_descend_expression(tg, t->fst_child, &fst);
	irexp *rhs = tc_descend_expression(tg, t->fst_child->next->next, &snd);

	char token = t->fst_child->next->type != 0 ? t->fst_child->next->fst_child->token->type : t->fst_child->next->token->type;
	switch(token) {
	// Numeric comparison operators
	case '<': case '>': case T_LTE: case T_GTE:
		if(fst.type != T_INT || snd.type != T_INT)
			PARSING_FAIL("Numeric comparison (<, >, <=, >=) works only on integers");
		res->type = T_BOOL;
		if(token == '<' || token == T_LTE) {
			return mkirbinop(LE, lhs, rhs);
		} else {
			return mkirbinop(GE, lhs, rhs);
		}
	// Binary composition operators
	case T_AND:
	case T_OR:
		if(fst.type != T_BOOL || snd.type != T_BOOL)
			PARSING_FAIL("Binary composition (&&, ||) works only on booleans");
		res->type = T_BOOL;
		return mkirbinop(token == T_AND ? AND : OR, lhs, rhs);
	// Binary or numeric equality operators
	case T_EQ: case T_NE:
		if((fst.type != T_INT && fst.type != T_BOOL)
		|| (snd.type != T_INT && snd.type != T_BOOL))
			PARSING_FAIL("Binary or numeric equality (==, !=) works only on booleans and integers");
		if(fst.type != snd.type)
			PARSING_FAIL("Binary or numeric equality (==, !=) can only work on two equal types");
		res->type = T_BOOL;
		return mkirbinop(token == T_EQ ? EQ : NE, lhs, rhs);
	// Numeric mathematical operators
	case '+': case '-': case '*': case '/': case '%':
		if(fst.type != T_INT || snd.type != T_INT)
			PARSING_FAIL("Numeric mathematics (+, -, *, /, %) work only on integers");
		res->type = T_INT;
		switch(token) {
			case '+':
				return mkirbinop(PLUS, lhs, rhs);
			case '-':
				return mkirbinop(MINUS, lhs, rhs);
			case '*':
				return mkirbinop(MUL, lhs, rhs);
			case '/':
				return mkirbinop(DIV, lhs, rhs);
			case '%':
				return mkirbinop(MOD, lhs, rhs);
		}
	// List composition operator
	case ':':
		if(snd.type != '[')
			PARSING_FAIL("List composition (:) needs a list on the right side");
		if(snd.list_type == 0 || snd.list_type->type != fst.type)
			PARSING_FAIL("List composition (:) types don't match");
		res->type = '[';
		res->list_type = malloc(sizeof(struct type));
		memcpy(res->list_type, &fst, sizeof(struct type));
		return mkirconst(0); // XXX
	default:
		fprintf(stderr, "Unexpected operator in Expression type checking: %s\n", token_to_string(t->fst_child->next->token->type));
		abort();
	}
}

DESCEND_FUNC(return) {
	assert(arg != NULL);
	assert(t->fst_child->type == 0 && t->fst_child->token->type == T_RETURN);
	assert(t->fst_child->next->next == NULL || t->fst_child->next->next->next == NULL);
	irexp *ret;

	struct tc_func *tc = arg;
	struct type newtype;
	if(t->fst_child->next->next == NULL) {
		assert(t->fst_child->next->type == 0 && t->fst_child->next->token->type == ';');
		newtype.type = T_VOID;
		ret = mkirconst(0);
	} else {
		assert(t->fst_child->next->next->type == 0 && t->fst_child->next->next->token->type == ';');
		ret = tc_descend_expression(tg, t->fst_child->next, &newtype);
	}
	unify_types(tg, tc->returntype, &newtype, NULL);
	return mkirret(ret);
}

struct irunit *
typechecker(synt_tree *t, grammar *gram) {
	int i;
	struct tc_globals tg;
	memset(&tg, 0, sizeof(struct tc_globals));
	tg.gram = gram;
	tg.funcall_rule = get_id_for_rule(gram, "FunCall");
	tg.funcs_last = &tg.funcs;
	rule_handlers = malloc((gram->lastrule + 1) * sizeof(descend_ft));
	for(i = 0; gram->lastrule >= i; i++) {
		rule_handlers[i] = tc_descend_simple;
	}
#define SET_RULE_HANDLER(rulename, func)	rule_handlers[get_id_for_rule(gram, #rulename)] = tc_descend_ ## func;
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
	SET_RULE_HANDLER(Stmt, stmt);
	SET_RULE_HANDLER(Stmt+, simple_seq);
	SET_RULE_HANDLER(Decl+, simple_seq);
	SET_RULE_HANDLER(VarDecl+, simple_seq);
	SET_RULE_HANDLER(S, init);
	return tc_descend(&tg, t, NULL);
}
