/*
 * Generic implementation of map collections for GGGGC
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

#include "ggggc/gc.h"
#include "ggggc/collections/map.h"

/* get an element out of a map */
int GGC_MapGet(GGC_Map map, void *key, void **value, ggc_map_hash_t hash, ggc_map_cmp_t cmp)
{
    GGC_MapEntry entry = NULL;
    void *keyCmp = NULL;
    size_t hashV;

    GGC_PUSH_4(map, entry, key, keyCmp);

    if (GGC_RD(map, size) == 0)
        return 0;

    hashV = hash(key) % GGC_RD(map, size);
    entry = GGC_RAP(GGC_RP(map, entries), hashV);
    while (entry) {
        keyCmp = GGC_RP(entry, key);
        if (keyCmp && cmp(key, keyCmp) == 0) {
            *value = GGC_RP(entry, value);
            return 1;
        }
        entry = GGC_RP(entry, next);
    }

    return 0;
}

/* put an element in a map */
void GGC_MapPut(GGC_Map map, void *key, void *value, ggc_map_hash_t hash, ggc_map_cmp_t cmp)
{
    void *keyCmp = NULL;
    void *valueCmp = NULL;
    GGC_MapEntry entry = NULL, nextEntry = NULL, prevEntry = NULL;
    GGC_MapEntryArray newEntries = NULL;
    size_t hashV;
    ggc_size_t newSize, newUsed, i;

    GGC_PUSH_9(map, key, value, keyCmp, valueCmp, entry, nextEntry, prevEntry,
        newEntries);

    if (GGC_RD(map, size) == 0) {
        /* start with something */
        GGC_WD(map, size, 4);
        newEntries = GGC_NEW_PA(GGC_MapEntry, 4);
        GGC_WP(map, entries, newEntries);
    }

    newUsed = GGC_RD(map, used);
    if (newUsed > GGC_RD(map, size) / 2) {
        /* the hash is getting full. Expand it */
        newSize = GGC_RD(map, size) * 2;

        newEntries = GGC_NEW_PA(GGC_MapEntry, newSize);
        for (i = 0; i < GGC_RD(map, size); i++) {
            entry = GGC_RAP(GGC_RP(map, entries), i);
            while (entry) {
                nextEntry = GGC_RP(entry, next);
                keyCmp = GGC_RP(entry, key);
                hashV = hash(keyCmp) % newSize;
                prevEntry = GGC_RAP(newEntries, hashV);
                GGC_WP(entry, next, prevEntry);
                GGC_WAP(newEntries, hashV, entry);
                entry = nextEntry;
            }
        }

        GGC_WD(map, size, newSize);
        GGC_WP(map, entries, newEntries);
    }

    /* figure out where to put it */
    hashV = hash(key) % GGC_RD(map, size);

    /* look over current entries */
    entry = GGC_RAP(GGC_RP(map, entries), hashV);
    while (entry) {
        /* entry found. Does it match? */
        keyCmp = GGC_RP(entry, key);
        if (cmp(key, keyCmp) == 0) {
            /* yes. Just update the value */
            GGC_WP(entry, value, value);
            return;
        }
        entry = GGC_RP(entry, next);
    }

    /* didn't find a current entry. Make a new one */
    newEntries = GGC_RP(map, entries);
    entry = GGC_NEW(GGC_MapEntry);
    nextEntry = GGC_RAP(newEntries, hashV);
    GGC_WP(entry, next, nextEntry);
    GGC_WP(entry, key, key);
    GGC_WP(entry, value, value);
    GGC_WAP(newEntries, hashV, entry);

    /* and keep track of our use */
    newUsed++;
    GGC_WD(map, used, newUsed);

    return;
}

/* clone a map */
GGC_Map GGC_MapClone(GGC_Map map)
{
    GGC_Map ret = NULL;
    GGC_MapEntryArray oldEntries = NULL, newEntries = NULL;
    GGC_MapEntry oldEntry = NULL, oldNextEntry = NULL, newEntry = NULL, newNextEntry = NULL;
    void *key = NULL;
    void *value = NULL;
    ggc_size_t size, used, i;

    GGC_PUSH_10(map, ret, oldEntries, newEntries, oldEntry, oldNextEntry, newEntry, newNextEntry, key, value);

    ret = GGC_NEW(GGC_Map);
    /* if it's empty, no further work */
    if (GGC_RD(map, size) == 0) return ret;

    /* copy the basic info */
    size = GGC_RD(map, size);
    used = GGC_RD(map, used);
    oldEntries = GGC_RP(map, entries);
    GGC_WD(ret, size, size);
    GGC_WD(ret, used, used);

    /* copy all the entries */
    newEntries = GGC_NEW_PA(GGC_MapEntry, size);
    for (i = 0; i < size; i++) {
        oldEntry = GGC_RAP(oldEntries, i);
        if (oldEntry) {
            newEntry = GGC_NEW(GGC_MapEntry);
            GGC_WAP(newEntries, i, newEntry);
            while (oldEntry) {
                oldNextEntry = GGC_RP(oldEntry, next);
                if (oldNextEntry)
                    newNextEntry = GGC_NEW(GGC_MapEntry);
                else
                    newNextEntry = NULL;
                GGC_WP(newEntry, next, newNextEntry);
                key = GGC_RP(oldEntry, key);
                GGC_WP(newEntry, key, key);
                value = GGC_RP(oldEntry, value);
                GGC_WP(newEntry, value, value);

                oldEntry = oldNextEntry;
                newEntry = newNextEntry;
            }
        }
    }
    GGC_WP(ret, entries, newEntries);

    return ret;
}
