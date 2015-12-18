#include <multitask.h>
#include <stdio.h>
#include <stdlib.h>
#include "rat.h"
#include "fifo.h"

int debug;
int threadcount;

enum
{
	TMAX = 1000,
	STK = 2048
};

int
chanclosing(Chan *c)
{
	if(atomic_load(&c->closed))
		return 1;
	return 0;
}


typedef struct Ps Ps;
struct Ps
{
	Chan *close;
	Chan *req;
	Chan *dat;
};

Rat
getr(Ps *f)
{
	Rat	r;

	chansend(f->req, NULL);
	chanrecv(f->dat, &r);
	return r;
}

Ps*
psmk(int nel)
{
	Ps	*d;

	d = malloc(sizeof(*d));
	d->close = channew(1, 0);
	d->req = channew(1, 0);
	d->dat = channew(sizeof(Rat), nel);
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
_split(void *a)
{
	void	**argv;
	Ps	*f, *s;
	Chan	*qchan, *close;
	Fifo	q;
	Rat	r;
	enum{
		REQ,
		QCHAN,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	f = argv[0];
	s = argv[1];
	qchan = argv[2];
	close = argv[3];

	alts[REQ].c = s->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[QCHAN].c = qchan;
	alts[QCHAN].v = &r;
	alts[QCHAN].op = CHANRECV;
	alts[CLOSE].c = s->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	q.front = NULL;
	for(;;){
		switch(alt(alts)){
		case REQ:
			if(q.front == NULL){
				r = getr(f);
				chansend(qchan, &r);
				chansend(s->dat, &r);
			}else{
				r = delete(&q);
				chansend(s->dat, &r);
			}
			break;
		case QCHAN:
			insert(&q, r);
			break;
		case CLOSE:
			while(q.front != NULL)
				delete(&q);
			psfree(s);
			if(chanclosing(qchan) == -1){
				if(close != NULL){
					chansend(close, NULL);
					chanfree(close);
				}
				chanclose(qchan);
			}else{
				chanfree(qchan);
				chansend(f->close, NULL);
			}
			if(debug) threadcount--;
			taskexit();
		}
	}
}

void
split(Ps *f, Ps *s[2], Chan *splitclose)
{
	void	*argv[2][4];
	Chan	*q;
	int	i;

	q = channew(sizeof(Rat), 0);
	for(i = 0; i < 2; i++){
		s[i] = psmk(0);
		argv[i][0] = f;
		argv[i][1] = s[i];
		argv[i][2] = q;
		argv[i][3] = splitclose;
		threadcreate(_split, argv[i], STK);
		if(debug) threadcount++;
		taskyield();
	}
}

static void
_mkconst(void *a)
{
	void	**argv;
	Ps	*o;
	Rat	c;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	c = *(Rat*)argv[0];
	o = argv[1];

	alts[REQ].c = o->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			chansend(o->dat, &c);
			break;
		case CLOSE:
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
mkconst(Rat c)
{
	void	*argv[2];
	Ps	*o;

	o = psmk(0);
	argv[0] = &c;
	argv[1] = o;
	threadcreate(_mkconst, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return o;
}

Ps*
binop(void (*oper)(void*), Ps *f, Ps *g)
{
	Ps *argv[3], *o;

	o = psmk(0);
	argv[0] = f;
	argv[1] = g;
	argv[2] = o;
	threadcreate(oper, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return o;
}

static void
_psadd(void *a)
{
	Ps	**argv, *f, *g, *s;
	Rat	r;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	f = argv[0];
	g = argv[1];
	s = argv[2];

	alts[REQ].c = s->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = s->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			r = ratadd(getr(f), getr(g));
			chansend(s->dat, &r);
			break;
		case CLOSE:
			psfree(s);
			chansend(f->close, NULL);
			chansend(g->close, NULL);
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
psadd(Ps *f, Ps *g)
{
	return binop(_psadd, f, g);
}

static void
_psderiv(void *a)
{
	Ps	**argv, *f, *d;
	Rat	t, r;
	int	i;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	f = argv[0];
	d = argv[1];

	alts[REQ].c = d->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = d->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	getr(f);
	for(i = 1;; i++){
		switch(alt(alts)){
		case REQ:
			r = getr(f);
			t = ratmk(i * r.num, r.den);
			chansend(d->dat, &t);
			break;
		case CLOSE:
			psfree(d);
			chansend(f->close, NULL);
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
psderiv(Ps *f)
{
	Ps	*argv[2], *d = psmk(0);

	argv[0] = f;
	argv[1] = d;
	threadcreate(_psderiv, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return d;
}

void
psprint(Ps *ps, int n)
{
	int	i;

	for(i = 0; i < n - 1; i++){
		ratprint(getr(ps));
		printf(", ");
	}
	ratprint(getr(ps));
	printf("\n");
}

static void
_psinteg(void *a)
{
	void	**argv = a;
	Ps	*f, *i;
	Rat	c, out;
	int	j;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt alts[NALT+1];

	f = argv[0];
	c = *(Rat*)argv[1];
	i = argv[2];

	alts[REQ].c = i->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = i->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		chansend(i->dat, &c);
		break;
	case CLOSE:
		goto End;
	}
	for(j = 1;; j++){
		switch(alt(alts)){
		case REQ:
			out = ratmul(getr(f), ratmk(1, j));
			chansend(i->dat, &out);
			break;
		case CLOSE:
			goto End;
		}
	}
End:
	psfree(i);
	chansend(f->close, NULL);
	if(debug) threadcount--;
	taskexit();
}


Ps*
psinteg(Ps *ps, Rat c)
{
	void	*argv[3];
	Ps	*i = psmk(0);

	argv[0] = ps;
	argv[1] = &c;
	argv[2] = i;
	threadcreate(_psinteg, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return i;
}

static void
_pscmul(void *a)
{
	void	**argv = a;
	Ps	*f, *o;
	Rat	c, p;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt 	alts[NALT+1];

	f = argv[0];
	c = *(Rat*)argv[1];
	o = argv[2];

	alts[REQ].c = o->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			p = ratmul(c, getr(f));
			chansend(o->dat, &p);
			break;
		case CLOSE:
			psfree(o);
			chansend(f->close, NULL);
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
pscmul(Rat c, Ps* f)
{
	void	*argv[3];
	Ps	*o = psmk(0);

	argv[0] = f;
	argv[1] = &c;
	argv[2] = o;
	threadcreate(_pscmul, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return o;
}

static void
_psmul(void *a)
{
	Ps	**argv, *f, *g, *p;
	Fifo	fq, gq;
	Node	*fnode, *gnode;
	Rat	sum;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt 	alts[NALT+1];

	argv = a;
	f = argv[0];
	g = argv[1];
	p = argv[2];

	alts[REQ].c = p->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = p->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	fq.front = gq.front = NULL;
	for(;;){
		switch(alt(alts)){
		case REQ:
			insert(&fq, getr(f));
			frontinsert(&gq, getr(g));
			fnode = fq.front;
			gnode = gq.front;
			sum = ratmk(0, 1);
			while(fnode != NULL){
				sum = ratadd(sum, ratmul(fnode->val, gnode->val));
				fnode = fnode->link;
				gnode = gnode->link;	
			}
			chansend(p->dat, &sum);
			break;
		case CLOSE:
			psfree(p);
			while(fq.front != NULL)
				delete(&fq);
			while(gq.front != NULL)
				delete(&gq);
			chansend(f->close, NULL);
			chansend(g->close, NULL);
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
psmul(Ps *f, Ps *g)
{
	return binop(_psmul, f, g);
}

static void
_psrecip(void *a)
{
	void	**argv;
	Ps	*f, *r, *rr, *recip;
	Chan	*close;
	Rat	g;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt alts[NALT+1];

	argv = a;
	f = argv[0];
	r = argv[1];
	rr = argv[2];
	close = argv[3];

	alts[REQ].c = r->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		g = ratrecip(getr(f));
		chansend(r->dat, &g);
		break;
	case CLOSE:
		chansend(rr->close, NULL);
		chansend(f->close, NULL);
		goto End;
	}
	recip = pscmul(ratneg(g), psmul(f, rr));
	for(;;){
		switch(alt(alts)){
		case REQ:
			g = getr(recip);
			chansend(r->dat, &g);
			break;
		case CLOSE:
			chansend(recip->close, NULL);
			goto End;
		}
	}
End:
	chanrecv(r->close, NULL);
	psfree(r);
	if(debug) threadcount--;
	taskexit();
}

Ps*
psrecip(Ps *f)
{
	void	*argv[4];
	Ps	*rr[2], *r = psmk(0);
	Chan	*close = channew(1, 0);

	split(r, rr, close);
	argv[0] = f;
	argv[1] = r;
	argv[2] = rr[0];
	argv[3] = close;
	threadcreate(_psrecip, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return rr[1];
}

static void
_psmsubst(void *a)
{
	void	**argv;
	Ps	*f, *m;
	Rat	c, zero, r;
	int	deg, i, j;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	f = argv[0];
	c = *(Rat*)argv[1];
	deg = *(int*)argv[2];
	m = argv[3];

	alts[REQ].c = m->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = m->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	zero = ratmk(0, 1);
	for(i = 0;; i++){
		switch(alt(alts)){
		case REQ:
			r = ratmul(getr(f), ratpow(c, i));
			chansend(m->dat, &r);
			for(j = 0; j < deg - 1; j++){
				switch(alt(alts)){
				case REQ:
					chansend(m->dat, &zero);
					break;
				case CLOSE:
					goto End;
				}
			}
			break;
		case CLOSE:
			goto End;
		}
	}
End:
	psfree(m);
	chansend(f->close, NULL);
	if(debug) threadcount--;
	taskexit();
}

Ps*
psmsubst(Ps *f, Rat c, int deg)
{
	void	*argv[4];
	Ps	*m;

	m = psmk(0);
	argv[0] = f;
	argv[1] = &c;
	argv[2] = &deg;
	argv[3] = m;
	threadcreate(_psmsubst, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return m;
}

Ps *pssubst(Ps*, Ps*);

static void
_pssubst(void *a)
{
	Ps	**argv, *gg[2], *f, *g, *o, *subst;
	Rat	r;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt	alts[NALT+1];

	argv = a;
	f = argv[0];
	g = argv[1];
	o = argv[2];

	alts[REQ].c = o->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		r = getr(f);
		chansend(o->dat, &r);
		break;
	case CLOSE:
		psfree(o);
		chansend(f->close, NULL);
		chansend(g->close, NULL);
		if(debug) threadcount--;
		taskexit();
	}
	split(g, gg, NULL);
	getr(gg[0]);
	subst = psmul(gg[0], pssubst(f, gg[1]));
	for(;;){
		switch(alt(alts)){
		case REQ:
			r = getr(subst);
			chansend(o->dat, &r);
			break;
		case CLOSE:
			psfree(o);
			chansend(subst->close, NULL);
			if(debug) threadcount--;
			taskexit();
		}
	}
}

Ps*
pssubst(Ps *f, Ps *g)
{
	return binop(_pssubst, f, g);
}

static void
_psrev(void *a)
{
	void	**argv;
	Ps	*f, *r, *rr, *rever;
	Chan	*close;
	Rat	g;
	enum{
		REQ,
		CLOSE,
		NALT
	};
	Alt alts[NALT+1];

	argv = a;
	f = argv[0];
	r = argv[1];
	rr = argv[2];
	close = argv[3];

	alts[REQ].c = r->req;
	alts[REQ].v = NULL;
	alts[REQ].op = CHANRECV;
	alts[CLOSE].c = close;
	alts[CLOSE].v = NULL;
	alts[CLOSE].op = CHANRECV;
	alts[NALT].op = CHANEND;

	g = ratmk(0, 1);
	switch(alt(alts)){
	case REQ:
		chansend(r->dat, &g);
		break;
	case CLOSE:
		chansend(rr->close, NULL);
		chansend(f->close, NULL);
		goto End;
	}
	getr(f);
	rever = psrecip(pssubst(f, rr));
	for(;;){
		switch(alt(alts)){
		case REQ:
			g = getr(rever);
			chansend(r->dat, &g);
			break;
		case CLOSE:
			chansend(rever->close, NULL);
			goto End;
		}
	}
End:
	chanrecv(r->close, NULL);
	psfree(r);
	if(debug) threadcount--;
	taskexit();
}

Ps*
psrev(Ps *f)
{
	void	*argv[4];
	Ps	*rr[2], *r;
	Chan	*close = channew(1,0);

	r = psmk(0);
	split(r, rr, close);
	argv[0] = f;
	argv[1] = r;
	argv[2] = rr[0];
	argv[3] = close;
	threadcreate(_psrev, argv, STK);
	if(debug) threadcount++;
	taskyield();
	return rr[1];
}

void
threadmain(int argc, char *argv[])
{
	Ps	*ps1, *ps2, *pssum, *ints, *tanx, *pspair[2];
	Rat	one, zero;

/*	ratfmtinstall();*/

	printf("here");
	zero = ratmk(0, 1);
	one = ratmk(1, 1);

	ps1 = mkconst(one);
	printf("1 1 1 1 1 1 1 1 1 1\n");
	psprint(ps1, 10);
	printf("\n");

	ps2 = mkconst(one);
	pssum = psadd(ps1, ps2);
	printf("2 2 2 2 2 2 2 2 2 2\n");
	psprint(pssum, 10);
	printf("\n");
	chansend(pssum->close, NULL);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	printf("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ints, 10);
	printf("\n");
	chansend(ints->close, NULL);

	ps1 = mkconst(one);
	ps2 = psinteg(ps1, one);
	printf("1 1 1/2 1/3 1/4 1/5 1/6 1/7 1/8 1/9\n");
	psprint(ps2, 10);
	printf("\n");
	chansend(ps2->close, NULL);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	split(ints, pspair, NULL);
	printf("1 2 3 4 5 6 7 8 9 10\n");
	printf("1 2 3 4 5 6 7 8 9 10\n");
	psprint(pspair[0], 10);
	psprint(pspair[1], 10);
	printf("\n");
	chansend(pspair[0]->close, NULL);
	chansend(pspair[1]->close, NULL);

	ps1 = mkconst(one);
	ps2 = pscmul(ratmk(2, 1), ps1);
	printf("2 2 2 2 2 2 2 2 2 2\n");
	psprint(ps2, 10);
	printf("\n");
	chansend(ps2->close, NULL);

	ps1 = mkconst(one);
	split(ps1, pspair, NULL);
	ps2 = psmul(pspair[0], pspair[1]);
	printf("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ps2, 10);
	printf("\n");
	chansend(ps2->close, NULL);

	ps1 = mkconst(one);
	ps2 = psrecip(ps1);
	printf("1 -1 0 0 0 0 0 0 0 0\n");
	psprint(ps2, 10);
	printf("\n");
	chansend(ps2->close, NULL);

	ps1 = mkconst(one);
	tanx = psrev(psinteg(psmsubst(ps1, ratmk(-1, 1), 2), zero));
	printf("0 1 0 1/3 0 2/15 0 17/315 0 62/2835 0 1382/155925\n");
	psprint(tanx, 12);
	printf("\n");
	chansend(tanx->close, NULL);

	exit(0);
}
