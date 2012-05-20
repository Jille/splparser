#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include "tokens.h"
#include "grammar.h"

// #define VERBOSE_GRAMMAR_DEBUG

int
get_id_for_rule(grammar *g, char *rule) {
	int i;
	for(i = 0; g->lastrule >= i; i++) {
		if(strcmp(rule, g->rules[i].name) == 0) {
			return i;
		}
	}
	g->lastrule++;
	assert(g->lastrule < sizeof(g->rules) / sizeof(struct rule));
	memset(&g->rules[g->lastrule], 0, sizeof(struct rule));
	g->rules[g->lastrule].id = g->lastrule;
	g->rules[g->lastrule].name = strdup(rule);
#ifdef VERBOSE_GRAMMAR_DEBUG
	printf("Creating rule %d for %s\n", g->lastrule, rule);
#endif
	return g->lastrule;
}

struct rulepart *
parse_branch(grammar *g, char *str) {
	char *part, *tokenizer;
	struct rulepart *head;
	struct rulepart **tail = &head;

	assert(strchr(str, '|') == NULL);

	part = strtok_r(str, " ", &tokenizer);
	do {
		// Handle part
		*tail = malloc(sizeof(struct rulepart));
		if(part[0] == '\'' && part[2] == '\'' && part[3] == 0) {
			(*tail)->is_literal = 1;
			(*tail)->token = part[1];
		} else if(((*tail)->token = string_to_token(part)) != 0) {
			(*tail)->is_literal = 1;
		} else {
			(*tail)->is_literal = 0;
			(*tail)->rule = get_id_for_rule(g, part);
		}
		tail = &(*tail)->next;
	} while((part = strtok_r(NULL, " ", &tokenizer)) != NULL);

	*tail = NULL;
	return head;
}

grammar *
parse_grammar(char *file) {
	FILE *fh = fopen(file, "r");
	char buf[1024];
	grammar *g = malloc(sizeof(grammar));
	g->lastrule = -1;
	while(!feof(fh)) {
		char *def;
		if(fgets(buf, sizeof(buf), fh) == NULL) {
			if(feof(fh)) {
				break;
			}
			err(1, "fgets");
		}
		if(buf[0] == '#') {
			continue;
		}
		def = strstr(buf, " := ");
		assert(def != NULL);
		*def = 0;
		def += 4;
		def[strlen(def) - 1] = 0;
#ifdef VERBOSE_GRAMMAR_DEBUG
		printf("'%s': '%s'\n", buf, def);
#endif

		struct rule *rule = &g->rules[get_id_for_rule(g, buf)];
		int branchno = 0;
		char *nextbranch = def;
		do {
			char *branch = nextbranch;
			nextbranch = strstr(branch, " | ");
			if(nextbranch != NULL) {
				*nextbranch = 0;
				nextbranch += 3;
			}
			rule->branches[branchno++] = parse_branch(g, branch);
			assert(branchno < (sizeof(rule->branches) / sizeof(struct rulepart *)));
		} while(nextbranch != NULL);
#ifdef VERBOSE_GRAMMAR_DEBUG
		printf("Found %d branches for rule %s\n", branchno, rule->name);
#endif
	}
	fclose(fh);
	return g;
}

void
show_grammar(grammar *g) {
	int i;
	for(i = 0; g->lastrule >= i; i++) {
		if(g->rules[i].branches[0] == NULL) {
			fprintf(stderr, "Rule %s (#%d) not defined\n", g->rules[i].name, i);
			continue;
		}
#ifdef VERBOSE_GRAMMAR_DEBUG
		printf("Rule %s :=\n", g->rules[i].name);
#endif
		int b;
		for(b = 0; g->rules[i].branches[b] != NULL; b++) {
			struct rulepart *p = g->rules[i].branches[b];
			printf("\t");
			do {
				if(p->is_literal) {
					printf("%s ", token_to_string(p->token));
				} else {
					printf("%s ", g->rules[p->rule].name);
				}
			} while((p = p->next) != NULL);
			printf("\n");
		}
	}
}
