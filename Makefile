X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I${X11INC}
LIBS = -L${X11LIB} -lX11

CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS}
LDFLAGS = ${LIBS}

PREFIX = /usr/local
BINDIR = ${PREFIX}/bin

CC = cc
SRC = nuwm.c
OBJ = ${SRC:.c=.o}

all: nuwm

nuwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	install -Dm 755 nuwm ${DESTDIR}${BINDIR}/nuwm

clean:
	rm -f nuwm *.o
