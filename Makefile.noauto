CC=gcc
ECFLAGS=
OCFLAGS=$(ECFLAGS) -O2
CFLAGS=$(OCFLAGS) -g -Wall -Werror -ansi -pedantic
AR=ar
ARFLAGS=rc
RANLIB=ranlib

OBJS=alloc.o collector.o globals.o init.o

all: libggggc.a

libggggc.a: $(OBJS)
	$(AR) $(ARFLAGS) libggggc.a $(OBJS)
	$(RANLIB) libggggc.a

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) ggggc

include deps

deps:
	$(CC) -MM *.c > deps
