#include "mymalloc.h"
#include <stdlib.h>

static size_t total_allocated = 0;

void* my_malloc(size_t size) {
    void* ptr = (void*) malloc(sizeof(size_t) + size);
    if (ptr) total_allocated += size;
    if (ptr) (*(size_t*) ptr) = size;
    return ptr ? (((char*) ptr) + sizeof(size_t)) : NULL;
}

void my_free(void* ptr) {
    void* ptr_fix = (char*) ptr - sizeof(size_t);
    total_allocated -= *(size_t*) ptr_fix;
    free(ptr_fix);
}

size_t mem_allocated(void) {
    return total_allocated;
}

size_t my_malloc_test(void) {
    size_t add = 1024 * 1024;
    for (size_t s = add;; s += add) {
        void* ptr = (void*) malloc(s);
        if (!ptr) return s;
        free(ptr);
    }
    return 0; // unreachable
}
