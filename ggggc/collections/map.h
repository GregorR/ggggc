/*
 * Map collections for GGGGC
 *
 * Copyright (c) 2014 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef GGGGC_COLLECTIONS_MAP_H
#define GGGGC_COLLECTIONS_MAP_H 1

#include <string.h>

#include "../gc.h"

/* FIXME: collision support is very preliminary */

/* declarations for a map:
 * name: Name of the map type
 * typeK: Type of keys
 * typeV: Type of values
 */
#define GGGGC_MAP_DECL(name, typeK, typeV, static) \
GGC_TYPE(name) \
    GGC_MDATA(ggc_size_t, size); \
    GGC_MPTR(typeK ## Array, keys); \
    GGC_MPTR(typeV ## Array, values); \
GGC_END_TYPE(name, \
    GGC_PTR(name, keys) \
    GGC_PTR(name, values) \
    ); \
static int name ## Get(name map, typeK key, typeV *value); \
static void name ## Put(name map, typeK key, typeV value); \
static name name ## Clone(name map)

#define GGC_MAP_DECL_STATIC(name, typeK, typeV) \
    GGGGC_MAP_DECL(name, typeK, typeV, static)
#define GGC_MAP_DECL(name, typeK, typeV) \
    GGGGC_MAP_DECL(name, typeK, typeV, GGGGC_EMPTY)

/* definitions for a map
 * hash: Hash function (typeK)->size_t
 * cmp: Comparison function (typeK,typeK)->int with 0 as equal
 */
#define GGGGC_MAP_DEFN(name, typeK, typeV, hash, cmp, static) \
static int name ## Get(name map, typeK key, typeV *value) \
{ \
    typeK keyCmp = NULL; \
    size_t hashV; \
    \
    GGC_PUSH_3(map, key, keyCmp); \
    \
    if (GGC_RD(map, size) == 0) \
        return 0; \
    \
    hashV = hash(key) % GGC_RD(map, size); \
    keyCmp = GGC_RAP(GGC_RP(map, keys), hashV); \
    if (keyCmp && cmp(key, keyCmp) == 0) { \
        *value = GGC_RAP(GGC_RP(map, values), hashV); \
        return 1; \
    } \
    \
    return 0; \
} \
static void name ## Put(name map, typeK key, typeV value) \
{ \
    typeK keyCmp = NULL; \
    typeV valueCmp = NULL; \
    typeK ## Array newKeys = NULL; \
    typeV ## Array newValues = NULL; \
    size_t hashV, hashV2; \
    ggc_size_t newSize, i; \
    \
    GGC_PUSH_7(map, key, value, keyCmp, valueCmp, newKeys, newValues); \
    \
    if (GGC_RD(map, size) == 0) { \
        /* start with something */ \
        GGC_WD(map, size, 4); \
        newKeys = GGC_NEW_PA(typeK, 4); \
        newValues = GGC_NEW_PA(typeV, 4); \
        GGC_WP(map, keys, newKeys); \
        GGC_WP(map, values, newValues); \
    } \
    \
    while(1) { \
        hashV = hash(key) % GGC_RD(map, size); \
        \
        /* check for a current key */ \
        keyCmp = GGC_RAP(GGC_RP(map, keys), hashV); \
        if (keyCmp && cmp(key, keyCmp) != 0) { \
            /* key found but doesn't match! Expand */ \
            hashV2 = hashV; \
            newSize = GGC_RD(map, size); \
            while (hashV == hashV2) { \
                newSize *= 2; \
                hashV = hash(key) % newSize; \
                hashV2 = hash(keyCmp) % newSize; \
            } \
            \
            /* now reform it */ \
            newKeys = GGC_NEW_PA(typeK, newSize); \
            newValues = GGC_NEW_PA(typeV, newSize); \
            for (i = 0; i < GGC_RD(map, size); i++) { \
                keyCmp = GGC_RAP(GGC_RP(map, keys), i); \
                if (keyCmp) { \
                    valueCmp = GGC_RAP(GGC_RP(map, values), i); \
                    hashV = hash(keyCmp) % newSize; \
                    GGC_WAP(newKeys, hashV, keyCmp); \
                    GGC_WAP(newValues, hashV, valueCmp); \
                } \
            } \
            \
            GGC_WD(map, size, newSize); \
            GGC_WP(map, keys, newKeys); \
            GGC_WP(map, values, newValues); \
            \
        } else { \
            /* found our slot */ \
            newKeys = GGC_RP(map, keys); \
            newValues = GGC_RP(map, values); \
            GGC_WAP(newKeys, hashV, key); \
            GGC_WAP(newValues, hashV, value); \
            break; \
            \
        } \
    } \
} \
static name name ## Clone(name map) \
{ \
    name ret = NULL; \
    typeK ## Array oldKeys = NULL, newKeys = NULL; \
    typeV ## Array oldValues = NULL, newValues = NULL; \
    ggc_size_t size; \
    \
    GGC_PUSH_6(map, ret, oldKeys, newKeys, oldValues, newValues); \
    \
    ret = GGC_NEW(name); \
    /* if it's empty, no further work */ \
    if (GGC_RD(map, size) == 0) return ret; \
    \
    size = GGC_RD(map, size); \
    oldKeys = GGC_RP(map, keys); \
    oldValues = GGC_RP(map, values); \
    \
    newKeys = GGC_NEW_PA(typeK, oldKeys->length); \
    newValues = GGC_NEW_PA(typeV, oldValues->length); \
    \
    memcpy(newKeys->a__ptrs, oldKeys->a__ptrs, oldKeys->length * sizeof(typeK)); \
    memcpy(newValues->a__ptrs, oldValues->a__ptrs, oldValues->length * sizeof(typeV)); \
    \
    GGC_WD(ret, size, size); \
    GGC_WP(ret, keys, newKeys); \
    GGC_WP(ret, values, newValues); \
    \
    return ret; \
}

#define GGC_MAP_DEFN_STATIC(name, typeK, typeV, hash, cmp) \
    GGGGC_MAP_DEFN(name, typeK, typeV, hash, cmp, static)
#define GGC_MAP_DEFN(name, typeK, typeV, hash, cmp) \
    GGGGC_MAP_DEFN(name, typeK, typeV, hash, cmp, GGGGC_EMPTY)

/* and both together */
#define GGGGC_MAP(name, typeK, typeV, hash, cmp, static) \
    GGGGC_MAP_DECL(name, typeK, typeV, static); \
    GGGGC_MAP_DEFN(name, typeK, typeV, hash, cmp, static)

#define GGC_MAP_STATIC(name, typeK, typeV, hash, cmp) \
    GGGGC_MAP(name, typeK, typeV, hash, cmp, static)

#endif
