#pragma once

/*!
 * @file kas_alloc.h
 * Provides a special purpose memory allocator
 *
 * Functions and macroses with names like *_atomic are guaranteed not to call any syscall if their
 * arguments are valid.
 *
 * Functions and macroses with names like *_sycall are allowed to call syscalls, but don't have to
 * use them.
 *
 * Functions and macroses with usual names can be controlled by flags parameter.
 */

#include <stddef.h>

/*!
 * @brief Flag for atomic allocations
 *
 * This flag makes allocations atomic, without it they are non-atomic by default
 */
#define KAS_ALLOC_ATOMIC (1 << 0)

#ifndef KAS_TEST

void *kas_alloc(size_t size, int flags);
void *kas_calloc(size_t member_size, size_t member_nr, int flags);
void *kas_realloc(void *addr, size_t new_size, int flags);

void kas_free(void *mem);

#else
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

#endif

#define kas_alloc_atomic(S) kas_alloc((S), KAS_ALLOC_ATOMIC)
#define kas_alloc_syscall(S) kas_alloc((S), 0)

#define kas_calloc_atomic(MS, MN) kas_calloc((MS), (MN), KAS_ALLOC_ATOMIC)
#define kas_calloc_syscall(MS, MN) kas_calloc((MS), (MN), 0)

#define kas_realloc_atomic(A, S) kas_realloc((A), (S), KAS_ALLOC_ATOMIC)
#define kas_realloc_syscall(A, S) kas_realloc((A), (S), 0)
