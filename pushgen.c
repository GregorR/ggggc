/*
 * Generator for ggggc/push.h
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

#include <stdio.h>

int main()
{
    int i, j;
    printf("/* this file is generated by pushgen.c. Do not edit manually. */\n"
           "#ifndef GGGGC_PUSH_H\n#define GGGGC_PUSH_H 1\n");
    for (i = 1; i <= 16; i++) {
        /* first the fake case */
        printf("#ifdef GGGGC_DEBUG_NOPUSH\n"
               "#define GGC_PUSH_%d(", i);
        for (j = 0; j < i; j++) {
            if (j != 0) printf(", ");
            printf("ggggc_ptr_%c", 'a' + j);
        }
        printf(") 0\n"
               "#else\n");

        /* then the real case */
        printf("struct GGGGC_PointerStack%d {\n"
               "    struct GGGGC_PointerStack ps;\n"
               "    void *pointers[%d];\n"
               "};\n"
               "#define GGC_PUSH_%d(",
               i, i, i);
        for (j = 0; j < i; j++) {
            if (j != 0) printf(", ");
            printf("ggggc_ptr_%c", 'a' + j);
        }
        printf(") \\\n"
               "struct GGGGC_PointerStack%d ggggc_localPointerStack; \\\n"
               "GGGGC_LOCAL_PUSH \\\n"
               "do { \\\n"
               "    struct GGGGC_PointerStack *ggggc_pstack_cur = \\\n"
               "        &ggggc_localPointerStack.ps; \\\n"
               "    ggggc_pstack_cur->next = ggggc_pointerStack; \\\n"
               "    ggggc_pstack_cur->size = %d; \\\n",
               i, i);
        for (j = 0; j < i; j++) {
            printf("    ggggc_pstack_cur->pointers[%d] = &(ggggc_ptr_%c); \\\n", j, 'a' + j);
        }
        printf("    ggggc_pstack_cur->pointers[%d] = NULL; \\\n"
               "    ggggc_pointerStack = ggggc_pstack_cur; \\\n"
               "    GGC_YIELD(); \\\n"
               "} while(0)\n"
               "#endif\n", i);
    }
    printf("#define GGC_PUSH_N(n, pptrs) \\\n"
           "GGGGC_LOCAL_PUSH \\\n"
           "do { \\\n"
           "    ggc_size_t ggggc_n = (n); \\\n"
           "    struct GGGGC_PointerStack *ggggc_pstack_cur = \\\n"
           "        (struct GGGGC_PointerStack *) \\\n"
           "        alloca(sizeof(struct GGGGC_PointerStack) + sizeof(void *) * ggggc_n); \\\n"
           "    void *ggggc_pptrs[] = (void*[]) pptrs; \\\n"
           "    ggggc_pstack_cur->next = ggggc_pointerStack; \\\n"
           "    memcpy(ggggc_pstack_cur->pointers, ggggc_pptrs, sizeof(void *) * ggggc_n); \\\n"
           "    ggggc_pstack_cur->pointers[ggggc_n] = NULL; \\\n"
           "    ggggc_pointerStack = ggggc_pstack_cur; \\\n"
           "    GGC_YIELD(); \\\n"
           "} while(0)\n"
           "#endif\n");
    return 0;
}
