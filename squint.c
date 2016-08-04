#include <u.h>
#include <libc.h>
#include <thread.h>
#include "rat.h"
#include "queue.h"

enum
{
	STK = 8192
};

typedef struct Ps Ps;
struct Ps
{
	char @close;
	char @req;
	Rat @dat;
};

int debug;
int threadcount;

Rat
getr(Ps *f)
{
	Rat r;
	f->req @= 0;
	r = @f->dat;
	return r;
}

Ps*
psmk(int nel)
{
	Ps	*d;

	d = malloc(sizeof(*d));
	chanset(d->close, 0);
	chanset(d->req, 0);
	chanset(d->dat, nel);
	return d;
}

void
psfree(Ps *d)
{
	chanfree(d->close);
	chanfree(d->req);
	chanfree(d->dat);
	free(d);
}


static void
_split(Ps *f, Ps *s, Rat @qchan, char @close)
{
	Queue q;
	Rat r;

	queueset(&q);
	for(;;) switch @{
	alt @s->req:
		if(q.front == nil) {
			r = getr(f);
			qchan @= r;
			s->dat @= r;
		} else
			s->dat @= delete(&q);
		break;
	alt r = @qchan:
		insert(&q, r);
		break;
	alt @s->close:
		while(q.front != nil)
			delete(&q);
		psfree(s);
		if(chanclosing(qchan) == -1) {
			if(close != nil) {
				close @= 0;
				chanfree(close);
			}
			chanclose(qchan);
		} else {
			chanfree(qchan);
			f->close @= 0;
		}
		if(debug) threadcount--;
		threadexits(0);
	}
}

void
split(Ps *f, Ps *s[2], char @splitclose)
{
	Rat @q;
	int i;

	chanset(q, 0);
	for(i = 0; i < 2; i++) {
		s[i] = psmk(0);
		cothread(_split(f, s[i], q, splitclose), STK);
		if(debug) threadcount++;
	}
}

static void
_mkconst(Rat c, Ps *o)
{
	for(;;) switch @{
	alt @o->req:
		o->dat @= c;
		break;
	alt @o->close:
		if(debug) threadcount--;
		threadexits(0);
	}
}

Ps*
mkconst(Rat c)
{
	Ps *o;

	o = psmk(0);
	cothread(_mkconst(c, o), STK);
	if(debug) threadcount++;
	return o;
}

Ps*
binop(void (*oper)(Ps*, Ps*, Ps*), Ps *f, Ps *g)
{
	Ps *o;

	o = psmk(0);
	cothread(oper(f, g, o), STK);
	if(debug) threadcount++;
	return o;
}

static void
_psadd(Ps *f, Ps *g, Ps *s)
{
	for(;;) switch @{
	alt @s->req:
		s->dat @= ratadd(getr(f), getr(g));
		break;
	alt @s->close:
		psfree(s);
		f->close @= 0;
		g->close @= 0;
		if(debug) threadcount--;
		threadexits(0);
	}
}

Ps*
psadd(Ps *f, Ps *g)
{
	return binop(_psadd, f, g);
}

static void
_psderiv(Ps *f, Ps *d)
{
	Rat r;
	int i;

	getr(f);
	for(i = 1;; i++) {
		switch @{
		alt @d->req:
			r = getr(f);
			d->dat @= ratmk(i * r.num, r.den);
			break;
		alt @d->close:
			psfree(d);
			f->close @= 0;
			if(debug) threadcount--;
			threadexits(0);
		}
	}
}

Ps*
psderiv(Ps *f)
{
	Ps *d;

	d = psmk(0);
	cothread(_psderiv(f, d), STK);
	if(debug) threadcount++;
	return d;
}

void
psprint(Ps *ps, int n)
{
	int	i;

	for(i = 0; i < n - 1; i++)
		print("%R ", getr(ps));
	print("%R\n", getr(ps));

}

