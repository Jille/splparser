#include "tokens.h"

struct token {
	char type;
	union {
		int ival;
		char bval;
		char *sval;
	} value;
	int lineno;
};

struct scannerstate {
	generator *ig;
	struct token *active;
	int curline;
	char *error;
};
