#include <stdio.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "parser.h"
#include "grammar.h"
#include "ir.h"
#include "types.h"
#include "ssm.h"

static irfunc builtin_head, builtin_tail, builtin_isempty, builtin_fst, builtin_snd;

void
init_builtin_functions(struct tc_globals *tg) {
	struct tc_func *head = calloc(1, sizeof(struct tc_func));
	head->returntype = malloc(sizeof(struct type));
	head->returntype->type = T_WORD;
	head->returntype->pm_name = "t";
	head->returntype->accept_empty_list = 0; // ?
	head->name = "head";
	head->args = malloc(sizeof(struct func_arg));
	head->args->name = "list";
	head->args->type = malloc(sizeof(struct type));
	head->args->type->type = '[';
	head->args->type->list_type = malloc(sizeof(struct type));
	head->args->type->list_type->type = T_WORD;
	head->args->type->list_type->pm_name = "t";
	head->args->type->list_type->accept_empty_list = 0; // ?
	head->args->farg = 0;
	head->args->next = NULL;
	head->decls = NULL;
	head->func = getfunc();
	head->nargs = 1;
	head->nlocals = 0;
	head->next = NULL;

	builtin_head = head->func;
	*tg->funcs_last = head;
	tg->funcs_last = &head->next;

	struct tc_func *tail = calloc(1, sizeof(struct tc_func));
	tail->returntype = malloc(sizeof(struct type));
	tail->returntype->type = '[';
	tail->returntype->list_type = malloc(sizeof(struct type));
	tail->returntype->list_type->type = T_WORD;
	tail->returntype->list_type->pm_name = "t";
	tail->returntype->list_type->accept_empty_list = 0; // ?
	tail->name = "tail";
	tail->args = malloc(sizeof(struct func_arg));
	tail->args->name = "list";
	tail->args->type = malloc(sizeof(struct type));
	tail->args->type->type = '[';
	tail->args->type->list_type = malloc(sizeof(struct type));
	tail->args->type->list_type->type = T_WORD;
	tail->args->type->list_type->pm_name = "t";
	tail->args->type->list_type->accept_empty_list = 0; // ?
	tail->args->farg = 0;
	tail->args->next = NULL;
	tail->decls = NULL;
	tail->func = getfunc();
	tail->nargs = 1;
	tail->nlocals = 0;
	tail->next = NULL;

	builtin_tail = tail->func;
	*tg->funcs_last = tail;
	tg->funcs_last = &tail->next;

	struct tc_func *isempty = calloc(1, sizeof(struct tc_func));
	isempty->returntype = malloc(sizeof(struct type));
	isempty->returntype->type = T_BOOL;
	isempty->name = "isempty";
	isempty->args = malloc(sizeof(struct func_arg));
	isempty->args->name = "list";
	isempty->args->type = malloc(sizeof(struct type));
	isempty->args->type->type = '[';
	isempty->args->type->list_type = malloc(sizeof(struct type));
	isempty->args->type->list_type->type = T_WORD;
	isempty->args->type->list_type->pm_name = "t";
	isempty->args->type->list_type->accept_empty_list = 0; // ?
	isempty->args->farg = 0;
	isempty->args->next = NULL;
	isempty->decls = NULL;
	isempty->func = getfunc();
	isempty->nargs = 1;
	isempty->nlocals = 0;
	isempty->next = NULL;

	builtin_isempty = isempty->func;
	*tg->funcs_last = isempty;
	tg->funcs_last = &isempty->next;

	struct tc_func *fst = calloc(1, sizeof(struct tc_func));
	fst->returntype = malloc(sizeof(struct type));
	fst->returntype->type = T_WORD;
	fst->returntype->pm_name = "f";
	fst->returntype->accept_empty_list = 0;
	fst->name = "fst";
	fst->args = malloc(sizeof(struct func_arg));
	fst->args->name = "tuple";
	fst->args->type = malloc(sizeof(struct type));
	fst->args->type->type = '(';
	fst->args->type->fst_type = malloc(sizeof(struct type));
	fst->args->type->fst_type->type = T_WORD;
	fst->args->type->fst_type->pm_name = "f";
	fst->args->type->fst_type->accept_empty_list = 0;
	fst->args->type->snd_type = malloc(sizeof(struct type));
	fst->args->type->snd_type->type = T_WORD;
	fst->args->type->snd_type->pm_name = "s";
	fst->args->type->snd_type->accept_empty_list = 0;
	fst->args->farg = 0;
	fst->args->next = NULL;
	fst->decls = NULL;
	fst->func = getfunc();
	fst->nargs = 1;
	fst->nlocals = 0;
	fst->next = NULL;

	builtin_fst = fst->func;
	*tg->funcs_last = fst;
	tg->funcs_last = &fst->next;

	struct tc_func *snd = calloc(1, sizeof(struct tc_func));
	snd->returntype = malloc(sizeof(struct type));
	snd->returntype->type = T_WORD;
	snd->returntype->pm_name = "s";
	snd->returntype->accept_empty_list = 0;
	snd->name = "snd";
	snd->args = malloc(sizeof(struct func_arg));
	snd->args->name = "tuple";
	snd->args->type = malloc(sizeof(struct type));
	snd->args->type->type = '(';
	snd->args->type->fst_type = malloc(sizeof(struct type));
	snd->args->type->fst_type->type = T_WORD;
	snd->args->type->fst_type->pm_name = "f";
	snd->args->type->fst_type->accept_empty_list = 0;
	snd->args->type->snd_type = malloc(sizeof(struct type));
	snd->args->type->snd_type->type = T_WORD;
	snd->args->type->snd_type->pm_name = "s";
	snd->args->type->snd_type->accept_empty_list = 0;
	snd->args->farg = 0;
	snd->args->next = NULL;
	snd->decls = NULL;
	snd->func = getfunc();
	snd->nargs = 1;
	snd->nlocals = 0;
	snd->next = NULL;

	builtin_snd = snd->func;
	*tg->funcs_last = snd;
	tg->funcs_last = &snd->next;
}

