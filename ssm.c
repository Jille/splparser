#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "ssm.h"
#include "ir.h"

// Ordered mapping of irlabels to ssmlabels
struct ssmlabelmapping {
	ssmlabel ssm;
	irlabel ir;
	struct ssmlabelmapping *next;
};
// Ordered mapping of irfuncs to ssmlabels
struct ssmfuncmapping {
	ssmlabel ssm;
	irfunc ir;
	struct ssmfuncmapping *next;
};

static struct ssmlabelmapping *ssmlabels = NULL;
static struct ssmfuncmapping *ssmfuncs = NULL;
static ssmlabel ssmlabelptr = 0;

ssmlabel
get_ssmlabel_from_irlabel(irlabel ir) {
	if(ssmlabels != NULL && ssmlabels->ir == ir)
		return ssmlabels->ssm;
	if(ssmlabels == NULL || ssmlabels->ir > ir) {
		struct ssmlabelmapping *newfst = malloc(sizeof(struct ssmlabelmapping));
		newfst->ssm = ++ssmlabelptr;
		newfst->ir = ir;
		newfst->next = ssmlabels;
		ssmlabels = newfst;
		return ssmlabels->ssm;
	}
	// find the first mapping whose irlabel >= ir
	struct ssmlabelmapping *it = ssmlabels;
	while(it->next != NULL) {
		if(it->next->ir == ir)
			return it->next->ssm;
		if(it->next->ir > ir)
			break;
		it = it->next;
	}
	struct ssmlabelmapping *newthis = malloc(sizeof(struct ssmlabelmapping));
	newthis->ssm = ++ssmlabelptr;
	newthis->ir = ir;
	newthis->next = it->next;
	it->next = newthis;
	return newthis->ssm;
}

ssmlabel
get_ssmlabel_from_irfunc(irfunc ir) {
	if(ssmfuncs != NULL && ssmfuncs->ir == ir)
		return ssmfuncs->ssm;
	if(ssmfuncs == NULL || ssmfuncs->ir > ir) {
		struct ssmfuncmapping *newfst = malloc(sizeof(struct ssmfuncmapping));
		newfst->ssm = ++ssmlabelptr;
		newfst->ir = ir;
		newfst->next = ssmfuncs;
		ssmfuncs = newfst;
		return ssmfuncs->ssm;
	}
	// find the first mapping whose irlabel >= ir
	struct ssmfuncmapping *it = ssmfuncs;
	while(it->next != NULL) {
		if(it->next->ir == ir)
			return it->next->ssm;
		if(it->next->ir > ir)
			break;
		it = it->next;
	}
	struct ssmfuncmapping *newthis = malloc(sizeof(struct ssmfuncmapping));
	newthis->ssm = ++ssmlabelptr;
	newthis->ir = ir;
	newthis->next = it->next;
	it->next = newthis;
	return newthis->ssm;
}

struct ssmline *
ssm_iterate_last(struct ssmline *line) {
	if(line == NULL)
		return NULL;
	while(line->next != NULL)
		line = line->next;
	return line;
}

struct ssmline *
ir_exp_to_ssm(struct irunit *ir, int reg) {
	// if reg == 0, throw away result
	struct ssmline *nop = malloc(sizeof(struct ssmline));
	nop->label = 0;
	nop->instr = NOP;
	nop->next = 0;

	switch(ir->type) {
	case CONST:
		return nop;
	case NAME:
		return nop;
	case TEMP:
		return nop;
	case BINOP:
		return nop;
	case MEM:
		return nop;
	case CALL:
		return nop;
	case ESEQ:
		return nop;
	default:
		printf("Didn't expect IR type %d here\n", ir->type);
		assert(0 && "Didn't expect that IR type here");
	}
}

struct ssmline *
ir_to_ssm(struct irunit *ir) {
	assert(ir != NULL);

	struct ssmline *nop = malloc(sizeof(struct ssmline));
	nop->label = 0;
	nop->instr = NOP;
	nop->next = 0;

	switch(ir->type) {
	case MOVE: // Evaluate src and store the result in dst (TEMP or MEM)
		assert(ir->move.dst->type == TEMP || ir->move.dst->type == MEM);

		//struct ssmline *ir

		if(ir->move.dst->type == TEMP) {
			// Store the result in a temporary variable
			// TODO
		} else {
			// Store the result in a memory location
			// TODO
		}
		return nop;
		/*show_ir_tree(ir->move.dst, indent);
		show_ir_tree(ir->move.src, indent);
		break;*/
	case JUMP: // jump to address as result of expression
		//show_ir_tree(ir->jump.exp, indent);
		return nop;
	case CJUMP: // jump to label as result of relop
		/*show_ir_tree(ir->cjump.left, indent);
		printf("%s\n", irop_to_string(ir->cjump.op, 1));
		show_ir_tree(ir->cjump.right, indent);
		printf("if true: %d\n", ir->cjump.iftrue);
		printf("if false: %d\n", ir->cjump.iffalse);
		break;*/
		return nop;
	case SEQ: // Statement "left" followed by "right"
		// Unless "left" is a label or function declaration, in which
		// case it simply sets the label of "right"
		if(ir->seq.left->type == LABEL) {
			struct ssmline *res = ir_to_ssm(ir->seq.right);
			res->label = get_ssmlabel_from_irlabel(ir->seq.left->label);
			return res;
		} else if(ir->seq.left->type == FUNC) {
			struct ssmline *res = ir_to_ssm(ir->seq.right);
			res->label = get_ssmlabel_from_irfunc(ir->seq.left->func);
			return res;
		} else {
			struct ssmline *first = ir_to_ssm(ir->seq.left);
			struct ssmline *second = ir_to_ssm(ir->seq.right);
			ssm_iterate_last(first)->next = second;
			return first;
		}
	case RET: ;
		// TODO: clean up what FUNC added earlier
		struct ssmline *res = malloc(sizeof(struct ssmline));
		res->label = 0;
		res->instr = RET;
		res->next = 0;
		return res;
	case EXP: // evaluate expression, throw away result
		return ir_exp_to_ssm(ir->exp, 0);
	default:
		printf("Didn't expect IR type %d here\n", ir->type);
		assert(0 && "Didn't expect that IR type here");
	}
}

void
write_ssm(struct ssmline *ssm, FILE *fd) {
	while(ssm != NULL) {
		if(ssm->label == 0) {
			printf("         ");
		} else {
			printf("lbl%04d: ", ssm->label);
		}

		switch(ssm->instr) {
		// no parameters
		case NOP:  printf("NOP"); break;
		case HALT: printf("HALT"); break;
		case RET:  printf("RET"); break;
		// label parameters
		case BRA:  printf("BRA lbl%04d", ssm->arg1); break;
		default:
			printf("Unknown instruction %d\n", ssm->instr);
			assert(0);
		}

		printf("\n");
		ssm = ssm->next;
	}
}

void
show_ssm(struct ssmline *ssm) {
	write_ssm(ssm, stdout);
}
