#pragma once

#include <stddef.h>
#include <stdlib.h>


static inline void *kas_alloc(size_t size, int flags) {
    (void)flags;
    return malloc(size);
}

static inline void *kas_calloc(size_t member_size, size_t member_nr, int flags) {
    (void)flags;
    return calloc(member_size, member_nr);
}

static inline void *kas_realloc(void *addr, size_t new_size, int flags) {
    (void)flags;
    return realloc(addr, new_size);
}

static inline void kas_free(void *mem) {
    free(mem);
}
