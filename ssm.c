#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "parser.h"
#include "grammar.h"
#include "ir.h"
#include "types.h"
#include "ssm.h"
#include "builtins.h"

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
static ssmlabel heaplabel = 0;
static int args_for_current_function = -1;

static struct ssmline *
alloc_ssmline(ssminstr instr) {
	struct ssmline *ssm = malloc(sizeof(struct ssmline));
	ssm->label = 0;
	ssm->instr = instr;
	ssm->comment = NULL;
	ssm->next = NULL;
	return ssm;
}

static void
ssmline_set_comment_ir(struct ssmline *ssm, struct irunit *ir) {
	// XXX show_ir_tree() ombouwen dat hij een string returned
	asprintf(&ssm->comment, "IR %d", ir->type);
}

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
	case HP: return "HP";
	case RR: return "RR";
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
	if(dst == src) {
		return NULL;
	}
	switch(src) {
		case STACK: ;
			if(dst == NONE) {
				struct ssmline *ajs = alloc_ssmline(SAJS);
				ajs->arg1.intval = -1;
				ajs->comment = "Move data from STACK to NONE";
				return ajs;
			}
			struct ssmline *str = alloc_ssmline(SSTR);
			str->arg1.regval = dst;
			return str;
		case PC:
		case SP:
		case MP:
		case HP:
		case RR:
		case R5:
		case R6:
		case R7:
			if(dst == NONE) {
				return NULL;
			} else if(dst == STACK) {
				struct ssmline *ldr = alloc_ssmline(SLDR);
				ldr->arg1.regval = src;
				return ldr;
			} else {
				struct ssmline *ldrr = alloc_ssmline(SLDRR);
				ldrr->arg1.regval = dst;
				ldrr->arg2.regval = src;
				return ldrr;
			}
			break;
		case NONE:
			assert(!"reached");
	}
	assert(!"reached");
}

static struct ssmline *
chain_ssmlines(struct ssmline **lines) {
	int i = 0;
	while(lines[i] != NULL) {
		lines[i]->next = lines[i+1];
		i++;
	}
	return lines[0];
}

