/*
 * Map collections for GGGGC
 *
 * Copyright (c) 2014, 2015 Gregor Richards
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

#include "../gc.h"

/* generic map type */
GGC_TYPE(GGC_MapEntry)
    GGC_MPTR(GGC_MapEntry, next);
    GGC_MPTR(void *, key);
    GGC_MPTR(void *, value);
GGC_END_TYPE(GGC_MapEntry,
    GGC_PTR(GGC_MapEntry, next)
    GGC_PTR(GGC_MapEntry, key)
    GGC_PTR(GGC_MapEntry, value)
    )

GGC_TYPE(GGC_Map)
    GGC_MDATA(ggc_size_t, size);
    GGC_MDATA(ggc_size_t, used);
    GGC_MPTR(GGC_MapEntryArray, entries);
GGC_END_TYPE(GGC_Map,
    GGC_PTR(GGC_Map, entries)
    )

/* type for hash functions */
typedef size_t (*ggc_map_hash_t)(void *);

/* type for comparison functions */
typedef int (*ggc_map_cmp_t)(void *, void *);

/* get an element out of a map */
int GGC_MapGet(GGC_Map map, void *key, void **value, ggc_map_hash_t hash, ggc_map_cmp_t cmp);

/* put an element in a map */
void GGC_MapPut(GGC_Map map, void *key, void *value, ggc_map_hash_t hash, ggc_map_cmp_t cmp);

/* clone a map */
GGC_Map GGC_MapClone(GGC_Map map);

/* declarations for a typed map:
 * name: Name of the map type
 * typeK: Type of keys
 * typeV: Type of values
 * hash: hash function (typeK)->size_t
 * cmp: comparison function (typeK,typeK)->int with 0 as equal
 */
#define GGC_MAP(name, typeK, typeV, hash, cmp) \
GGC_TYPE(name ## Entry) \
    GGC_MPTR(name ## Entry, next); \
    GGC_MPTR(typeK, key); \
    GGC_MPTR(typeV, value); \
GGC_END_TYPE(name ## Entry, \
    GGC_PTR(name ## Entry, next) \
    GGC_PTR(name ## Entry, key) \
    GGC_PTR(name ## Entry, value) \
    ); \
GGC_TYPE(name) \
    GGC_MDATA(ggc_size_t, size); \
    GGC_MDATA(ggc_size_t, used); \
    GGC_MPTR(name ## EntryArray, entries); \
GGC_END_TYPE(name, \
    GGC_PTR(name, entries) \
    ); \
static int name ## Get(name map, typeK key, typeV *value) \
{ \
    return GGC_MapGet((GGC_Map) map, key, (void **) value, \
                      (ggc_map_hash_t) (hash), (ggc_map_cmp_t) (cmp)); \
} \
static void name ## Put(name map, typeK key, typeV value) \
{ \
    GGC_MapPut((GGC_Map) map, key, value, \
               (ggc_map_hash_t) (hash), (ggc_map_cmp_t) (cmp)); \
} \
static name name ## Clone(name map) \
{ \
    return (name) GGC_MapClone((GGC_Map) map); \
}

#endif
