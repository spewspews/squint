#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "rat.h"

static int
gcd(int64_t a, int64_t b)
{
	int64_t t;

	while(b != 0){
		t = b;
		b = a % b;
		a = t;
	}
	return llabs(a);
}

Rat
ratmk(int64_t n, int64_t d)
{
	Rat		r;
	int64_t	g;

	g = gcd(n, d);
	if(d > 0){
		r.num = n/g;
		r.den = d/g;
	}else{
		r.num = -n/g;
		r.den = -d/g;
	}
	return r;
}

Rat
ratrecip(Rat a)
{
	int64_t	t;

	t = a.num;
	a.num = a.den;
	a.den = t;
	if(a.den < 0){
		a.num = -a.num;
		a.den = -a.den;
	}
	return a;
}

Rat
ratneg(Rat a)
{
	a.num = -a.num;
	return a;
}

Rat
ratadd(Rat a, Rat b)
{
	int64_t	aden, bden, g;

	g = gcd(a.den, b.den);
	aden = a.den / g;
	bden = b.den / g;
	return ratmk(a.num*bden + b.num*aden, a.den*bden);
}

Rat
ratsub(Rat r, Rat s)
{
	return ratadd(r, ratneg(s));
}

Rat
ratmul(Rat r, Rat s)
{
	Rat	t;
	int64_t	g, h;

	g = gcd(r.num, s.den);
	h = gcd(s.num, r.den);
	t.num = (r.num/g) * (s.num/h);
	t.den = (r.den/h) * (s.den/g);
	return t;
}

Rat
ratpow(Rat r, int e)
{
	r.num = pow(r.num, e);
	r.den = pow(r.den, e);
	return r;
}

int
ratprint(Rat r)
{
	if(r.den == 1)
		return printf("%ld", r.num);
	else
		return printf("%ld/%ld", r.num, r.den);
}
	
/*
int
Rfmt(Fmt *f)
{
	Rat r;

	r = va_arg(f->args, Rat);
	if(r.den == 1)
		return fmtprint(f, "%ld", r.num);
	else
		return fmtprint(f, "%ld/%ld", r.num, r.den);
}

void
ratfmtinstall(void)
{
	fmtinstall('R', Rfmt);
}
*/
