CC=gcc
ECFLAGS=
OCFLAGS=$(ECFLAGS) -O2
CFLAGS=$(OCFLAGS) -g -DGC -I..
LD=$(CC)
LDFLAGS=
GC_LIBS=-lgc
GGGGC_LIBS=../libggggc.a
LIBS=-lm

BTOBJS=binary_trees_td.o
BTGCOBJS=binary_trees_gc_td.o
BTGGGGCOBJS=binary_trees_ggggc_td.o

BADLLLOBJS=badlll.o

REMEMBEROBJS=remember.o

GCBENCHOBJS=../gc_bench/GCBench.o

GGGGCBENCHOBJS=../gc_bench/GCBench.ggggc.o

all: bt btgc btggggc badlll remember gcbench ggggcbench

bt: $(BTOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTOBJS) $(LIBS) -o bt

btgc: $(BTGCOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGCOBJS) $(GC_LIBS) $(LIBS) -o btgc

btggggc: $(BTGGGGCOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGGGGCOBJS) $(GGGGC_LIBS) $(LIBS) -o btggggc

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
	rm -f $(BTOBJS) bt
	rm -f $(BTGCOBJS) btgc
	rm -f $(BTGGGGCOBJS) btggggc
	rm -f $(BADLLLOBJS) badlll
	rm -f $(REMEMBEROBJS) remember
	rm -f $(GCBENCHOBJS) gcbench
	rm -f $(GGGGCBENCHOBJS) ggggcbench
