#include <stdint.h>
#include "ir.h"

typedef enum { SNOP, SBRA, SRET, SHALT, SLDC, SSTR, SBSR, SSWPRR, SLINK, SUNLINK, STRAP, SBRF, SBRT, SLDL, SSTL, SAJS, SADD, SSUB, SMUL, SDIV, SMOD, SAND, SOR, SXOR, S_EQ, SNE, SLT, SGT, SLE, SGE, SLDR, SSTA, SLDA, SSTMA, SLDRR } ssminstr;
// XXX [2012-05-14 jille] Waarom zijn NONE en PC hardcoded?
// "Registers 0, 1, 2 and 3 are called PC (program counter), SP (stack pointer), MP (mark pointer) and RR (return register) respectively."
// [2012-05-19 jille] Zucht, ssm.jar vindt 0..7 PC, SP, MP, HP, RR, Rn
typedef enum { NONE = 0, PC = 1, STACK, SP, MP, HP, RR, R5, R6, R7 } ssmregister;
typedef int ssmlabel;

struct ssmline {
	ssmlabel label; // <-- 0 means no label set
	ssminstr instr;
	union {
		int intval;
		ssmlabel labelval;
		ssmregister regval;
	} arg1;
	union {
		int intval;
		ssmlabel labelval;
		ssmregister regval;
	} arg2;
	char *comment;
	struct ssmline *next;
};

/* ssm.c */
ssmlabel get_ssmlabel_from_irlabel(irlabel ir);
ssmlabel get_ssmlabel_from_irfunc(irfunc ir);
struct ssmline *ir_to_ssm(struct irunit *ir);
void write_ssm(struct ssmline *ssm, FILE *fd);
void show_ssm(struct ssmline *ssm);
