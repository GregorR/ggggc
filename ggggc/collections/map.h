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

#include "../gc.h"

/* declarations for a map:
 * name: Name of the map type
 * typeK: Type of keys
 * typeV: Type of values
 */
#define GGGGC_MAP_DECL(name, typeK, typeV, static) \
GGC_TYPE(name ## Entry) \
    GGC_MPTR(name ## Entry, next); \
    GGC_MPTR(typeK, key); \
    GGC_MPTR(typeV, value); \
GGC_END_TYPE(name ## Entry, \
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
static int name ## Get(name map, typeK key, typeV *value); \
static void name ## Put(name map, typeK key, typeV value)

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
    name ## Entry entry = NULL; \
    typeK keyCmp = NULL; \
    size_t hashV; \
    \
    GGC_PUSH_4(map, entry, key, keyCmp); \
    \
    if (GGC_RD(map, size) == 0) \
        return 0; \
    \
    hashV = hash(key) % GGC_RD(map, size); \
    entry = GGC_RAP(GGC_RP(map, entries), hashV); \
    while (entry) { \
        keyCmp = GGC_RP(entry, key); \
        if (keyCmp && cmp(key, keyCmp) == 0) { \
            *value = GGC_RP(entry, value); \
            return 1; \
        } \
        entry = GGC_RP(entry, next); \
    } \
    \
    return 0; \
} \
static void name ## Put(name map, typeK key, typeV value) \
{ \
    typeK keyCmp = NULL; \
    typeV valueCmp = NULL; \
    name ## Entry entry = NULL, nextEntry = NULL, prevEntry = NULL; \
    name ## EntryArray newEntries = NULL; \
    size_t hashV, hashV2; \
    ggc_size_t newSize, newUsed, i; \
    \
    GGC_PUSH_9(map, key, value, keyCmp, valueCmp, entry, nextEntry, prevEntry, \
        newEntries); \
    \
    if (GGC_RD(map, size) == 0) { \
        /* start with something */ \
        GGC_WD(map, size, 4); \
        newEntries = GGC_NEW_PA(name ## Entry, 4); \
        GGC_WP(map, entries, newEntries); \
    } \
    \
    newUsed = GGC_RD(map, used); \
    if (newUsed > GGC_RD(map, size) / 2) { \
        /* the hash is getting full. Expand it */ \
        newSize = GGC_RD(map, size) * 2; \
        \
        newEntries = GGC_NEW_PA(name ## Entry, newSize); \
        for (i = 0; i < GGC_RD(map, size); i++) { \
            entry = GGC_RAP(GGC_RP(map, entries), i); \
            while (entry) { \
                nextEntry = GGC_RP(entry, next); \
                keyCmp = GGC_RP(entry, key); \
                hashV = hash(keyCmp) % newSize; \
                prevEntry = GGC_RAP(newEntries, hashV); \
                GGC_WP(entry, next, prevEntry); \
                GGC_WAP(newEntries, hashV, entry); \
                entry = nextEntry; \
            } \
        } \
        \
        GGC_WD(map, size, newSize); \
        GGC_WP(map, entries, newEntries); \
    } \
    \
    /* figure out where to put it */ \
    hashV = hash(key) % GGC_RD(map, size); \
    \
    /* look over current entries */ \
    entry = GGC_RAP(GGC_RP(map, entries), hashV); \
    while (entry) { \
        /* entry found. Does it match? */ \
        keyCmp = GGC_RP(entry, key); \
        if (cmp(key, keyCmp) == 0) { \
            /* yes. Just update the value */ \
            GGC_WP(entry, value, value); \
            return; \
        } \
        entry = GGC_RP(entry, next); \
    } \
    \
    /* didn't find a current entry. Make a new one */ \
    newEntries = GGC_RP(map, entries); \
    entry = GGC_NEW(name ## Entry); \
    nextEntry = GGC_RAP(newEntries, hashV); \
    GGC_WP(entry, next, nextEntry); \
    GGC_WP(entry, key, key); \
    GGC_WP(entry, value, value); \
    GGC_WAP(newEntries, hashV, entry); \
    \
    /* and keep track of our use */ \
    newUsed++; \
    GGC_WD(map, used, newUsed); \
} \
static name name ## Clone(name map) \
{ \
    name ret = NULL; \
    name ## EntryArray oldEntries = NULL, newEntries = NULL; \
    name ## Entry oldEntry = NULL, oldNextEntry = NULL, newEntry = NULL, newNextEntry = NULL; \
    typeK key = NULL; \
    typeV value = NULL; \
    ggc_size_t size, used, i; \
    \
    GGC_PUSH_10(map, ret, oldEntries, newEntries, oldEntry, oldNextEntry, newEntry, newNextEntry, key, value); \
    \
    ret = GGC_NEW(name); \
    /* if it's empty, no further work */ \
    if (GGC_RD(map, size) == 0) return ret; \
    \
    /* copy the basic info */ \
    size = GGC_RD(map, size); \
    used = GGC_RD(map, used); \
    oldEntries = GGC_RP(map, entries); \
    GGC_WD(ret, size, size); \
    GGC_WD(ret, used, used); \
    \
    /* copy all the entries */ \
    newEntries = GGC_NEW_PA(name ## Entry, size); \
    for (i = 0; i < size; i++) { \
        oldEntry = GGC_RAP(oldEntries, i); \
        if (oldEntry) { \
            newEntry = GGC_NEW(name ## Entry); \
            GGC_WAP(newEntries, i, newEntry); \
            while (oldEntry) { \
                oldNextEntry = GGC_RP(oldEntry, next); \
                if (oldNextEntry) \
                    newNextEntry = GGC_NEW(name ## Entry); \
                else \
                    newNextEntry = NULL; \
                GGC_WP(newEntry, next, newNextEntry); \
                key = GGC_RP(oldEntry, key); \
                GGC_WP(newEntry, key, key); \
                value = GGC_RP(oldEntry, value); \
                GGC_WP(newEntry, value, value); \
                \
                oldEntry = oldNextEntry; \
                newEntry = newNextEntry; \
            } \
        } \
    } \
    GGC_WP(ret, entries, newEntries); \
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
