#include <pthread.h>

typedef struct _generator generator;
typedef void (*genfunc)(generator *, void *);

struct generator_item {
	struct generator_item *next;
	void *item;
};

struct _generator {
	pthread_t thr;
	genfunc function;
	void *arg;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	union {
		struct {
			int async;
			void *item;
			int has_item;
		};
		struct {
			struct generator_item *ready;
			struct generator_item *output;
			struct generator_item **output_last;
		};
	};
};

void generator_yield(generator *g, void *item);
int generator_eof(generator *g);
void *generator_shift(generator *g);
generator *generator_create(genfunc f, void *arg, int async);
void generator_destroy(generator *g);
