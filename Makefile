CC=gcc
ECFLAGS=-O0 -g
CFLAGS=$(ECFLAGS)
AR=ar
ARFLAGS=rc
RANLIB=ranlib

OBJS=allocate.o collect.o globals.o roots.o

all: libggggc.a

libggggc.a: $(OBJS)
	$(AR) $(ARFLAGS) libggggc.a $(OBJS)
	$(RANLIB) libggggc.a

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libggggc.a deps

include deps

deps:
	-$(CC) -MM *.c > deps
