#include <u.h>
#include <libc.h>
#include "rat.h"

static int
gcd(vlong a, vlong b)
{
	vlong t;

	while(b != 0) {
		t = b;
		b = a % b;
		a = t;
	}
	return abs(a);
}

Rat
ratmk(vlong n, vlong d)
{
	Rat		r;
	vlong	g;

	g = gcd(n, d);
	if(d > 0) {
		r.num = n/g;
		r.den = d/g;
	} else {
		r.num = -n/g;
		r.den = -d/g;
	}
	return r;
}

Rat
ratrecip(Rat a)
{
	vlong	t;

	t = a.num;
	a.num = a.den;
	a.den = t;
	if(a.den < 0) {
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
	vlong	aden, bden, g;

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
	vlong	g, h;

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
Rfmt(Fmt *f)
{
	Rat r;

	r = va_arg(f->args, Rat);
	if(r.den == 1)
		return fmtprint(f, "%lld", r.num);
	else
		return fmtprint(f, "%lld/%lld", r.num, r.den);
}

void
ratfmtinstall(void)
{
	fmtinstall('R', Rfmt);
}
