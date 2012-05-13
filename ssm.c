#include <stdio.h>
#include "ssm.h"
#include "ir.h"

struct ssmline *
ir_to_ssm(struct irunit *ir) {
	return 0;
}

void
write_ssm(struct ssmline *ssm, FILE *fd) {
}

void
show_ssm(struct ssmline *ssm) {
	write_ssm(ssm, stdout);
}
