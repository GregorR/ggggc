AUTOMAKE_OPTIONS=foreign

lib_LIBRARIES=libggggc.a

if BOEHM_GC
libggggc_a_SOURCES= \
    alloc_boehm.c
else
libggggc_a_SOURCES= \
    alloc.c \
    collector.c \
    globals.c \
    init.c
endif
