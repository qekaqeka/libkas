#include "kas_search.h"
#include "chk_op.h"
#include "kas_defs_internal.h"
#include <assert.h>
#include "kas_def_utils.h"
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static bool kas_kmem_scan(struct kreader *kreader, kaddr_t bottom, kaddr_t top, void *ubuf, size_t len, const void *target, size_t step, kaddr_t *out) {
    assert(len > 0);
    assert(top >= bottom);

    while ( true ) {
        size_t diff;
        if ( chk_sub(top, bottom, &diff) || diff < len )
            return false;

        if ( kreader_read(kreader, bottom, len, ubuf) != len )
            return false;

        if ( !memcmp(ubuf, target, len) ) {
            *out = bottom;
            return true;
        }

        if ( chk_add(bottom, step, &bottom) )
            return false;
    }

}

static bool kas_get_align_pad_size(struct kreader *kreader, kaddr_t addr, size_t *pad_size_out) {
    assert(is_aligned(addr, KALLSYMS_LABELS_ALIGN));
    kas_byte_t byte;

    for ( size_t pad_size = 0; pad_size < KALLSYMS_LABELS_ALIGN; pad_size++ ) {
        if ( !kreader_read_var(kreader, addr - 1 - pad_size, &byte) )
            return false;

        if ( byte != KAS_ALIGN_FILLER ) {
            *pad_size_out = pad_size;
            return true;
        }
    }
    
    *pad_size_out = KALLSYMS_LABELS_ALIGN;
    return true;
}

static bool kas_token_table_last_size(struct kreader *kreader, kaddr_t start_offsets, size_t *out_full_size, size_t *out_size) {
    char c;
    bool end_found = false;

    size_t pad_size;
    if ( !kas_get_align_pad_size(kreader, start_offsets, &pad_size) )
        return false;

    // Null terminator could be counted
    if ( KAS_ALIGN_FILLER == '\0' )
        pad_size--;

    kaddr_t token_end = start_offsets - pad_size - 1;

    for ( kaddr_t iter = token_end - 1; token_end - iter < KALLSYMS_SYM_MAX_LEN; iter-- ) {
        if ( !kreader_read_var(kreader, iter, &c) )
            return false;

        if ( c == '\0' ) {
            *out_full_size = start_offsets - iter;
            *out_size = token_end - iter;
            return true;
        }
    }

    return false;
}

static bool kas_token_table_offs_verify(struct kreader *kreader, kaddr_t start_offsets, kas_short_t *token_table_offs_out) {
    kaddr_t iter = start_offsets;
    
    if ( !kreader_read_var(kreader, iter, &token_table_offs_out[0]) )
        return false;

    iter += sizeof_el(token_table_offs_out);

    // Token table offsets start with 0
    if ( token_table_offs_out[0] != 0 )
        return false;

    for ( int i = 1; i < KALLSYMS_TOKEN_TABLE_SIZE; i++ ) {
        if ( !kreader_read_var(kreader, iter, &token_table_offs_out[i]) )
            return false;

        if ( token_table_offs_out[i - 1] >= token_table_offs_out[i] )
            return false;

        iter += sizeof_el(token_table_offs_out);
    }

    return true;
}

static bool kas_token_verify(char *token, size_t size, size_t index) {
    if ( token[size - 1] != '\0' )
        return false;

    if ( size == 2 )
        return token[0] == index;

    for ( size_t i = 0; i < size - 1; i++ ) {
        if ( !isprint(token[i]) )
            return false;
    }

    return true;
}

static bool kas_token_table_tokens_verify(struct kreader *kreader, kaddr_t token_table_start, kas_short_t *token_offs, size_t last_token_size) {
    char token_buff[KALLSYMS_SYM_MAX_LEN];

    kaddr_t iter = token_table_start;

    for ( int i = 0; i < KALLSYMS_TOKEN_TABLE_SIZE; i++ ) {
        size_t token_size;

        if ( i == KALLSYMS_TOKEN_TABLE_SIZE - 1) {
            token_size = last_token_size;
        } else {
            token_size = token_offs[i + 1] - token_offs[i];
        }

        if ( kreader_read(kreader, iter, token_size, token_buff) != token_size )
            return false;

        if ( !kas_token_verify(token_buff, token_size, i) )
            return false;

        iter += token_size;
    }

    return true;
}

static bool kas_token_table_check(struct kreader *kreader, kaddr_t start_offsets, struct kas_info *out) {
    kas_short_t token_table_offs[KALLSYMS_TOKEN_TABLE_SIZE];

    if ( !kas_token_table_offs_verify(kreader, start_offsets, token_table_offs) )
        return false;

    // last token size with aligment zeroes
    size_t last_token_full_size;
    size_t last_token_size;
    if ( !kas_token_table_last_size(kreader, start_offsets, &last_token_full_size, &last_token_size) )
        return false;

    size_t token_offset_table_size = last_token_full_size + token_table_offs[KALLSYMS_TOKEN_TABLE_SIZE - 1] - 1;
    kaddr_t token_table_start = start_offsets - token_offset_table_size;

    if ( !kas_token_table_tokens_verify(kreader, token_table_start, token_table_offs, last_token_size) )
        return false;

    out->token_table_addr = token_table_start;
    out->last_token_size = last_token_size;
    return true;
}

