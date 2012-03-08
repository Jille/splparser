#include <assert.h>
#include <stdlib.h>
#define NDEBUG
#ifndef NDEBUG
#include <stdio.h>
#endif
#include "generator.h"

void
generator_yield(generator *g, void *item) {
#ifndef NDEBUG
	if(item != NULL) {
		printf("Yield: %p\n", item);
	} else {
		printf("Yield: NULL\n");
	}
#endif
	pthread_testcancel();
	if(g->async == -1) {
		g->item = item;
		g->has_item = 1;
		pthread_cond_signal(&g->cond);
		while(g->has_item) {
			pthread_cond_wait(&g->cond, &g->lock);
		}
	} else {
		pthread_mutex_lock(&g->lock);
		pthread_testcancel();
		*g->output_last = calloc(1, sizeof(struct generator_item));
		(*g->output_last)->item = item;
		g->output_last = &((*g->output_last)->next);
		pthread_cond_signal(&g->cond);
		pthread_mutex_unlock(&g->lock);
	}
}

int
generator_eof(generator *g) {
	if(g->async == -1) {
		if(g->ready != NULL) {
			return 0;
		}
		while(g->has_item == 0) {
			pthread_cond_signal(&g->cond);
			pthread_cond_wait(&g->cond, &g->lock);
		}
		return (g->has_item == -1);
	} else {
		while(g->ready == NULL) {
			pthread_mutex_lock(&g->lock);
			while(g->output == NULL) {
				pthread_cond_wait(&g->cond, &g->lock);
			}
			g->ready = g->output;
			g->output = NULL;
			g->output_last = &g->output;
			pthread_mutex_unlock(&g->lock);
		}
		return (g->ready == (void *)-1);
	}
}

void *
generator_shift(generator *g) {
	void *ret;
	if(generator_eof(g)) {
		return NULL;
	}
	if(g->async == -1 && g->ready != NULL) {
		ret = g->item;
		g->item = NULL;
		if(g->has_item == -2) {
			g->has_item = -1;
		} else {
			g->has_item = 0;
		}
	} else {
		void *container = g->ready;
		ret = g->ready->item;
		g->ready = g->ready->next;
		free(container);
	}
	return ret;
}

void
generator_unshift(generator *g, void *item) {
	struct generator_item *gi = malloc(sizeof(struct generator_item));
	gi->item = item;
	gi->next = g->ready;
	g->ready = gi;
}

static void *
generator_run(void *arg) {
	generator *g = (generator *)arg;
	pthread_setcanceltype(PTHREAD_CANCEL_DISABLE, NULL);
	if(g->async == -1) {
		pthread_mutex_lock(&g->lock);
	}
	g->function(g, g->arg);
	pthread_testcancel();
	if(g->async != -1) {
		pthread_mutex_lock(&g->lock);
		*g->output_last = (void *)-1;
	} else {
		if(g->has_item == 0) {
			g->has_item = -1;
		} else {
			g->has_item = -2;
		}
	}
	*(int *)g->thr = 0;
	pthread_cond_signal(&g->cond);
	pthread_mutex_unlock(&g->lock);
	return 0;
}

generator *
generator_create(genfunc f, void *arg, int async) {
	generator *g = calloc(1, sizeof(generator));
	g->function = f;
	g->arg = arg;
	if(async) {
		g->ready = NULL;
		g->output = NULL;
		g->output_last = &g->output;
	} else {
		g->async = -1;
		g->item = NULL;
		g->has_item = 0;
	}
	pthread_mutex_init(&g->lock, NULL);
	pthread_cond_init(&g->cond, NULL);
	if(!async) {
		pthread_mutex_lock(&g->lock);
	}
	pthread_create(&g->thr, NULL, generator_run, (void *)g);
	return g;
}

void
generator_destroy(generator *g) {
	if(g->async != -1) {
		pthread_mutex_lock(&g->lock);
	}
	if(*(int *)g->thr != 0) {
		pthread_cancel(g->thr);
	}
	if(g->async != -1) {
		// Concat g->ready to g->output
		assert(*g->output_last == NULL || *g->output_last == (void *)-1);
		*g->output_last = g->ready;
		// Free g->output
		while(g->output != NULL && g->output != (void *)-1) {
			struct generator_item *c = g->output;
			g->output = c->next;
			free(c);
		}
	}
	pthread_mutex_unlock(&g->lock);
	pthread_mutex_destroy(&g->lock);
	pthread_cond_destroy(&g->cond);
	free(g);
}
