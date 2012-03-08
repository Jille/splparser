#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include "tokens.h"

struct rule {
	int id;
	char *name;
	struct rulepart *branches[20];
};

struct rulepart {
	int is_literal;
	union {
		int rule;
		int token;
	};
	struct rulepart *next;
};

int lastrule = -1;
struct rule rules[2000];

int
get_id_for_rule(char *rule) {
	int i;
	for(i = 0; lastrule >= i; i++) {
		if(strcmp(rule, rules[i].name) == 0) {
			return i;
		}
	}
	lastrule++;
	memset(&rules[lastrule], 0, sizeof(struct rule));
	rules[lastrule].id = lastrule;
	rules[lastrule].name = strdup(rule);
	printf("Creating rule %d for %s\n", lastrule, rule);
	return lastrule;
}

struct rulepart *
parse_branch(char *str) {
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
			(*tail)->rule = get_id_for_rule(part);
		}
		tail = &(*tail)->next;
	} while((part = strtok_r(NULL, " ", &tokenizer)) != NULL);

	*tail = NULL;
	return head;
}

void
parse_grammar(char *file) {
	FILE *fh = fopen(file, "r");
	char buf[256];
	while(!feof(fh)) {
		char *def;
		if(fgets(buf, sizeof(buf), fh) == NULL) {
			if(feof(fh)) {
				break;
			}
			err(1, "fgets");
		}
		def = strstr(buf, " := ");
		assert(def != NULL);
		*def = 0;
		def += 4;
		def[strlen(def) - 1] = 0;
		printf("'%s': '%s'\n", buf, def);

		struct rule *rule = &rules[get_id_for_rule(buf)];
		int branchno = 0;
		char *nextbranch = def;
		do {
			char *branch = nextbranch;
			nextbranch = strstr(branch, " | ");
			if(nextbranch != NULL) {
				*nextbranch = 0;
				nextbranch += 3;
			}
			rule->branches[branchno++] = parse_branch(branch);
		} while(nextbranch != NULL);
		printf("Found %d branches for rule %s\n", branchno, rule->name);
	}
	fclose(fh);
}

void
show_grammar() {
	int i;
	for(i = 0; lastrule >= i; i++) {
		if(rules[i].branches[0] == NULL) {
			printf("Rule %s (#%d) not defined\n", rules[i].name, i);
			continue;
		}
		printf("Rule %s :=\n", rules[i].name);
		int b;
		for(b = 0; rules[i].branches[b] != NULL; b++) {
			struct rulepart *p = rules[i].branches[b];
			printf("\t");
			do {
				if(p->is_literal) {
					printf("%s ", token_to_string(p->token));
				} else {
					printf("%s ", rules[p->rule].name);
				}
			} while((p = p->next) != NULL);
			printf("\n");
		}
	}
}

int
main(int argc, char **argv) {
	parse_grammar("grammar.g");
	show_grammar();
	return 0;
}
