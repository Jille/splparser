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

static ssmlabel
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

static ssmlabel
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

static struct ssmline *
ssm_iterate_last(struct ssmline *line) {
	if(line == NULL)
		return NULL;
	while(line->next != NULL)
		line = line->next;
	return line;
}

static const char *
ssm_register_to_string(ssmregister reg) {
	switch(reg) {
	case PC: return "PC";
	case SP: return "SP";
	case MP: return "MP";
	case RR: return "RR";
	case R4: return "R4";
	case R5: return "R5";
	case R6: return "R6";
	case R7: return "R7";
	default:
		printf("Unknown register %d\n", reg);
		assert(0);
	}
}

static struct ssmline *
ssm_move_data(ssmregister dst, ssmregister src) {
	// XXX met dst=NONE en src=STACK wel poppen?
	if(dst == NONE || dst == src) {
		return NULL;
	}
	switch(src) {
		case STACK: ;
			struct ssmline *str = malloc(sizeof(struct ssmline));
			str->label = 0;
			// [2012-08-14 jille] Is STR hier de goede instructie voor?
			str->instr = SSTR;
			str->arg1.regval = dst;
			str->next = NULL;
			return str;
		case PC:
		case SP:
		case MP:
		case RR:
		case R4:
		case R5:
		case R6:
		case R7:
			if(dst == STACK) {
				assert(!"yet implemented");
			} else {
				struct ssmline *swprr = malloc(sizeof(struct ssmline));
				swprr->label = 0;
				swprr->instr = SSWPRR; // XXX LDRR is een copy ipv swap, dus beter?
				swprr->arg1.regval = dst;
				swprr->arg2.regval = src;
				swprr->next = NULL;
				return swprr;
			}
			break;
		case NONE:
			assert(!"reached");
	}
	assert(!"reached");
}

static struct ssmline *
ir_exp_to_ssm(struct irunit *ir, ssmregister reg) {
	// if reg == NONE, throw away result
	// if ret == STACK, push it on the stack
	struct ssmline *nop = malloc(sizeof(struct ssmline));
	nop->label = 0;
	nop->instr = SNOP;
	nop->next = 0;

	switch(ir->type) {
	case CONST:
		if(reg == NONE) {
			return nop;
		}
		// LDC: pushes the inline constant on the stack
		struct ssmline *ldc = malloc(sizeof(struct ssmline));
		ldc->label = 0;
		ldc->instr = SLDC;
		ldc->arg1.intval = ir->value;
		ldc->next = ssm_move_data(reg, STACK);
		return ldc;
	case NAME:
		return nop;
	case BINOP:
		return nop;
	case LOCAL: ;
		struct ssmline *ret = malloc(sizeof(struct ssmline));
		ret->label = 0;
		ret->instr = SLDL;
		ret->arg1.labelval = -ir->local;
		ret->next = ssm_move_data(reg, STACK);
		return ret;
	case GLOBAL:
	case FARG:
		return nop;
	case CALL: ;
		// TODO: don't scratch the RR register (push RR, CALL, SWAPR?)
		// TODO: parameters
		// TODO: what if the called function is void / returns nothing?
		struct ssmline *bsr = malloc(sizeof(struct ssmline));
		bsr->label = 0;
		bsr->instr = SBSR;
		bsr->arg1.labelval = ir->call.func;
		bsr->next = ssm_move_data(reg, RR);
		return bsr;
	case ESEQ:
		return nop;
	default:
		printf("Didn't expect IR type %d here\n", ir->type);
		assert(0 && "Didn't expect that IR type here");
	}
}

static struct ssmline *
ssm_return(irexp *retval) {
	struct ssmline *res = malloc(sizeof(struct ssmline));
	res->label = 0;
	res->instr = SUNLINK;
	res->next = malloc(sizeof(struct ssmline));
	res->next->label = 0;
	res->next->instr = SRET;
	res->next->next = NULL;
	if(retval != NULL) {
		struct ssmline *exp = ir_exp_to_ssm(retval, RR);
		ssm_iterate_last(exp)->next = res;
		return exp;
	}
	return res;
}