static bool kas_token_table_find(struct kreader *kreader, kaddr_t bottom, kaddr_t top, struct kas_info *out) {
    assert(top > bottom);
    kaddr_t token_table_offs_start;

    kas_short_t token;
    const kas_short_t target = 0;

    bottom = align_div_floor(bottom, KALLSYMS_LABELS_ALIGN);

    while ( true ) {
        if ( !kas_kmem_scan(kreader, bottom, top, &token, sizeof(token),
                   &target, KALLSYMS_LABELS_ALIGN, &token_table_offs_start) )
            return false;

        if ( !is_aligned(token_table_offs_start, KALLSYMS_LABELS_ALIGN) ) {
            bottom = align_div_ceil(token_table_offs_start + 1, KALLSYMS_LABELS_ALIGN);

            // Corner case
            if ( bottom > top )
                return false;

            continue;
        }

        if ( kas_token_table_check(kreader, token_table_offs_start, out)
                && is_aligned(out->token_table_addr, KALLSYMS_LABELS_ALIGN) ) {

            out->token_offsets_addr = token_table_offs_start;
            return true;
        } else {
            bottom = align_div_ceil(token_table_offs_start + 1, KALLSYMS_LABELS_ALIGN);

            // Corner case
            if ( bottom > top )
                return false;
        }
    }
}

static bool kas_markers_check(struct kreader *kreader, kaddr_t markers_top, kaddr_t *markers_start_out, kas_long_t *last_marker_out, kas_long_t *markers_nr_out) {
    kas_long_t markers_nr = 0;

    kas_long_t marker_prev;
    kas_long_t marker;

    size_t pad_size;
    if ( !kas_get_align_pad_size(kreader, markers_top, &pad_size) )
        return false;

    pad_size = align_div_floor(pad_size, sizeof(kas_long_t));

    kaddr_t iter = markers_top - pad_size - sizeof(marker);

    if ( !kreader_read_var(kreader, iter, &marker_prev) )
        return false;

    *last_marker_out = marker_prev;

    iter -= sizeof(marker);

    markers_nr = 1;

    while ( true ) {
        if ( !kreader_read_var(kreader, iter, &marker) )
            return false;

        markers_nr++;

        if ( marker >= marker_prev )
            return false;

        if ( marker == 0 ) {
            *markers_nr_out = markers_nr;
            *markers_start_out = iter;
            return true;
        }

        iter -= sizeof(marker);
        marker_prev = marker;
    }
}

static bool kas_search_kallsyms_start(struct kreader *kreader, kaddr_t bottom, kaddr_t top, kas_long_t markers_nr, struct kas_info *out) {
    assert(is_aligned(top, KALLSYMS_LABELS_ALIGN));

    const kas_long_t target = (markers_nr - 1) * 256;

    kaddr_t iter = top - KALLSYMS_LABELS_ALIGN;

    kas_long_t var;

    while ( iter >= bottom ) {
        if ( !kreader_read_var(kreader, iter, &var) )
            return false;

        kas_long_t tmp = align_div_floor(var, 256);

        if ( tmp == target ) {
            out->symbols_nr = var;
            out->zipped_syms_addr = align_div_ceil(iter + sizeof(kas_long_t), KALLSYMS_LABELS_ALIGN);
            out->kallsyms_addr = iter;
            return true;
        }

        iter -= KALLSYMS_LABELS_ALIGN;
    }

    return false;
}

static bool kas_info_calc_addrs(struct kreader *kreader, struct kas_info *info) {
    kaddr_t token_offsets_top = info->token_offsets_addr + KALLSYMS_TOKEN_TABLE_SIZE * sizeof(kas_short_t);
    info->kallsyms_offsets_addr = align_div_ceil(token_offsets_top, KALLSYMS_LABELS_ALIGN);

    kaddr_t kallsyms_offsets_top = info->kallsyms_offsets_addr + info->symbols_nr * sizeof(kas_long_t);
    info->relative_base_addr = align_div_ceil(kallsyms_offsets_top, KALLSYMS_LABELS_ALIGN);

    kaddr_t kallsyms_seqs_of_names_addr = align_div_ceil(info->relative_base_addr + sizeof(kas_ptr_t), KALLSYMS_LABELS_ALIGN);
    info->seqs_of_names = kallsyms_seqs_of_names_addr;

    return true;
}

bool kas_search(struct kreader *kreader, kaddr_t kaddr_bottom, kaddr_t kaddr_top, struct kas_info *info) {
    kaddr_t iter = kaddr_bottom;

    while ( iter <= kaddr_top ) {
        // There is nothing to do if we didn't find any token tables
        if ( !kas_token_table_find(kreader, iter, kaddr_top, info) )
            return false;

        kas_long_t markers_nr;
        kas_long_t last_marker;
        kaddr_t markers_start;
        if ( !kas_markers_check(kreader, info->token_table_addr, 
                    &markers_start, &last_marker, &markers_nr) ) {

            iter = align_div_ceil(iter + 1, KALLSYMS_LABELS_ALIGN);
            continue;
        }
        info->markers_addr = markers_start;

        if ( !is_aligned(markers_start, KALLSYMS_LABELS_ALIGN) ) {
            iter = align_div_ceil(iter + 1, KALLSYMS_LABELS_ALIGN);
            continue;
        }

        kaddr_t top = align_div_ceil(markers_start - last_marker, KALLSYMS_LABELS_ALIGN);

        kaddr_t kallsyms;
        if ( !kas_search_kallsyms_start(kreader, kaddr_bottom, top, markers_nr, info) ) {
            iter = align_div_ceil(iter + 1, KALLSYMS_LABELS_ALIGN);
            continue;
        } else {
            if ( !kas_info_calc_addrs(kreader, info) ) {
                iter = align_div_ceil(iter + 1, KALLSYMS_LABELS_ALIGN);
                continue;
            }

            return true;
        }
    }

    return false;
}
