#ifndef GGGGC_COLLECTION_UNIT_H
#define GGGGC_COLLECTION_UNIT_H 1

#include "../gc.h"

#define GGC_UNIT_DECL(type) \
GGC_TYPE(GGC_ ## type ## _Unit) \
    GGC_MDATA(type, v); \
GGC_END_TYPE(GGC_ ## type ## _Unit, GGC_NO_PTRS)

#define GGC_UNIT(type) GGC_UNIT_DECL(type);

#endif