struct ssmline *
ir_to_ssm(struct irunit *ir) {
	struct ssmline *res;
	struct ssmline *exp;
	assert(ir != NULL);

	struct ssmline *nop = malloc(sizeof(struct ssmline));
	nop->label = 0;
	nop->instr = SNOP;
	nop->next = NULL;

	switch(ir->type) {
	case MOVE:; // Evaluate src and store the result in dst (LOCAL, GLOBAL or FARG)
		struct ssmline *ret;
		exp = ir_exp_to_ssm(ir->move.src, STACK);

		switch(ir->move.dst->type) {
			case LOCAL:
				// Store the result in a local variable
				ret = malloc(sizeof(struct ssmline));
				ret->label = 0;
				ret->instr = SSTL;
				ret->arg1.labelval = -ir->move.dst->local;
				ret->next = NULL;
				ssm_iterate_last(exp)->next = ret;
				break;
			case GLOBAL:
				return nop;
				break;
			case FARG:
				return nop;
				break;
			default:
				assert(!"reached");
		}
		return exp;
	case JUMP: // jump to address as result of expression
		//show_ir_tree(ir->jump.exp, indent);
		return nop;
	case CJUMP: // jump to label as result of relop
		res = malloc(sizeof(struct ssmline));
		res->label = 0;
		res->instr = SBRF;
		res->arg1.labelval = get_ssmlabel_from_irlabel(ir->cjump.iffalse);
		res->next = NULL;
		exp = ir_exp_to_ssm(ir->cjump.exp, STACK);
		ssm_iterate_last(exp)->next = res;
		return exp;
	case SEQ: // Statement "left" followed by "right"
		// Unless "left" is a label or function declaration, in which
		// case it simply sets the label of "right"
		if(ir->seq.left->type == LABEL) {
			res = ir_to_ssm(ir->seq.right);
			res->label = get_ssmlabel_from_irlabel(ir->seq.left->label);
			return res;
		} else if(ir->seq.left->type == FUNC) {
			// reserve memory for locals: LINK, UNLINK
			// look forward how many locals will be used in this function alone
			res = malloc(sizeof(struct ssmline));
			res->label = get_ssmlabel_from_irfunc(ir->seq.left->func.funcid);
			res->instr = SLINK;
			res->arg1.intval = ir->seq.left->func.vars;
			res->next = ir_to_ssm(ir->seq.right);
			// Add RET to the end of the function if it's not there yet
			if(ssm_iterate_last(res)->instr != SRET) {
				ssm_iterate_last(res)->next = ssm_return(NULL);
			}
			return res;
		} else {
			struct ssmline *first = ir_to_ssm(ir->seq.left);
			struct ssmline *second = ir_to_ssm(ir->seq.right);
			ssm_iterate_last(first)->next = second;
			return first;
		}
	case RET:
		return ssm_return(ir->ret);
	case EXP: // evaluate expression, throw away result
		return ir_exp_to_ssm(ir->exp, 0);
	case TRAP:
		res = malloc(sizeof(struct ssmline));
		res->label = 0;
		res->instr = STRAP;
		res->arg1.intval = ir->syscall;
		res->next = NULL;
		return res;
	case HALT:
		res = malloc(sizeof(struct ssmline));
		res->label = 0;
		res->instr = SHALT;
		res->next = NULL;
		return res;
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
		case SNOP:  printf("NOP"); break;
		case SHALT: printf("HALT"); break;
		case SRET:  printf("RET"); break;
		case SUNLINK:  printf("UNLINK"); break;
		// integer parameter
		case SLDC:  printf("LDC %d", ssm->arg1.intval); break;
		case SLINK:  printf("LINK %d", ssm->arg1.intval); break;
		case STRAP:  printf("TRAP %d", ssm->arg1.intval); break;
		case SLDL:  printf("SLDL %d", ssm->arg1.intval); break;
		case SSTL:  printf("SSTL %d", ssm->arg1.intval); break;
		// label parameter
		case SBRA:  printf("BRA lbl%04d", ssm->arg1.labelval); break;
		case SBSR:  printf("BSR lbl%04d", ssm->arg1.labelval); break;
		case SBRF:  printf("BRF lbl%04d", ssm->arg1.labelval); break;
		// register parameter
		case SSTR:  printf("STR %s", ssm_register_to_string(ssm->arg1.regval)); break;
		case SSWPRR:printf("SWPRR %s %s", ssm_register_to_string(ssm->arg1.regval), ssm_register_to_string(ssm->arg2.regval)); break;
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
