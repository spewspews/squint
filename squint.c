#include <u.h>
#include <libc.h>
#include <thread.h>
#include "rat.h"
#include "fifo.h"

int debug;
int threadcount;

enum
{
	TMAX = 1000,
	STK = 2048
};

typedef struct Ps Ps;
struct Ps
{
	Channel *close;
	Channel *req;
	Channel	*dat;
};

Rat
getr(Ps *f)
{
	Rat	r;

	send(f->req, nil);
	recv(f->dat, &r);
	return r;
}

Ps*
psmk(int nel)
{
	Ps	*d;

	d = malloc(sizeof(*d));
	d->close = chancreate(1, 0);
	d->req = chancreate(1, 0);
	d->dat = chancreate(sizeof(Rat), nel);
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
	Channel	*qchan, *close;
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[QCHAN].c = qchan;
	alts[QCHAN].v = &r;
	alts[QCHAN].op = CHANRCV;
	alts[CLOSE].c = s->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	q.front = nil;
	for(;;){
		switch(alt(alts)){
		case REQ:
			if(q.front == nil){
				r = getr(f);
				send(qchan, &r);
				send(s->dat, &r);
			}else{
				r = delete(&q);
				send(s->dat, &r);
			}
			break;
		case QCHAN:
			insert(&q, r);
			break;
		case CLOSE:
			while(q.front != nil)
				delete(&q);
			psfree(s);
			if(chanclosing(qchan) == -1){
				if(close != nil){
					send(close, nil);
					chanfree(close);
				}
				chanclose(qchan);
			}else{
				chanfree(qchan);
				send(f->close, nil);
			}
			if(debug) threadcount--;
			threadexits(0);
		}
	}
}

