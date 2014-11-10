CC=gcc
ECFLAGS=-O3 -g
CFLAGS=-D_XOPEN_SOURCE=600 $(ECFLAGS)
AR=ar
ARFLAGS=rc
RANLIB=ranlib

OBJS=allocate.o collect.o globals.o roots.o threads.o

all: libggggc.a

libggggc.a: $(OBJS)
	$(AR) $(ARFLAGS) libggggc.a $(OBJS)
	$(RANLIB) libggggc.a

%.o: %.c
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
