#ifndef GGGGC_INTERNAL
#define GGGGC_INTERNAL

void GGGGC_collector_init();
void *GGGGC_trymalloc_gen(unsigned char gen, size_t sz, unsigned char ptrs);

#endif
