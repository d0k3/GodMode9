#include "mymalloc.h"
#include <stdlib.h>

static size_t total_allocated = 0;

void* my_malloc(size_t size) {
    if (!size) return NULL; // nothing, return nothing
    void* ptr = (void*) malloc(sizeof(size_t) + size);
    if (ptr) total_allocated += size;
    if (ptr) (*(size_t*) ptr) = size;
    return ptr ? (((char*) ptr) + sizeof(size_t)) : NULL;
}

void *my_realloc(void *ptr, size_t new_size) {
    if (!ptr)
        return my_malloc(new_size);

    if (!new_size) {
        my_free(ptr);
        return NULL;
    }

    void *real_ptr = (char*)ptr - sizeof(size_t);
    size_t old_size = *(size_t*)real_ptr;

    void *new_ptr = realloc(real_ptr, new_size + sizeof(size_t));
    if (new_ptr) {
        total_allocated -= old_size;
        total_allocated += new_size;

        *(size_t*)new_ptr = new_size;
        return (char*)new_ptr + sizeof(size_t);
    }

    return new_ptr;
}

void my_free(void* ptr) {
    if (!ptr) return; // just like real free, dont do anything here
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
