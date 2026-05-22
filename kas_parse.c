#include "kas_defs_internal.h"
#include "kas_def_utils.h"
#include "kas_parse.h"
#include "kas_alloc.h"
#include <stdbool.h>
#include <string.h>

#define KALLSYMS_1BYTE_LEN_MAX 0x7f

struct kas_table {
    kas_long_t symbols_nr;
    struct kas_symbol *symbols;

    char *token_table[KALLSYMS_TOKEN_TABLE_SIZE];

    kaddr_t relative_base;
};

static bool kas_token_table_parse(struct kreader *kreader, struct kas_info *info, char **token_table) {
    kas_short_t offsets[KALLSYMS_TOKEN_TABLE_SIZE];

    if ( kreader_read(kreader, info->token_offsets_addr, sizeof(offsets), offsets) != sizeof(offsets) )
        return false;

    kaddr_t iter = info->token_table_addr; 

    int i = 0;

    for ( ; i < KALLSYMS_TOKEN_TABLE_SIZE; i++ ) {
        size_t token_size;

        if ( i == KALLSYMS_TOKEN_TABLE_SIZE - 1 ) {
            token_size = info->last_token_size;
        } else {
            token_size = offsets[i + 1] - offsets[i]; 
        }

        char *token = kas_alloc(token_size);
        if ( !token )
            goto free_tokens;

        if ( kreader_read(kreader, iter, token_size, token) != token_size ) {
            kas_free(token);
            goto free_tokens;
        }

        iter += token_size;

        token_table[i] = token;
    }

    return true;

free_tokens:
    for ( int j = 0; j < i; j++ ) {
        kas_free(token_table[j]);
    }

    return false;
}

static bool kas_expand_symbol(struct kas_table *kt, kas_byte_t *symbol, size_t sym_size, char *out, size_t out_size) {
    char *iter = out;
    size_t remain_size = out_size;

    for ( size_t i = 0; i < sym_size; i++ ) {
        size_t shift = snprintf(iter, remain_size, "%s", kt->token_table[symbol[i]]);
        if ( shift >= remain_size )
            return false;

        iter = offptr(iter, shift);
        remain_size -= shift;
    }

    return true;
}

static ssize_t kas_symbol_name_parse(struct kreader *kreader, kaddr_t addr, kas_byte_t *buff, size_t *sym_size_out) {
    kas_byte_t byte;
    size_t len;
    size_t len_len;

    kaddr_t iter = addr;

    if ( !kreader_read_var(kreader, iter, &byte) )
        return -1;

    iter++;

    if ( byte > KALLSYMS_1BYTE_LEN_MAX ) {
        kas_byte_t byte2;
        if ( !kreader_read_var(kreader, iter, &byte2) )
            return -1;

        len = (byte & 0x7f) + (byte2 << 7);
        
        iter++;
        len_len = 2;
    } else {
        len = byte;
        len_len = 1;
    }

    if ( kreader_read(kreader, addr + len_len, len, buff) != len )
        return -1;

    *sym_size_out = len;
    return len + len_len ;
}

static bool kas_symbols_name_parse(struct kreader *kreader, struct kas_table *kt, struct kas_info *info) {
    kas_byte_t raw_symbol_buff[KALLSYMS_SYM_MAX_LEN];
    char symbol_buff[KALLSYMS_SYM_MAX_LEN];

    kaddr_t iter = info->zipped_syms_addr;

    size_t i = 0;
    for ( ; i < kt->symbols_nr; i++ ) {
        size_t size;
        ssize_t full_size = kas_symbol_name_parse(kreader, iter, raw_symbol_buff, &size);
        if ( full_size == -1 )
            goto free_syms;

        iter += full_size;

        if ( !kas_expand_symbol(kt, raw_symbol_buff, size, symbol_buff, sizeof(symbol_buff)) )
            goto free_syms;

        size_t expand_len = strnlen(symbol_buff, KALLSYMS_SYM_MAX_LEN);
        if ( expand_len == KALLSYMS_SYM_MAX_LEN )
            goto free_syms;

        kt->symbols[i].name = kas_alloc(expand_len + 1);
        if ( !kt->symbols[i].name )
            goto free_syms;

        kt->symbols[i].type = symbol_buff[0];
        memcpy(kt->symbols[i].name, symbol_buff + 1, expand_len);
    }

    return true;
free_syms:
    for ( size_t j = 0; j < i; j++ ) {
        kas_free(kt->symbols[j].name);
    }
    return false;
}

static bool kas_symbols_offset_parse(struct kreader *kreader, struct kas_table *kt, struct kas_info *info) {
    kaddr_t iter = info->kallsyms_offsets_addr;
    for ( size_t i = 0; i < kt->symbols_nr; i++ ) {
        kas_long_t offset;
        if ( !kreader_read_var(kreader, iter, &offset) )
            return false;

        kt->symbols[i].offset = offset;

        iter += sizeof(offset);
    }

    return true;
}

struct kas_table *kas_table_parse(struct kreader *kreader, struct kas_info *info) {
    struct kas_table *kt = kas_alloc(sizeof(struct kas_table));
    if ( !kt )
        return NULL;

    if ( !kreader_read_var(kreader, info->relative_base_addr, &kt->relative_base) )
        goto kt_free;

    kt->symbols_nr = info->symbols_nr;
    kt->symbols = kas_calloc(sizeof(struct kas_symbol), kt->symbols_nr);
    if ( !kt->symbols )
        goto kt_free;

    if ( !kas_token_table_parse(kreader, info, kt->token_table) )
        goto symbols_free;

    if ( !kas_symbols_name_parse(kreader, kt, info) )
        goto token_table_free;

    if ( !kas_symbols_offset_parse(kreader, kt, info) )
        goto symbols_name_free;

    return kt;

symbols_name_free:
    for ( size_t i = 0; i < kt->symbols_nr; i++ ) {
        kas_free(kt->symbols[i].name);
    }
token_table_free:
    for ( int i = 0; i < KALLSYMS_TOKEN_TABLE_SIZE; i++ ) {
        kas_free(kt->token_table[i]);
    }
symbols_free:
    kas_free(kt->symbols);
kt_free:
    kas_free(kt);
    return NULL;
}

bool kas_table_get_symbol(struct kas_table *kt, const char *symbol, struct kas_symbol *out) {
    for ( size_t i = 0; i < kt->symbols_nr; i++ ) {
        if ( !strcmp(kt->symbols[i].name, symbol) ) {
            *out = kt->symbols[i];
            return true;
        }
    }

    return false;
}

kaddr_t kas_table_get_relative_base(struct kas_table *kt) {
    return kt->relative_base;
}

void kas_table_destroy(struct kas_table *kt) {
    for ( size_t i = 0; i < kt->symbols_nr; i++ ) {
        kas_free(kt->symbols[i].name);
    }
    kas_free(kt->symbols);
    kas_free(kt);
}
