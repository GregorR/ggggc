CC=gcc
CFLAGS=-O2 -g -Wall -Werror -ansi -pedantic
LD=$(CC)
LDFLAGS=
LIBS=-lm

OBJS=alloc.o collector.o globals.o init.o binary_trees_ggggc_td.o

all: ggggc

ggggc: $(OBJS)
	$(LD) $(LDFLAGS) $(CFLAGS) $(OBJS) $(LIBS) -o ggggc

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) ggggc

include deps

deps:
	$(CC) -MM *.c > deps