static void
_psinteg(Ps *f, Rat c, Ps *i)
{
	int j;

	switch @{
	alt @i->req:
		i->dat @= c;
		break;
	alt @i->close:
		goto End;
	}
	for(j = 1;; j++) {
		switch @{
		alt @i->req:
			i->dat @= ratmul(getr(f), ratmk(1, j));
			break;
		alt @i->close:
			goto End;
		}
	}
End:
	psfree(i);
	f->close @= 0;
	if(debug) threadcount--;
	threadexits(0);
}


Ps*
psinteg(Ps *ps, Rat c)
{
	Ps *i;

	i = psmk(0);
	cothread(_psinteg(ps, c, i), STK);
	if(debug) threadcount++;
	return i;
}

static void
_pscmul(Ps *f, Rat c, Ps *o)
{
	for(;;) switch @{
	alt @o->req:
		o->dat @= ratmul(c, getr(f));
		break;
	alt @o->close:
		psfree(o);
		f->close @= 0;
		if(debug) threadcount--;
		threadexits(0);
	}
}

Ps*
pscmul(Rat c, Ps* f)
{
	Ps *o;

	o = psmk(0);
	cothread(_pscmul(f, c, o), STK);
	if(debug) threadcount++;
	return o;
}

static void
_psmul(Ps *f, Ps *g, Ps *p)
{
	Queue fq, gq;
	Node *fnode, *gnode;
	Rat sum;

	queueset(&fq);
	queueset(&gq);
	for(;;) switch @{
	alt @p->req:
		insert(&fq, getr(f));
		frontinsert(&gq, getr(g));
		fnode = fq.front;
		gnode = gq.front;
		sum = ratmk(0, 1);
		while(fnode != nil) {
			sum = ratadd(sum, ratmul(fnode->val, gnode->val));
			fnode = fnode->link;
			gnode = gnode->link;	
		}
		p->dat @= sum;
		break;
	alt @p->close:
		psfree(p);
		while(fq.front != nil)
			delete(&fq);
		while(gq.front != nil)
			delete(&gq);
		f->close @= 0;
		g->close @= 0;
		if(debug) threadcount--;
		threadexits(0);
	}
}

Ps*
psmul(Ps *f, Ps *g)
{
	return binop(_psmul, f, g);
}

static void
_psrecip(Ps *f, Ps *r, Ps *rr, char @close)
{
	Ps *recip;
	Rat g;

	switch @{
	alt @r->req:
		g = ratrecip(getr(f));
		r->dat @= g;
		break;
	alt @close:
		rr->close @= 0;
		f->close @= 0;
		goto End;
	}
	recip = pscmul(ratneg(g), psmul(f, rr));
	for(;;) switch @{
	alt @r->req:
		r->dat @= getr(recip);
		break;
	alt @close:
		recip->close @= 0;
		goto End;
	}
End:
	@r->close;
	psfree(r);
	if(debug) threadcount--;
	threadexits(0);
}

Ps*
psrecip(Ps *f)
{
	Ps *rr[2], *r;
	char @close;

	r = psmk(0);
	chanset(close, 0);

	split(r, rr, close);
	cothread(_psrecip(f, r, rr[0], close), STK);
	if(debug) threadcount++;
	return rr[1];
}

static void
_psmsubst(Ps *f, Rat c, int deg, Ps *m)
{
	Rat zero;
	int i, j;

	zero = ratmk(0, 1);
	for(i = 0;; i++) {
		switch @{
		alt @m->req:
			m->dat @= ratmul(getr(f), ratpow(c, i));
			for(j = 0; j < deg - 1; j++){
				switch @{
				alt @m->req:
					m->dat @= zero;
					break;
				alt @m->close:
					goto End;
				}
			}
			break;
		alt @m->close:
			goto End;
		}
	}
End:
	psfree(m);
	f->close @= 0;
	if(debug) threadcount--;
	threadexits(0);
}

Ps*
psmsubst(Ps *f, Rat c, int deg)
{
	Ps *m;

	m = psmk(0);
	cothread(_psmsubst(f, c, deg, m), STK);
	if(debug) threadcount++;
	return m;
}

Ps *pssubst(Ps*, Ps*);

