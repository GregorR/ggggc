CC=gcc
ECFLAGS=-O2 -g -Wall -Werror -ansi -pedantic
CFLAGS=$(ECFLAGS)
AR=ar
ARFLAGS=rc
RANLIB=ranlib

OBJS=alloc.o collector.o gcthreads.o globals.o init.o

all: libggggc.a

libggggc.a: $(OBJS)
	$(AR) $(ARFLAGS) libggggc.a $(OBJS)
	$(RANLIB) libggggc.a

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libggggc.a

include deps

deps:
	$(CC) -MM *.c > deps
