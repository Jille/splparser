#include <stdint.h>
#include "ir.h"

typedef enum { SNOP, SBRA, SRET, SHALT, SLDC, SSTR, SBSR, SSWPRR, SLINK, SUNLINK } ssminstr;
typedef enum { NONE = 0, PC = 1, SP, MP, RR, R4, R5, R6, R7 } ssmregister;
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
	struct ssmline *next;
};

struct ssmline *ir_to_ssm(struct irunit *ir);
void write_ssm(struct ssmline *ssm, FILE *fd);
void show_ssm(struct ssmline *ssm);

