typedef struct Node	Node;
typedef struct Fifo Fifo;

struct Node
{
	Node	*link;
	Rat	val;
};

struct Fifo
{
	Node	*front;
	Node	**rear;
};

Fifo	*mkfifo(void);
void	insert(Fifo*, Rat);
void	frontinsert(Fifo*, Rat);
Rat	delete(Fifo*);
