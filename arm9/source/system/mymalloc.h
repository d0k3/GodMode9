#pragma once

#include <stddef.h>

void* my_malloc(size_t size);
void my_free(void* ptr);
size_t mem_allocated(void);
size_t my_malloc_test(void);
