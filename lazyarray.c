#include <stdlib.h>

#include "generator.h"

typedef struct _lazyarray {
	generator *g;
	void **array;
	int last;
	int size;
} lazyarray;

int
lazyarray_exists(lazyarray *la, int num) {
	while(num > la->last) {
		if(generator_eof(la->g)) {
			return 0;
		}
		if(la->last == la->size) {
			la->size *= 2;
			la->array = realloc(la->array, la->size);
		}
		la->array[la->last++] = generator_shift(la->g);
	}
}

void *
lazyarray_get(lazyarray *la, int num) {
	if(!lazyarray_exists(la, num)) {
		return NULL;
	}
	return la->array[num];
}

void
lazyarray_init(genfunc f, void *arg, int async) {
	lazyarray *la = malloc(sizeof(lazyarray));
	la->last = 0;
	la->size = 1;
	la->g = generator_create(f, arg, async);
}

void
lazyarray_destroy(lazyarray *la) {
	generator_destroy(la->g);
	free(la->array);
}