void
split(Ps *f, Ps *s[2], Channel *splitclose)
{
	void	*argv[2][4];
	Channel	*q;
	int	i;

	q = chancreate(sizeof(Rat), 0);
	for(i = 0; i < 2; i++){
		s[i] = psmk(0);
		argv[i][0] = f;
		argv[i][1] = s[i];
		argv[i][2] = q;
		argv[i][3] = splitclose;
		threadcreate(_split, argv[i], STK);
		if(debug) threadcount++;
		yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			send(o->dat, &c);
			break;
		case CLOSE:
			if(debug) threadcount--;
			threadexits(0);
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
	yield();
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
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = s->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			r = ratadd(getr(f), getr(g));
			send(s->dat, &r);
			break;
		case CLOSE:
			psfree(s);
			send(f->close, nil);
			send(g->close, nil);
			if(debug) threadcount--;
			threadexits(0);
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = d->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	getr(f);
	for(i = 1;; i++){
		switch(alt(alts)){
		case REQ:
			r = getr(f);
			t = ratmk(i * r.num, r.den);
			send(d->dat, &t);
			break;
		case CLOSE:
			psfree(d);
			send(f->close, nil);
			if(debug) threadcount--;
			threadexits(0);
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
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = i->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		send(i->dat, &c);
		break;
	case CLOSE:
		goto End;
	}
	for(j = 1;; j++){
		switch(alt(alts)){
		case REQ:
			out = ratmul(getr(f), ratmk(1, j));
			send(i->dat, &out);
			break;
		case CLOSE:
			goto End;
		}
	}
End:
	psfree(i);
	send(f->close, nil);
	if(debug) threadcount--;
	threadexits(0);
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
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	for(;;){
		switch(alt(alts)){
		case REQ:
			p = ratmul(c, getr(f));
			send(o->dat, &p);
			break;
		case CLOSE:
			psfree(o);
			send(f->close, nil);
			if(debug) threadcount--;
			threadexits(0);
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
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = p->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	fq.front = gq.front = nil;
	for(;;){
		switch(alt(alts)){
		case REQ:
			insert(&fq, getr(f));
			frontinsert(&gq, getr(g));
			fnode = fq.front;
			gnode = gq.front;
			sum = ratmk(0, 1);
			while(fnode != nil){
				sum = ratadd(sum, ratmul(fnode->val, gnode->val));
				fnode = fnode->link;
				gnode = gnode->link;	
			}
			send(p->dat, &sum);
			break;
		case CLOSE:
			psfree(p);
			while(fq.front != nil)
				delete(&fq);
			while(gq.front != nil)
				delete(&gq);
			send(f->close, nil);
			send(g->close, nil);
			if(debug) threadcount--;
			threadexits(0);
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
	Channel	*close;
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		g = ratrecip(getr(f));
		send(r->dat, &g);
		break;
	case CLOSE:
		send(rr->close, nil);
		send(f->close, nil);
		goto End;
	}
	recip = pscmul(ratneg(g), psmul(f, rr));
	for(;;){
		switch(alt(alts)){
		case REQ:
			g = getr(recip);
			send(r->dat, &g);
			break;
		case CLOSE:
			send(recip->close, nil);
			goto End;
		}
	}
End:
	recv(r->close, nil);
	psfree(r);
	if(debug) threadcount--;
	threadexits(0);
}

Ps*
psrecip(Ps *f)
{
	void	*argv[4];
	Ps	*rr[2], *r = psmk(0);
	Channel	*close = chancreate(1, 0);

	split(r, rr, close);
	argv[0] = f;
	argv[1] = r;
	argv[2] = rr[0];
	argv[3] = close;
	threadcreate(_psrecip, argv, STK);
	if(debug) threadcount++;
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = m->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	zero = ratmk(0, 1);
	for(i = 0;; i++){
		switch(alt(alts)){
		case REQ:
			r = ratmul(getr(f), ratpow(c, i));
			send(m->dat, &r);
			for(j = 0; j < deg - 1; j++){
				switch(alt(alts)){
				case REQ:
					send(m->dat, &zero);
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
	send(f->close, nil);
	if(debug) threadcount--;
	threadexits(0);
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
	yield();
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = o->close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	switch(alt(alts)){
	case REQ:
		r = getr(f);
		send(o->dat, &r);
		break;
	case CLOSE:
		psfree(o);
		send(f->close, nil);
		send(g->close, nil);
		if(debug) threadcount--;
		threadexits(0);
	}
	split(g, gg, nil);
	getr(gg[0]);
	subst = psmul(gg[0], pssubst(f, gg[1]));
	for(;;){
		switch(alt(alts)){
		case REQ:
			r = getr(subst);
			send(o->dat, &r);
			break;
		case CLOSE:
			psfree(o);
			send(subst->close, nil);
			if(debug) threadcount--;
			threadexits(0);
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
	Channel	*close;
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
	alts[REQ].v = nil;
	alts[REQ].op = CHANRCV;
	alts[CLOSE].c = close;
	alts[CLOSE].v = nil;
	alts[CLOSE].op = CHANRCV;
	alts[NALT].op = CHANEND;

	g = ratmk(0, 1);
	switch(alt(alts)){
	case REQ:
		send(r->dat, &g);
		break;
	case CLOSE:
		send(rr->close, nil);
		send(f->close, nil);
		goto End;
	}
	getr(f);
	rever = psrecip(pssubst(f, rr));
	for(;;){
		switch(alt(alts)){
		case REQ:
			g = getr(rever);
			send(r->dat, &g);
			break;
		case CLOSE:
			send(rever->close, nil);
			goto End;
		}
	}
End:
	recv(r->close, nil);
	psfree(r);
	if(debug) threadcount--;
	threadexits(0);
}

Ps*
psrev(Ps *f)
{
	void	*argv[4];
	Ps	*rr[2], *r;
	Channel	*close = chancreate(1,0);

	r = psmk(0);
	split(r, rr, close);
	argv[0] = f;
	argv[1] = r;
	argv[2] = rr[0];
	argv[3] = close;
	threadcreate(_psrev, argv, STK);
	if(debug) threadcount++;
	yield();
	return rr[1];
}

void
threadmain(int argc, char *argv[])
{
	Ps	*ps1, *ps2, *pssum, *ints, *tanx, *pspair[2];
	Rat	one, zero;

	ratfmtinstall();

	zero = ratmk(0, 1);
	one = ratmk(1, 1);

	ps1 = mkconst(one);
	print("1 1 1 1 1 1 1 1 1 1\n");
	psprint(ps1, 10);
	print("\n");

	ps2 = mkconst(one);
	pssum = psadd(ps1, ps2);
	print("2 2 2 2 2 2 2 2 2 2\n");
	psprint(pssum, 10);
	print("\n");
	send(pssum->close, nil);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ints, 10);
	print("\n");
	send(ints->close, nil);

	ps1 = mkconst(one);
	ps2 = psinteg(ps1, one);
	print("1 1 1/2 1/3 1/4 1/5 1/6 1/7 1/8 1/9\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	ints = psderiv(ps1);
	split(ints, pspair, nil);
	print("1 2 3 4 5 6 7 8 9 10\n");
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(pspair[0], 10);
	psprint(pspair[1], 10);
	print("\n");
	send(pspair[0]->close, nil);
	send(pspair[1]->close, nil);

	ps1 = mkconst(one);
	ps2 = pscmul(ratmk(2, 1), ps1);
	print("2 2 2 2 2 2 2 2 2 2\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	split(ps1, pspair, nil);
	ps2 = psmul(pspair[0], pspair[1]);
	print("1 2 3 4 5 6 7 8 9 10\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	ps2 = psrecip(ps1);
	print("1 -1 0 0 0 0 0 0 0 0\n");
	psprint(ps2, 10);
	print("\n");
	send(ps2->close, nil);

	ps1 = mkconst(one);
	tanx = psrev(psinteg(psmsubst(ps1, ratmk(-1, 1), 2), zero));
	print("0 1 0 1/3 0 2/15 0 17/315 0 62/2835 0 1382/155925\n");
	psprint(tanx, 12);
	print("\n");
	send(tanx->close, nil);

	threadexitsall(0);
}
