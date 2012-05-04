#include <stdlib.h>
#include <stdarg.h>
#include "ir.h"

static irlabel labelctr = 0;

irlabel
getlabel() {
	return ++labelctr;
}

irstm *
irconcat(irstm *stm, ...) {
	va_list ap;
	va_start(ap, stm);
	while(1) {
		irstm *next = va_arg(ap, irstm *);
		if(next == NULL) {
			break;
		}
		stm = mkirseq(stm, next);
	}
	va_end(ap);
	return stm;
}

irstm *
mkirseq_opt(irstm *left, irstm *right) {
	if(left == NULL) {
		return right;
	} else if(right == NULL) {
		return left;
	}
	return mkirseq(left, right);
}

irstm *
mkirseq(irstm *left, irstm *right) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = SEQ;
	ret->seq.left = left;
	ret->seq.right = right;
	return ret;
}

irstm *
mkirmove(irexp *dst, irexp *src) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = MOVE;
	ret->move.dst = dst;
	ret->move.src = src;
	return ret;
}

irexp *
mkirtemp(int num) {
	irstm *ret = malloc(sizeof(struct irunit));
	ret->type = TEMP;
	ret->temp = num;
	return ret;
}
