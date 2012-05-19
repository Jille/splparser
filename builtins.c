#include <stdio.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "parser.h"
#include "grammar.h"
#include "ir.h"
#include "types.h"
#include "ssm.h"

irfunc builtin_head, builtin_tail, builtin_isempty;

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
}

void
ssm_builtin_functions() {
	printf("lbl%04d: LDS -1 ; Builtin function head()\n", get_ssmlabel_from_irfunc(builtin_head));
	printf("         LDA 0\n");
	printf("         STR RR\n");
	printf("         RET\n");
	printf("lbl%04d: LDS -1 ; Builtin function tail()\n", get_ssmlabel_from_irfunc(builtin_tail));
	printf("         LDA 1\n");
	printf("         STR RR\n");
	printf("         RET\n");
	printf("lbl%04d: LDS -1 ; Builtin function isempty()\n", get_ssmlabel_from_irfunc(builtin_isempty));
	printf("         LDA 1\n");
	printf("         LDC 0\n");
	printf("         NE\n");
	printf("         STR RR\n");
	printf("         RET\n");
}
