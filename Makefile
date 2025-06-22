CC?=gcc
CFLAGS?=-Wall -Wextra -g -pedantic -std=c17 -I${INCDIR}
LDFLAGS?=

PREFIX?=.
INCDIR=${PREFIX}/include
SRCDIR=${PREFIX}/src

PROG=zippeek

SRCS=$(shell find ${SRCDIR} -type f -name '*.c' | sort)
OBJS=${SRCS:.c=.o}

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS}

.SUFFIXES: .c .o

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f ${OBJS} ${PROG}

compdb:
	bear -- make clean all

.PHONY: all clean
