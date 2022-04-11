CC=gcc
ECFLAGS=-O3 -g
CFLAGS=-D_XOPEN_SOURCE=600 -I. $(ECFLAGS)
AR=ar
ARFLAGS=rc
RANLIB=ranlib

PATCH_DEST=../ggggc
PATCHES=

OBJS=allocator.o collector/gembc.o collector/portablems.o globals.o roots.o \
     threads.o collections/list.o collections/map.o

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

patch:
	for i in *.c *.h collections/*.c ggggc/*.h ggggc/collections/*.h; \
	do \
	    if [ ! -e $(PATCH_DEST)/$$i -o $$i -nt $(PATCH_DEST)/$$i ]; \
	    then \
	        mkdir -p $(PATCH_DEST)/`dirname $$i`; \
	        cp $$i $(PATCH_DEST)/$$i; \
	        rm -f $(PATCH_DEST)/Makefile; \
	    fi; \
	done
	[ -e $(PATCH_DEST)/Makefile ] || \
	    for p in $(PATCHES); do cat patches/$$p/*.diff; done | \
	        ( cd $(PATCH_DEST); patch -p1 )
	cp Makefile $(PATCH_DEST)/Makefile

include deps

deps:
	-$(CC) -MM *.c > deps