static void
_pssubst(Ps *f, Ps *g, Ps *o)
{
	Ps *gg[2], *subst;

	switch @{
	alt @o->req:
		o->dat @= getr(f);
		break;
	alt @o->close:
		psfree(o);
		f->close @= 0;
		g->close @= 0;
		if(debug) threadcount--;
		threadexits(0);
	}
	split(g, gg, nil);
	getr(gg[0]);
	subst = psmul(gg[0], pssubst(f, gg[1]));
	for(;;) switch @{
	alt @o->req:
		o->dat @= getr(subst);
		break;
	alt @o->close:
		psfree(o);
		subst->close @= 0;
		if(debug) threadcount--;
		threadexits(0);
	}
}

Ps*
pssubst(Ps *f, Ps *g)
{
	return binop(_pssubst, f, g);
}

static void
_psrev(Ps *f, Ps *r, Ps *rr, char @close)
{
	Ps *rever;
	Rat g;

	g = ratmk(0, 1);
	switch @{
	alt @r->req:
		r->dat @= g;
		break;
	alt @close:
		rr->close @= 0;
		f->close @= 0;
		goto End;
	}
	getr(f);
	rever = psrecip(pssubst(f, rr));
	for(;;) switch @{
	alt @r->req:
		r->dat @= getr(rever);
		break;
	alt @close:
		rever->close @= 0;
		goto End;
	}
End:
	@r->close;
	psfree(r);
	if(debug) threadcount--;
	threadexits(0);
}

Ps*
psrev(Ps *f)
{
	Ps *rr[2], *r;
	char @close;

	chanset(close, 0);
	r = psmk(0);
	split(r, rr, close);
	cothread(_psrev(f, r, rr[0], close), STK);
	if(debug) threadcount++;
	return rr[1];
}

void
threadmain(int, char **)
{
	Ps	*ps1, *ps2, *pssum, *ints, *tanx, *pspair[2];
	Rat	one, zero;

	debug = 1;
	ratfmtinstall();

	zero = ratmk(0, 1);
	one = ratmk(1, 1);

	ps1 = mkconst(one);
	print("1/1-x\n");
	print("1 1 1 1 1 1 1 1 1 1\n");
	psprint(ps1, 10);
	print("\n");

	ps2 = mkconst(one);
	pssum = psadd(ps1, ps2);
	print("1/1-x + 1/1-x\n");
	print("2 2 2 2 2 2 2 2 2 2\n");
	psprint(pssum, 10);
	print("\n");
	send(pssum->close, nil);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	print("d/dx(1/1-x)\n");
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ints, 10);
	print("\n");
	send(ints->close, nil);

	ps1 = mkconst(one);
	ps2 = psinteg(ps1, one);
	print("integral of 1/1-x\n");
	print("1 1 1/2 1/3 1/4 1/5 1/6 1/7 1/8 1/9\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	split(ints, pspair, nil);
	print("d/dx(1/1-x) split into two streams\n");
	print("1 2 3 4 5 6 7 8 9 10\n");
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(pspair[0], 10);
	psprint(pspair[1], 10);
	print("\n");
	send(pspair[0]->close, nil);
	send(pspair[1]->close, nil);

	ps1 = mkconst(one);
	ps2 = pscmul(ratmk(2, 1), ps1);
	print("2*(1/1-x)\n");
	print("2 2 2 2 2 2 2 2 2 2\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	split(ps1, pspair, nil);
	ps2 = psmul(pspair[0], pspair[1]);
	print("1/1-x split into two streams and then multiplied\n");
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	ps2 = psrecip(ps1);
	print("1/(1/1-x)\n");
	print("1 -1 0 0 0 0 0 0 0 0\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	tanx = psrev(psinteg(psmsubst(ps1, ratmk(-1, 1), 2), zero));
	print("tan(x)\n");
	print("0 1 0 1/3 0 2/15 0 17/315 0 62/2835 0 1382/155925\n");
	psprint(tanx, 12);
	send(tanx->close, nil);
	threadexitsall(0);
}
