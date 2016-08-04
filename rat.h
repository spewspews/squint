typedef struct Rat Rat;

struct Rat
{
	vlong num;
	vlong den;
};

#pragma varargck type "R" Rat

Rat ratmk(vlong, vlong);
Rat ratrecip(Rat);
Rat ratneg(Rat);
Rat ratadd(Rat, Rat);
Rat ratsub(Rat, Rat);
Rat ratmul(Rat, Rat);
Rat ratpow(Rat, int);
void ratfmtinstall(void);
