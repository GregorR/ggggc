#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc/gc.h"

/* globalize some local elements in the pointer stack */
void ggggc_globalize()
{
    struct GGGGC_PointerStack *gPointerStack;

    /* make a global copy */
    gPointerStack = malloc(sizeof(struct GGGGC_PointerStack) + ggggc_pointerStack->size * sizeof(void *));
    gPointerStack->next = NULL;
    gPointerStack->size = ggggc_pointerStack->size;
    memcpy(gPointerStack->pointers, ggggc_pointerStack->pointers, ggggc_pointerStack->size * sizeof(void *));

    /* then add it to the stack */
    if (!ggggc_pointerStackGlobals) {
        /* need to find the globals first! */
        struct GGGGC_PointerStack *cur = ggggc_pointerStack;
        while (cur->next) cur = cur->next;
        ggggc_pointerStackGlobals = cur;
    }
    ggggc_pointerStackGlobals->next = gPointerStack;
    ggggc_pointerStackGlobals = gPointerStack;
}
