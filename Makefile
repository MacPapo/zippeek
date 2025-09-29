CC?=gcc
LDFLAGS?=

PREFIX?=.
INCDIR=${PREFIX}/include
SRCDIR=${PREFIX}/src

PROG=zippeek

# CFLAGS per lo sviluppo (massimi controlli)
DEBUG_CFLAGS:=-Wall -Wextra -g -pedantic -std=c23 -I${INCDIR} \
			  -fsanitize=address -fsanitize=undefined \
			  -fstack-protector-strong

# CFLAGS per il rilascio (ottimizzazione e sicurezza)
RELEASE_CFLAGS:=-Wall -Wextra -O3 -pedantic -std=c23 -I${INCDIR} \
				-D_FORTIFY_SOURCE=2 -fstack-protector-strong -march=native

# Usa i CFLAGS di debug di default
CFLAGS?=${DEBUG_CFLAGS}

SRCS=$(shell find ${SRCDIR} -type f -name '*.c' | sort)
OBJS=${SRCS:.c=.o}

all: ${PROG}

# Target specifico per compilare la versione di rilascio
release: CFLAGS=${RELEASE_CFLAGS}
release: all

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS} ${LDFLAGS}

.SUFFIXES: .c .o

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f ${OBJS} ${PROG}

compdb:
	bear -- make clean all

.PHONY: all clean release
