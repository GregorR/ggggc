CC=gcc
CFLAGS=-O2 -g -Wall -Werror -ansi -pedantic
LD=$(CC)
LDFLAGS=

OBJS=alloc.o globals.o init.o main.o

all: ggggc

ggggc: $(OBJS)
	$(LD) $(LDFLAGS) $(CFLAGS) $(OBJS) -o ggggc

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) ggggc
