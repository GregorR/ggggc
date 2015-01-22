#ifndef GGGGC_COLLECTION_UNIT_H
#define GGGGC_COLLECTION_UNIT_H 1

#include "../gc.h"

#define GGC_UNIT(type) \
GGC_TYPE(GGC_ ## type ## _Unit) \
    GGC_MDATA(type, v); \
GGC_END_TYPE(GGC_ ## type ## _Unit, GGC_NO_PTRS) \
static GGC_ ## type ## _Array GGC_ ## type ## _UnitArrayDeunit(GGC_ ## type ## _UnitArray ua) \
{ \
    GGC_ ## type ## _Array ret = NULL; \
    type val; \
    size_t i; \
    \
    GGC_PUSH_2(ua, ret); \
    \
    ret = GGC_NEW_DA(type, ua->length); \
    \
    for (i = 0; i < ua->length; i++) { \
        val = GGC_RD(GGC_RAP(ua, i), v); \
        GGC_WAD(ret, i, val); \
    } \
    \
    return ret; \
}

#endif