static struct ssmline *
ir_exp_to_ssm(struct irunit *ir, ssmregister reg) {
	// if reg == NONE, throw away result
	// if ret == STACK, push it on the stack

	switch(ir->type) {
	case CONST:
		if(reg == NONE) {
			struct ssmline *nop = alloc_ssmline(SNOP);
			nop->comment = "exp_to_ssm: put CONST in NONE";
			return nop;
		}
		// LDC: pushes the inline constant on the stack
		struct ssmline *ldc = alloc_ssmline(SLDC);
		ldc->arg1.intval = ir->value;
		ldc->next = ssm_move_data(reg, STACK);
		return ldc;
	case BINOP: {
		struct ssmline *left = ir_exp_to_ssm(ir->binop.left, STACK);
		struct ssmline *right = ir_exp_to_ssm(ir->binop.right, STACK);
		struct ssmline *op;
		switch(ir->binop.op) {
#define CONVERT_BINOP_TO_SSMLINE(irop) case irop: op = alloc_ssmline(S ## irop); break
			case PLUS:
				op = alloc_ssmline(SADD);
				break;
			case MINUS:
				op = alloc_ssmline(SSUB);
				break;
			case EQ:
				op = alloc_ssmline(S_EQ);
				break;
			CONVERT_BINOP_TO_SSMLINE(MUL);
			CONVERT_BINOP_TO_SSMLINE(DIV);
			CONVERT_BINOP_TO_SSMLINE(MOD);
			CONVERT_BINOP_TO_SSMLINE(AND);
			CONVERT_BINOP_TO_SSMLINE(OR);
			CONVERT_BINOP_TO_SSMLINE(XOR);
			CONVERT_BINOP_TO_SSMLINE(NE);
			CONVERT_BINOP_TO_SSMLINE(LT);
			CONVERT_BINOP_TO_SSMLINE(GT);
			CONVERT_BINOP_TO_SSMLINE(LE);
			CONVERT_BINOP_TO_SSMLINE(GE);
			case LSHIFT:
			case RSHIFT:
			default:
				assert(!"reached");
		}
		ssm_iterate_last(left)->next = right;
		ssm_iterate_last(right)->next = op;
		ssm_iterate_last(op)->next = ssm_move_data(reg, STACK);
		return left;
	}
	case LOCAL: {
		struct ssmline *ret = alloc_ssmline(SLDL);
		ret->arg1.intval = ir->local + 1;
		ret->next = ssm_move_data(reg, STACK);
		asprintf(&ret->comment, "Fetch local %d", ir->local);
		return ret;
	}
	case GLOBAL: {
		struct ssmline *ret = alloc_ssmline(SLDR);
		ret->arg1.regval = R5;
		ret->next = alloc_ssmline(SLDA);
		ret->next->arg1.intval = ir->global;
		ret->next->next = ssm_move_data(reg, STACK);
		asprintf(&ret->comment, "Fetch global %d", ir->global);
		return ret;
	}
	case FARG: {
		struct ssmline *ret = alloc_ssmline(SLDL);
		assert(args_for_current_function > 0);
		ret->arg1.intval = -args_for_current_function + ir->farg - 2; // XXX klopt dit?
		ret->next = ssm_move_data(reg, STACK);
		ret->comment = "fetch FARG";
		asprintf(&ret->comment, "Fetch FARG %d", ir->farg);
		return ret;
	}
	case CALL: ;
		// [2012-05-xx sjors] don't scratch the RR register (push RR, CALL, SWAPR?)
		// [2012-05-18 jille] Dat kan wel denk ik eigenlijk. Verder, als we dat niet zouden willen kunnen waarschijnlijk beter onze callingconventie aanpassen dat we via de stack returnen
		// [2012-05-xx sjors]: what if the called function is void / returns nothing?
		// [2012-05-18 jille] Dan returnen we stiekem 0
		struct irexplist *args = ir->call.args;
		struct ssmline *ret = NULL, *prev;
		int nargs = 0;
		while(args != NULL) {
			struct ssmline *exp = ir_exp_to_ssm(args->exp, STACK);
			if(ret == NULL) {
				ret = exp;
			} else {
				ssm_iterate_last(prev)->next = exp;
			}
			prev = exp;
			args = args->next;
			nargs++;
		}
		struct ssmline *bsr = alloc_ssmline(SBSR);
		bsr->arg1.labelval = get_ssmlabel_from_irfunc(ir->call.func);
		if(ret != NULL) {
			assert(ssm_iterate_last(prev) == ssm_iterate_last(ret));
			ssm_iterate_last(prev)->next = bsr;
			struct ssmline *ajs = alloc_ssmline(SAJS);
			ajs->arg1.intval = -nargs;
			ssm_iterate_last(bsr)->next = ajs;
			ajs->next = ssm_move_data(reg, RR);
			return ret;
		}
		bsr->next = ssm_move_data(reg, RR);
		return bsr;
	case LISTEL:
	case TUPLE: {
		/*
			<fetch exp>
			<fetch next>
			LDR HP
			STMA 0 2
			LDR HP
			LDC 2
			ADD
			STR HP
		*/
		struct ssmline *exp, *next; // ookwel *fst en *snd, bij TUPLE's
		if(ir->type == LISTEL) {
			exp = ir_exp_to_ssm(ir->listel.exp, STACK);
			next = ir_exp_to_ssm(ir->listel.next, STACK);
		} else {
			exp = ir_exp_to_ssm(ir->tuple.fst, STACK);
			next = ir_exp_to_ssm(ir->tuple.snd, STACK);
		}
		struct ssmline *ret[8];
		ret[0] = alloc_ssmline(SLDR);
		ret[0]->arg1.regval = HP;
		ret[1] = alloc_ssmline(SSTMA);
		ret[1]->arg1.intval = 0;
		ret[1]->arg2.intval = 2;
		ret[2] = ssm_move_data(reg, HP);
		if(ret[2] == NULL) {
			// Dit is niet heel mooi, maar chain_ssmlines kan (nog?) niet overweg met NULL-values ertussen
			ret[2] = alloc_ssmline(SNOP);
			ret[2]->comment = "resultaat is niet nodig.";
		}
		ret[3] = alloc_ssmline(SLDR);
		ret[3]->arg1.regval = HP;
		ret[4] = alloc_ssmline(SLDC);
		ret[4]->arg1.intval = 2;
		ret[5] = alloc_ssmline(SADD);
		ret[6] = alloc_ssmline(SSTR);
		ret[6]->arg1.regval = HP;
		ret[7] = NULL;
		ssm_iterate_last(exp)->next = next;
		ssm_iterate_last(next)->next = chain_ssmlines(ret);
		return exp;
	}
	case ESEQ:
		assert(!"implemented");
		break;
	default:
		printf("Didn't expect IR type %d here\n", ir->type);
		assert(0 && "Didn't expect that IR type here");
	}
}

