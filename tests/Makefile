CC=gcc
CXX=g++
ECFLAGS=
OCFLAGS=$(ECFLAGS) -O2
CFLAGS=$(OCFLAGS) -g -DGC -I..
LD=$(CC)
LDFLAGS=
GC_LIBS=-lgc
GGGGC_LIBS=../libggggc.a -pthread
LIBS=-lm

BTOBJS=binary_trees_td.o
BTGCOBJS=binary_trees_gc_td.o
BTGGGGCOBJS=binary_trees_ggggc_td.o
BTGGGGCTHOBJS=binary_trees_ggggc_td_th.o

BADLLLOBJS=badlll.o

REMEMBEROBJS=remember.o

GCBENCHOBJS=gc_bench/GCBench.o

GGGGCBENCHOBJS=gc_bench/GCBench.ggggc.o

LISTSOBJS=lists.o
MAPSOBJS=maps.o

GRAPHOBJS=graph.o

all: bt btgc btggggc btggggcth badlll gcbench ggggcbench lists maps graph graphpp

bt: $(BTOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTOBJS) $(LIBS) -o bt

btgc: $(BTGCOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGCOBJS) $(GC_LIBS) $(LIBS) -o btgc

btggggc: $(BTGGGGCOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGGGGCOBJS) $(GGGGC_LIBS) $(LIBS) -o btggggc

btggggcth: $(BTGGGGCTHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BTGGGGCTHOBJS) $(GGGGC_LIBS) $(LIBS) -o btggggcth

badlll: $(BADLLLOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(BADLLLOBJS) $(GGGGC_LIBS) $(LIBS) -o badlll

remember: $(REMEMBEROBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(REMEMBEROBJS) $(GGGGC_LIBS) $(LIBS) -o remember

gcbench: $(GCBENCHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(GCBENCHOBJS) $(GC_LIBS) $(LIBS) -o gcbench

ggggcbench: $(GGGGCBENCHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(GGGGCBENCHOBJS) $(GGGGC_LIBS) $(LIBS) -o ggggcbench

lists: $(LISTSOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(LISTSOBJS) $(GGGGC_LIBS) $(LIBS) -o lists

maps: $(MAPSOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(MAPSOBJS) $(GGGGC_LIBS) $(LIBS) -o maps

graph: $(GRAPHOBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) $(GRAPHOBJS) $(GGGGC_LIBS) $(LIBS) -o graph

graphpp: graph.cc
	$(CXX) $(CFLAGS) $(LDFLAGS) $< $(GGGGC_LIBS) $(LIBS) -o $@

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BTOBJS) bt
	rm -f $(BTGCOBJS) btgc
	rm -f $(BTGGGGCOBJS) btggggc
	rm -f $(BTGGGGCTHOBJS) btggggcth
	rm -f $(BADLLLOBJS) badlll
	rm -f $(REMEMBEROBJS) remember
	rm -f $(GCBENCHOBJS) gcbench
	rm -f $(GGGGCBENCHOBJS) ggggcbench
	rm -f $(LISTSOBJS) lists
	rm -f $(MAPSOBJS) maps
	rm -f $(GRAPHOBJS) graph
	rm -f graphpp
