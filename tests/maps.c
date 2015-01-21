#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc/collections/map.h"

GGC_TYPE(Key)
    GGC_MDATA(size_t, key);
GGC_END_TYPE(Key, GGC_NO_PTRS);

GGC_TYPE(Value)
    GGC_MDATA(int, value);
GGC_END_TYPE(Value, GGC_NO_PTRS);

static size_t hash(Key key)
{
    return GGC_RD(key, key);
}

static int cmp(Key a, Key b)
{
    size_t l, r;
    l = GGC_RD(a, key);
    r = GGC_RD(b, key);
    if (l == r) return 0;
    else if (l < r) return -1;
    else return 1;
}

GGC_MAP(ExMap, Key, Value, hash, cmp)

int main()
{
    Key key = NULL;
    Value val = NULL;
    ExMap map = NULL;
    int i;

    GGC_PUSH_3(key, val, map);

    map = GGC_NEW(ExMap);
    for (i = 0; i < 1000; i++) {
        key = GGC_NEW(Key);
        GGC_WD(key, key, i);
        val = GGC_NEW(Value);
        GGC_WD(val, value, i);
        ExMapPut(map, key, val);
    }

    for (i = 0; i < 1000; i++) {
        key = GGC_NEW(Key);
        GGC_WD(key, key, i);
        printf("%d: ", i);

        if (ExMapGet(map, key, &val))
            printf("%d\n", GGC_RD(val, value));
        else
            printf("!!!\n");
    }

    return 0;
}
