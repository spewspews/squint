typedef struct Rat	Rat;
struct Rat
{
	int64_t num;
	int64_t den;
};

Rat	ratmk(int64_t, int64_t);
Rat	ratrecip(Rat);
Rat	ratneg(Rat);
Rat	ratadd(Rat, Rat);
Rat	ratsub(Rat, Rat);
Rat	ratmul(Rat, Rat);
Rat	ratpow(Rat, int);
/*void	ratfmtinstall(void);*/
int	ratprint(Rat);
