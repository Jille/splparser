#define _GNU_SOURCE // voor asprintf

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "generator.h"
#include "scanner.h"
#include "prototypes.h"

#define SCN_EOF	-1
#define SCN_ERR	-2
#define SCN_NUL	-3
#define SCN_SKP	-4

#define SCN_CASE_DIGIT '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'
#define SCN_CASE_LOWER 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z'
#define SCN_CASE_UPPER 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'
#define SCN_CASE_ALPHA SCN_CASE_LOWER: case SCN_CASE_UPPER
#define SCN_CASE_ALPHANUM SCN_CASE_ALPHA: case SCN_CASE_DIGIT

static int scan_initial_int(struct scannerstate *, char);
static int scan_initial_lower(struct scannerstate *, char);
static int scan_initial_upper(struct scannerstate *, char);
static int scan_initial_sign(struct scannerstate *, char);

void
gen_tokens(generator *g, void *arg) {
	struct scannerstate st;
	st.active = malloc(sizeof(struct token));
	st.active->type = 0;
	st.curline = 1;
	int nextchar = SCN_NUL;
	st.ig = generator_create(gen_input, arg, 0);

	while(nextchar != SCN_NUL || !generator_eof(st.ig)) {
		char in;
		if(nextchar == SCN_NUL) {
			in = (char)(int)generator_shift(st.ig);
		} else {
			in = nextchar;
			assert(in >= 0 && in < 256);
			nextchar = SCN_NUL;
		}
		switch(in) {
			case SCN_CASE_DIGIT:
				nextchar = scan_initial_int(&st, in);
				break;
			case SCN_CASE_LOWER:
				nextchar = scan_initial_lower(&st, in);
				break;
			case SCN_CASE_UPPER:
				nextchar = scan_initial_upper(&st, in);
				break;
			case '=':
			case ';':
			case ',':
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case ':':
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case '<':
			case '>':
			case '!':
			case '&':
			case '|':
				nextchar = scan_initial_sign(&st, in);
				break;
			case '\n':
				st.curline++;
			case ' ':
			case '\t':
			case '\r':
				// We zouden hier een T_WHITESPACE kunnen yielden
				nextchar = SCN_SKP;
				break;
			default:
				asprintf(&st.error, "Unknown character %c (%d) found", in, in);
				nextchar = SCN_ERR;
		}
		if(nextchar == SCN_ERR) {
			break;
		}
		if(nextchar == SCN_SKP) {
			nextchar = SCN_NUL;
			continue;
		}
		generator_yield(g, st.active);
		st.active = malloc(sizeof(struct token));
		st.active->type = 0;
		st.active->lineno = st.curline;
	}
	if(nextchar == SCN_ERR) {
		fprintf(stderr, "%s: Error from scanner: %s at line %d\n", (char *)arg, st.error, st.curline);
		free(st.error);
		exit(1);
	}
}

static int
scan_initial_int(struct scannerstate *st, char in) {
	int num = in - '0';

	while(!generator_eof(st->ig)) {
		in = (char)(int)generator_shift(st->ig);
		switch(in) {
			case SCN_CASE_DIGIT:
				num *= 10;
				num += in - '0';
				break;
			default:
				goto done;
		}
	}
	in = SCN_EOF;
done:
	st->active->type = T_NUMBER;
	st->active->value.ival = num;
	return in;
}

static int
scan_initial_lower(struct scannerstate *st, char in) {
	char buf[128];
	char *bufp = buf;
	*bufp++ = in;

	while(!generator_eof(st->ig)) {
		if(buf + sizeof(buf) == bufp) {
			asprintf(&st->error, "Word '%.*s...' too long.", sizeof(buf), buf);
			return SCN_ERR;
		}
		in = (char)(int)generator_shift(st->ig);
		switch(in) {
			case SCN_CASE_DIGIT:
			case SCN_CASE_LOWER:
				*bufp++ = in;
				break;
			case SCN_CASE_UPPER:
				asprintf(&st->error, "Word '%.*s...' contains invalid uppercase character.", bufp - buf, buf);
				return SCN_ERR;
			default:
				goto done;
		}
	}
	in = SCN_EOF;

done:
	*bufp = 0;
	if(strcmp(buf, "if") == 0) {
		st->active->type = T_IF;
	} else if(strcmp(buf, "while") == 0) {
		st->active->type = T_WHILE;
	} else if(strcmp(buf, "else") == 0) {
		st->active->type = T_ELSE;
	} else if(strcmp(buf, "return") == 0) {
		st->active->type = T_RETURN;
	} else {
		st->active->type = T_WORD;
		st->active->value.sval = strdup(buf);
	}
	return in;
}

static int
scan_initial_upper(struct scannerstate *st, char in) {
	char buf[16];
	char *bufp = buf;
	*bufp++ = in;

	while(!generator_eof(st->ig)) {
		if(buf + sizeof(buf) == bufp) {
			asprintf(&st->error, "Word '%.*s...' too long.", sizeof(buf), buf);
			return SCN_ERR;
		}
		in = (char)(int)generator_shift(st->ig);
		switch(in) {
			case SCN_CASE_ALPHA:
				*bufp++ = in;
				break;
			case SCN_CASE_DIGIT:
				asprintf(&st->error, "Keyword '%.*s...' contains digit.", bufp - buf, buf);
				return SCN_ERR;
			default:
				goto done;
		}
	}
	in = SCN_EOF;

done:
	*bufp = 0;
	if(strcmp(buf, "True") == 0) {
		st->active->type = T_TRUE;
	} else if(strcmp(buf, "False") == 0) {
		st->active->type = T_FALSE;
	} else if(strcmp(buf, "Int") == 0) {
		st->active->type = T_INT;
	} else if(strcmp(buf, "Bool") == 0) {
		st->active->type = T_BOOL;
	} else if(strcmp(buf, "Void") == 0) {
		st->active->type = T_VOID;
	} else {
		asprintf(&st->error, "Keyword '%s...' is unknown.", buf);
		return SCN_ERR;
	}
	return in;
}

static int
scan_initial_sign(struct scannerstate *st, char in) {
	char in2;
	st->active->type = in;
	switch(in) {
		case ';':
		case ',':
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case ':':
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
			return SCN_NUL;
		case '=':
		case '!':
		case '<':
		case '>':
		case '&':
		case '|':
			if(generator_eof(st->ig)) {
				in2 = SCN_EOF;
			} else {
				in2 = (char)(int)generator_shift(st->ig);
			}
			switch(in2) {
				case '=':
					switch(in) {
						case '=':
							st->active->type = T_EQ;
							break;
						case '!':
							st->active->type = T_NE;
							break;
						case '>':
							st->active->type = T_GTE;
							break;
						case '<':
							st->active->type = T_LTE;
							break;
						default:
							return in2;
					}
					return SCN_NUL;
				case '&':
				case '|':
					if(in == in2) {
						st->active->type = (in == '&') ? T_AND : T_OR;
						return SCN_NUL;
					}
					break;
				case SCN_EOF:
				default:
					if(in2 == '&' || in2 == '|') {
						break;
					}
					return in2;
			}
	}
	asprintf(&st->error, "Operator '%c' is unknown.", in);
	return SCN_ERR;
}