void
ssm_builtin_functions(FILE *fh) {
	fprintf(fh, "lbl%04d: LDS -1 ; Builtin function head()\n", get_ssmlabel_from_irfunc(builtin_head));
	fprintf(fh, "         LDA 0\n");
	fprintf(fh, "         STR RR\n");
	fprintf(fh, "         RET\n");
	fprintf(fh, "lbl%04d: LDS -1 ; Builtin function tail()\n", get_ssmlabel_from_irfunc(builtin_tail));
	fprintf(fh, "         LDA 1\n");
	fprintf(fh, "         STR RR\n");
	fprintf(fh, "         RET\n");
	fprintf(fh, "lbl%04d: LDS -1 ; Builtin function isempty()\n", get_ssmlabel_from_irfunc(builtin_isempty));
	fprintf(fh, "         LDC 0\n");
	fprintf(fh, "         EQ\n");
	fprintf(fh, "         STR RR\n");
	fprintf(fh, "         RET\n");
	fprintf(fh, "lbl%04d: LDS -1 ; Builtin function fst()\n", get_ssmlabel_from_irfunc(builtin_fst));
	fprintf(fh, "         LDA 0\n");
	fprintf(fh, "         STR RR\n");
	fprintf(fh, "         RET\n");
	fprintf(fh, "lbl%04d: LDS -1 ; Builtin function snd()\n", get_ssmlabel_from_irfunc(builtin_snd));
	fprintf(fh, "         LDA 1\n");
	fprintf(fh, "         STR RR\n");
	fprintf(fh, "         RET\n");
}

void
c_builtin_functions(FILE *fd) {
	fprintf(fd, "spltype f%d /* head */(spltype list) {\n", builtin_head);
	fputs("	return list.lval->value;\n", fd);
	fputs("}\n", fd);
	fprintf(fd, "spltype f%d /* tail */(spltype list) {\n", builtin_tail);
	fputs("	return (spltype)list.lval->next;\n", fd);
	fputs("}\n", fd);
	fprintf(fd, "spltype f%d /* isempty */(spltype list) {\n", builtin_isempty);
	fputs("	return (spltype)(list.lval->next == NULL);\n", fd);
	fputs("}\n", fd);
	fprintf(fd, "spltype f%d /* fst */(spltype list) {\n", builtin_fst);
	fputs("	return list.tval->fst;\n", fd);
	fputs("}\n", fd);
	fprintf(fd, "spltype f%d /* snd */(spltype list) {\n", builtin_snd);
	fputs("	return list.tval->snd;\n", fd);
	fputs("}\n", fd);
}
