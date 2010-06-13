CC=gcc
ECFLAGS=-O2 -g -DGC
CFLAGS=$(ECFLAGS) -I..
LD=$(CC)
LDFLAGS=
GC_LIBS=-lgc
GGGGC_LIBS=../libggggc.a
LIBS=-lm

BTGOBJS=binary_trees_ggggc_td.o
BTOBJS=binary_trees_td.o

BADLLLOBJS=badlll.o

REMEMBEROBJS=remember.o

GCBENCHOBJS=../gc_bench/GCBench.o

GGGGCBENCHOBJS=../gc_bench/GCBench.ggggc.o

all: btg bt badlll remember gcbench ggggcbench

btg: $(BTGOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGOBJS) $(GGGGC_LIBS) $(LIBS) -o btg

bt: $(BTOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTOBJS) $(LIBS) -o bt

badlll: $(BADLLLOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BADLLLOBJS) $(GGGGC_LIBS) $(LIBS) -o badlll

remember: $(REMEMBEROBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(REMEMBEROBJS) $(GGGGC_LIBS) $(LIBS) -o remember

gcbench: $(GCBENCHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(GCBENCHOBJS) $(GC_LIBS) $(LIBS) -o gcbench

ggggcbench: $(GGGGCBENCHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(GGGGCBENCHOBJS) $(GGGGC_LIBS) $(LIBS) -o ggggcbench

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BTGOBJS) btg
	rm -f $(BTOBJS) bt
	rm -f $(BADLLLOBJS) badlll
	rm -f $(REMEMBEROBJS) remember
	rm -f $(GCBENCHOBJS) gcbench
	rm -f $(GGGGCBENCHOBJS) ggggcbench
