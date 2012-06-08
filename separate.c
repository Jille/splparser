#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "generator.h"
#include "scanner.h"
#include "grammar.h"
#include "parser.h"
#include "separate.h"

static void my_fscanf(FILE *stream, const char *format, ...);
static synt_tree *descend_search(synt_tree *t, int rule);

synt_tree *
merge_synt_trees(grammar *g, synt_tree *left, synt_tree *right) {
	synt_tree *lelem = descend_search(left, get_id_for_rule(g, "FunDecl+"));
	synt_tree *relem = descend_search(right, get_id_for_rule(g, "FunDecl+"));
	assert(lelem != NULL);
	assert(relem != NULL);

	if(lelem->fst_child == NULL) {
		lelem->fst_child = relem->fst_child;
	} else {
		synt_tree *bla = lelem->fst_child;
		while(bla->next != NULL) {
			bla = bla->next;
		}
		bla->next = relem;
		relem->next = NULL;
	}

	lelem = descend_search(left, get_id_for_rule(g, "GlobalVars"));
	relem = descend_search(right, get_id_for_rule(g, "GlobalVars"));

	if(lelem != NULL && relem != NULL) {
		if(lelem->fst_child == NULL) {
			lelem->fst_child = relem->fst_child;
		} else {
			synt_tree *bla = lelem->fst_child;
			while(bla->next != NULL) {
				bla = bla->next;
			}
			bla->next = relem;
			relem->next = NULL;
		}
	} else if(relem != NULL) {
		lelem = descend_search(left, get_id_for_rule(g, "Decl"));
		if(relem->fst_child != NULL) {
			synt_tree *bla2 = relem->fst_child;
			while(bla2->next != NULL) {
				bla2 = bla2->next;
			}
			bla2->next = lelem->fst_child;
		} else {
			relem->fst_child = lelem->fst_child;
		}
		lelem->fst_child = relem;
	}

	return left;
}

static synt_tree *
descend_search(synt_tree *t, int rule) {
	synt_tree *child = t->fst_child;
	while(child != NULL) {
		if(child->type == 1) {
			if(child->rule == rule) {
				return child;
			}
			synt_tree *ret = descend_search(child, rule);
			if(ret != NULL) {
				return ret;
			}
		}
		child = child->next;
	}
	return NULL;
}

static void
_write_synt_tree(FILE *fh, synt_tree *t) {
tail_recurse:
	if(t->type == 0) {
		t->rule = -1;
	}
	fprintf(fh, "%04d", t->rule);
	if(t->type == 1) {
		_write_synt_tree(fh, t->fst_child);
		fprintf(fh, "%04d", -2);
	} else {
		fprintf(fh, "%04d%04d", t->token->type, t->token->lineno);
		switch(t->token->type) {
			case T_WORD:
				fprintf(fh, "%04d%s", strlen(t->token->value.sval), t->token->value.sval);
				break;
			case T_NUMBER:
				fprintf(fh, "%04d", t->token->value.ival);
				break;
		}
	}
	t = t->next;
	if(t) {
		goto tail_recurse;
	}
}

synt_tree *
read_synt_tree_fh(FILE *fh) {
	if(feof(fh)) {
		return NULL;
	}
	synt_tree *ret = malloc(sizeof(synt_tree));
	my_fscanf(fh, "%04d", &ret->rule);
	switch(ret->rule) {
		case -1:
			ret->type = 0;
			ret->token = malloc(sizeof(struct token));
			my_fscanf(fh, "%04d%04d", &ret->token->type, &ret->token->lineno);
			switch(ret->token->type) {
				case T_WORD: ;
					int len;
					my_fscanf(fh, "%04d", &len);
					ret->token->value.sval = malloc(len+1);
					fread(ret->token->value.sval, 1, len, fh);
					ret->token->value.sval[len] = 0;
					break;
				case T_NUMBER:
					my_fscanf(fh, "%04d", &ret->token->value.ival);
					break;
			}
			break;
		case -2:
			free(ret);
			return NULL;
		default:
			ret->type = 1;
			ret->fst_child = read_synt_tree_fh(fh);
	}
	ret->next = read_synt_tree_fh(fh);
	return ret;
}

synt_tree *
read_synt_tree(char *file) {
	FILE *fh = fopen(file, "r");
	if(fh == NULL) {
		char *error;
		asprintf(&error, "Could not open syntax tree %s", file);
		perror(error);
		free(error);
		return NULL;
	}
	synt_tree *t = read_synt_tree_fh(fh);
	fclose(fh);
	return t;
}

void
write_synt_tree(char *file, synt_tree *t) {
	FILE *fh = fopen(file, "w");
	_write_synt_tree(fh, t);
	fclose(fh);
}

static void
my_fscanf(FILE *fh, const char *format, ...) {
	int n = 0;
	const char *s = format;
	while(*s) {
		if(*s++ == '%') {
			n++;
		}
	}
	va_list ap;
	va_start(ap, format);
	if(vfscanf(fh, format, ap) != n) {
		abort();
	}
	va_end(ap);
}
