typedef struct Node Node;
typedef struct Queue Queue;

struct Node
{
	Node *link;
	Rat	val;
};

struct Queue
{
	Node *front;
	Node **rear;
};

Queue *mkqueue(void);
void queueset(Queue*);
void insert(Queue*, Rat);
void frontinsert(Queue*, Rat);
Rat delete(Queue*);
