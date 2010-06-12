CC=gcc
ECFLAGS=-O2 -g -ansi -pedantic
CFLAGS=$(ECFLAGS) -I..
LD=$(CC)
LDFLAGS=
GGGGC_LIBS=../libggggc.a
LIBS=-lm

BTGOBJS=binary_trees_ggggc_td.o
BTOBJS=binary_trees_td.o

BADLLLOBJS=badlll.o

all: btg bt badlll

btg: $(BTGOBJS)
	$(LD) $(LDFLAGS) $(BTGOBJS) $(GGGGC_LIBS) $(LIBS) -o btg

bt: $(BTOBJS)
	$(LD) $(LDFLAGS) $(BTOBJS) $(LIBS) -o bt

badlll: $(BADLLLOBJS)
	$(LD) $(LDFLAGS) $(BADLLLOBJS) $(GGGGC_LIBS) $(LIBS) -o badlll

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BTGOBJS) btg
	rm -f $(BTOBJS) bt
	rm -f $(BADLLLOBJS) badlll
