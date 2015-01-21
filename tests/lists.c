#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc/collections/list.h"

GGC_TYPE(Thing)
    GGC_MDATA(int, a);
    GGC_MDATA(int, b);
GGC_END_TYPE(Thing, GGC_NO_PTRS);

GGC_LIST(Thing)

int main()
{
    ThingList list = NULL;
    ThingArray array = NULL;
    Thing thing = NULL;
    int i;

    GGC_PUSH_3(list, array, thing);

    list = GGC_NEW(ThingList);
    for (i = 0; i < 100; i++) {
        thing = GGC_NEW(Thing);
        GGC_WD(thing, a, i);
        i++;
        GGC_WD(thing, b, i);
        i--;
        ThingListPush(list, thing);
    }

    array = ThingListToArray(list);
    for (i = 0; i < array->length; i++) {
        thing = GGC_RAP(array, i);
        printf("%d %d\n",
            GGC_RD(thing, a), GGC_RD(thing, b));
    }

    return 0;
}
