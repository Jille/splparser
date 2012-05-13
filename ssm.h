#include <stdint.h>
#include "ir.h"

typedef enum { NOP, BRA, HALT } ssminstr;
typedef int ssmlabel;
typedef int32_t ssmarg;

struct ssmline {
	ssmlabel label;
	ssminstr instr;
	ssmarg arg1;
	ssmarg arg2;
	struct ssmline *next;
};

struct ssmline *ir_to_ssm(struct irunit *ir);
void show_ssm(struct ssmline *ssm, int indent);

