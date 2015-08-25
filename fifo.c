#include <u.h>
#include <libc.h>
#include "rat.h"
#include "fifo.h"

Fifo*
mkfifo(void)
{
	Fifo *q;

	q = malloc(sizeof(*q));
	q->front = nil;
	return q;
}

int
isempty(Fifo *q)
{
	return q->front == nil;
}

void
frontinsert(Fifo *q, Rat val)
{
	Node	*n;

	n = malloc(sizeof(*n));
	if(q->front == nil)
		q->rear = &n->link;
	n->val = val;
	n->link = q->front;
	q->front = n;
	return;
}

void
insert(Fifo *q, Rat val)
{
	Node	*n;

	n = malloc(sizeof(*n));
	n->val = val;
	n->link = nil;
	if(q->front == nil)
		q->front = n;
	else
		*q->rear = n;
	q->rear = &n->link;
	return;
}

Rat
delete(Fifo *q)
{
	Rat	r;
	Node	*n;

	n = q->front;
	r = n->val;
	q->front = n->link;
	free(n);
	return r;
}
