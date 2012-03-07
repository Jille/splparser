#include <stdio.h>
#include <stdlib.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"

int
main(int argc, char **argv) {
	generator *g;
	g = generator_create(gen_tokens, "test.spl", 0);

	while(!generator_eof(g)) {
		struct token *t = generator_shift(g);
		printf("Token: %s\n", token_to_string(t->type));
		switch(t->type) {
			case T_WORD:
				printf("Value: '%s'\n", t->value.sval);
				free(t->value.sval);
				break;
			case T_NUMBER:
				printf("Value: %d\n", t->value.ival);
				break;
		}
		free(t);
	}

	return 0;
}
