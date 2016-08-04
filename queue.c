#include <u.h>
#include <libc.h>
#include "rat.h"
#include "queue.h"

Queue*
mkqueue(void)
{
	Queue *q;

	q = malloc(sizeof(*q));
	queueset(q);
	return q;
}

void
queueset(Queue *q)
{
	q->front = nil;
	q->rear = &q->front;
}

int
isempty(Queue *q)
{
	return q->front == nil;
}

void
frontinsert(Queue *q, Rat val)
{
	Node	*n;

	n = malloc(sizeof(*n));
	if(q->front == nil)
		q->rear = &n->link;
	n->val = val;
	n->link = q->front;
	q->front = n;
}

void
insert(Queue *q, Rat val)
{
	Node	*n;

	n = malloc(sizeof(*n));
	n->val = val;
	n->link = nil;
	*q->rear = n;
	q->rear = &n->link;
}

Rat
delete(Queue *q)
{
	Rat	r;
	Node	*n;

	n = q->front;
	r = n->val;
	q->front = n->link;
	free(n);
	if(q->front == nil)
		q->rear = &q->front;
	return r;
}