static struct ssmline *
ssm_return(irexp *retval) {
	struct ssmline *res = alloc_ssmline(SUNLINK);
	res->next = alloc_ssmline(SRET);
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

	switch(ir->type) {
	case MOVE:; // Evaluate src and store the result in dst (LOCAL, GLOBAL or FARG)
		struct ssmline *ret;
		exp = ir_exp_to_ssm(ir->move.src, STACK);

		switch(ir->move.dst->type) {
			case LOCAL:
				// Store the result in a local variable
				ret = alloc_ssmline(SSTL);
				ret->arg1.intval = ir->move.dst->local + 1;
				asprintf(&ret->comment, "Store in local %d", ir->move.dst->local);
				ssm_iterate_last(exp)->next = ret;
				break;
			case GLOBAL:
				ret = alloc_ssmline(SLDR);
				ret->arg1.regval = R5;
				ret->next = alloc_ssmline(SSTA);
				ret->next->arg1.intval = ir->move.dst->global;
				asprintf(&ret->comment, "Store in global %d", ir->move.dst->global);
				ssm_iterate_last(exp)->next = ret;
				break;
			case FARG:
				ret = alloc_ssmline(SSTL);
				ret->arg1.intval = -args_for_current_function + ir->move.dst->farg - 2;
				asprintf(&ret->comment, "Store in FARG %d", ir->move.dst->farg);
				ssm_iterate_last(exp)->next = ret;
				break;
			default:
				assert(!"reached");
		}
		exp->comment = "Move";
		return exp;
	case JUMP: // jump to address as result of expression
		res = alloc_ssmline(SBRA);
		res->arg1.labelval = get_ssmlabel_from_irlabel(ir->jump);
		return res;
	case CJUMP: // jump to label as result of relop
		res = alloc_ssmline(SBRF);
		res->arg1.labelval = get_ssmlabel_from_irlabel(ir->cjump.iffalse);
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
			res = alloc_ssmline(SLINK);
			res->label = get_ssmlabel_from_irfunc(ir->seq.left->func.funcid);
			res->arg1.intval = ir->seq.left->func.vars;
			asprintf(&res->comment, "Function with %d argument(s)", ir->seq.left->func.args);
			args_for_current_function = ir->seq.left->func.args;
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
		return ir_exp_to_ssm(ir->exp, NONE);
	case TRAP:
		exp = ir_exp_to_ssm(ir->trap.arg, STACK);
		res = alloc_ssmline(STRAP);
		res->arg1.intval = ir->trap.syscall;
		ssm_iterate_last(exp)->next = res;
		return exp;
	case HALT:
		return alloc_ssmline(SHALT);
	case LABEL: ;
		// XXX [2012-05-18 jille] Sjors, dit is lelijk. Je moet die SEQ echt niet zo uitwerken.
		struct ssmline *nop = alloc_ssmline(SNOP);
		nop->label = get_ssmlabel_from_irlabel(ir->label);
		nop->comment = "Gewoon een label";
		return nop;
	case GINIT: ;
		struct ssmline *lines[7];
		lines[0] = alloc_ssmline(SLDR);
		lines[0]->arg1.regval = HP;
		lines[1] = alloc_ssmline(SSTR);
		lines[1]->arg1.regval = R5;
		lines[2] = alloc_ssmline(SLDR);
		lines[2]->arg1.regval = HP;
		lines[3] = alloc_ssmline(SLDC);
		lines[3]->arg1.intval = ir->nglobals;
		lines[4] = alloc_ssmline(SADD);
		lines[5] = alloc_ssmline(SSTR);
		lines[5]->arg1.regval = HP;
		lines[6] = NULL;
		return chain_ssmlines(lines);
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
#define INSTR_VOID(instr)	case S ## instr: printf(#instr); break
		INSTR_VOID(NOP);
		INSTR_VOID(HALT);
		INSTR_VOID(RET);
		INSTR_VOID(UNLINK);
		INSTR_VOID(ADD);
		INSTR_VOID(SUB);
		INSTR_VOID(MUL);
		INSTR_VOID(DIV);
		INSTR_VOID(MOD);
		INSTR_VOID(AND);
		INSTR_VOID(OR);
		INSTR_VOID(XOR);
		case S_EQ:  printf("EQ"); break;
		INSTR_VOID(NE);
		INSTR_VOID(LT);
		INSTR_VOID(GT);
		INSTR_VOID(LE);
		INSTR_VOID(GE);

		// integer parameter
#define INSTR_INT(instr)	case S ## instr: printf(#instr " %d", ssm->arg1.intval); break
		INSTR_INT(LDC);
		INSTR_INT(LINK);
		INSTR_INT(TRAP);
		INSTR_INT(LDL);
		INSTR_INT(STL);
		INSTR_INT(AJS);
		INSTR_INT(STA);
		INSTR_INT(LDA);

		// label parameter
#define INSTR_LABEL(instr)	case S ## instr: printf(#instr " lbl%04d", ssm->arg1.labelval); break
		INSTR_LABEL(BRA);
		INSTR_LABEL(BSR);
		INSTR_LABEL(BRF);

		// register parameter
#define INSTR_REG(instr)	case S ## instr: printf(#instr " %s", ssm_register_to_string(ssm->arg1.regval)); break
		INSTR_REG(STR);
		INSTR_REG(LDR);

		// integer parameter, integer parameter
#define INSTR_INT_INT(instr)	case S ## instr: printf(#instr " %d %d", ssm->arg1.intval, ssm->arg2.intval); break
		INSTR_INT_INT(STMA);

		// register parameter, register parameter
#define INSTR_REG_REG(instr)	case S ## instr: printf(#instr " %s %s", ssm_register_to_string(ssm->arg1.regval), ssm_register_to_string(ssm->arg2.regval)); break
		INSTR_REG_REG(SWPRR);
		INSTR_REG_REG(LDRR);

		default:
			printf("Unknown instruction %d\n", ssm->instr);
			assert(0);
		}

		if(ssm->comment) {
			printf(" ; %s", ssm->comment);
		}

		printf("\n");
		ssm = ssm->next;
	}
	ssm_builtin_functions();
	printf("lbl%04d: NOP ; Begin of the heap\n", heaplabel);
}

void
show_ssm(struct ssmline *ssm) {
	write_ssm(ssm, stdout);
}
