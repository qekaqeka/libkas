#pragma once

#include <stdint.h>
#include <stdio.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

typedef int16_t kas_short_t;
typedef int32_t kas_long_t;
typedef uint32_t kas_ptr_t;
typedef uint8_t kas_byte_t;

typedef intptr_t kaddr_t;
#define kaddr_to_intptr(A) ((intptr_t)(A))

typedef ssize_t (*kreader_func_t)(void *private_arg, kaddr_t kaddr, size_t klen, void *ubuff);

struct kreader {
    kreader_func_t func;
    void *private_arg;
};

struct kas_info {
    size_t symbols_nr;

    kaddr_t kallsyms_addr;

    kaddr_t zipped_syms_addr;

    kaddr_t markers_addr;

    kaddr_t token_table_addr;
    kaddr_t token_offsets_addr;
    size_t last_token_size;

    kaddr_t kallsyms_offsets_addr;

    kas_ptr_t relative_base;

    kaddr_t seqs_of_names;
};
