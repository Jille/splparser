#include <stdint.h>
#include "ir.h"

typedef enum { NOP, BRA, HALT } ssminstr;
typedef int ssmlabel;
typedef int32_t ssmarg;

struct ssmline {
	ssmlabel label; // <-- 0 means no label set
	ssminstr instr;
	ssmarg arg1;
	ssmarg arg2;
	struct ssmline *next;
};

struct ssmline *ir_to_ssm(struct irunit *ir);
void write_ssm(struct ssmline *ssm, FILE *fd);
void show_ssm(struct ssmline *ssm);

