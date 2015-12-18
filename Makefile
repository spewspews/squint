BIN=	/usr/local/bin
TARG=	squint
OBJS=	rat.o\
	fifo.o\
	squint.o\

HFILES=	rat.h\
	fifo.h\

CC=	clang
DEBUG=	-g
CFLAGS=	-I /usr/local/include -O2 -pipe ${DEBUG} -Wall
LDFLAGS=-L/usr/local/lib -static
LDADD=	-lmultitask -lm -lpthread

all:	${TARG}

install:	${TARG} ${TARG}.1
	install -s -m 555 -g bin ${TARG} ${BIN}

${TARG}:	${OBJS} ${HFILES}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${TARG} ${OBJS}
