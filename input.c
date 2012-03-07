#include <stdio.h>
#include <err.h>
#include "generator.h"

void
gen_input(generator *g, void *arg) {
	FILE *fh = fopen((char *)arg, "r");
	if(fh == NULL) {
		err(1, "fopen");
	}
	while(!feof(fh)) {
		char in = getc(fh);
		if(in == EOF) {
			break;
		}
		generator_yield(g, (void *)(int)in);
	}
	fclose(fh);
}
