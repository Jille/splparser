#include <stdlib.h>
#include "lazyarray.h"

int
lazyarray_exists(lazyarray *la, int num) {
	while(num >= la->last) {
		if(generator_eof(la->g)) {
			return 0;
		}
		if(la->last == la->size) {
			la->size *= 2;
			la->array = realloc(la->array, la->size * sizeof(void *));
		}
		la->array[la->last++] = generator_shift(la->g);
	}
	return 1;
}

void *
lazyarray_get(lazyarray *la, int num) {
	if(!lazyarray_exists(la, num)) {
		return NULL;
	}
	return la->array[num];
}

lazyarray *
lazyarray_create(genfunc f, void *arg, int async) {
	lazyarray *la = malloc(sizeof(lazyarray));
	la->array = malloc(sizeof(void  *));
	la->last = 0;
	la->size = 1;
	la->g = generator_create(f, arg, async);
	return la;
}

void
lazyarray_destroy(lazyarray *la) {
	generator_destroy(la->g);
	free(la->array);
}
