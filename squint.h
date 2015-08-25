enum
{
	TMAX = 1000,
	STK = 2048
};

int threadcount;
int verbose;

typedef struct Rat	Rat;
typedef struct Node	Node;
typedef struct Lifo Lifo;

struct Rat{
	vlong num;
	vlong den;
};

struct Node{
	Rat		val;
	Node	*link;
};

struct Lifo{
	Node	*front;
	Node	*rear;
};

Lifo	*mklifo(void);
int		insert(Lifo*, Rat);
int		delete(Lifo*, Rat*);
