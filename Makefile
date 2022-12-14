X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I${X11INC}
LIBS = -L${X11LIB} -lX11

CFLAGS = -std=c99 -O0 -g -pedantic -Wall -Wno-deprecated-declarations ${INCS}
LDFLAGS = -g ${LIBS}

PREFIX = /usr/local
BINDIR = ${PREFIX}/bin

CC = cc
SRC = nuwm.c
OBJ = ${SRC:.c=.o}

all: nuwm

config.h:
	cp config.def.h $@

nuwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f nuwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/nuwm

clean:
	rm -f nuwm ${OBJ}
