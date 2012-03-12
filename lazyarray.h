#include "generator.h"

typedef struct _lazyarray {
	generator *g;
	void **array;
	int last;
	int size;
} lazyarray;

int lazyarray_exists(lazyarray *la, int num);
void *lazyarray_get(lazyarray *la, int num);
lazyarray *lazyarray_create(genfunc f, void *arg, int async);
void lazyarray_destroy(lazyarray *la);
