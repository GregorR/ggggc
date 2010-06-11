#include <stdio.h>

#include "ggggc.h"

GGC_DATA_STRUCT(Foo,
    int d;
);

int main()
{
    int i;

    GGC_INIT();

    return 0;
}
