CC=gcc
ECFLAGS=-O0 -g -Wall -Werror -std=c99 -pedantic -D_XOPEN_SOURCE=600
CFLAGS=$(ECFLAGS)
AR=ar
ARFLAGS=rc
RANLIB=ranlib

OBJS=allocate.o collect.o globals.o roots.o threads.o

all: libggggc.a

libggggc.a: $(OBJS)
	$(AR) $(ARFLAGS) libggggc.a $(OBJS)
	$(RANLIB) libggggc.a

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

push:
	$(CC) $(CFLAGS) pushgen.c -o pushgen
	./pushgen > ggggc/push.h
	rm -f pushgen

clean:
	rm -f $(OBJS) libggggc.a deps

include deps

deps:
	-$(CC) -MM *.c > deps
