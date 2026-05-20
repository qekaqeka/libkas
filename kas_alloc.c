#include "kas_alloc.h"
#include "chk_op.h"
#include "bitmask.h"
#include "listc.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "kas_def_utils.h"

struct kas_block;

static struct kas_block *kas_freelist;

struct kas_free_block_meta {
    struct listc list;
};

#define KAS_BLOCK_FREE (1 << 0)
#define KAS_BLOCK_END (1 << 1)

struct kas_block_common {
    size_t bl_size;
    struct kas_block *bl_back;
    int bl_flags;
};

struct kas_block {
    struct kas_block_common common;

    struct kas_free_block_meta free;
};

#define KAS_BLOCK_SIZE_SPLIT_THRESHOLD sizeof(struct kas_block_common)
#define KAS_MORECORE_ATOM 4096
#define KAS_BLOCK_DATA_OFF offsetof(struct kas_block, free)
#define KAS_BLOCK_DATA_SIZE_MAX (SIZE_MAX - KAS_BLOCK_DATA_OFF)
#define KAS_BLOCK_DATA_SIZE_MIN (sizeof(struct kas_block) - KAS_BLOCK_DATA_OFF)

#define kas_block_flags_test(B, F) bitmask_test((B)->common.bl_flags, (F))
#define kas_block_flags_set(B, F) bitmask_set(&(B)->common.bl_flags, (F))
#define kas_block_flags_clear(B, F) bitmask_clear(&(B)->common.bl_flags, (F))

#define kas_freelist_init_block(B) listc_init((B), free.list)
#define kas_freelist_get_prev(B) listc_get_prev((B), free.list)
#define kas_freelist_get_next(B) listc_get_next((B), free.list)
#define kas_freelist_add_block(B) do{ assert(kas_block_is_free(B)); listc_add_item_front(&kas_freelist, (B), free.list); } while (0)
#define kas_freelist_remove_block(B) listc_remove_item(&kas_freelist, (B), free.list)
#define KAS_FREELIST_FOREACH_START(ITER_NAME) LISTC_FOREACH_START(kas_freelist, free.list, ITER_NAME)
#define KAS_FREELIST_FOREACH_END(ITER_NAME) LISTC_FOREACH_END(kas_freelist, free.list, ITER_NAME)
#define KAS_FREELIST_FOREACH_START_SAFE(ITER_NAME) LISTC_FOREACH_START(kas_freelist, free.list, ITER_NAME)
#define KAS_FREELIST_FOREACH_END_SAFE(ITER_NAME) LISTC_FOREACH_END(kas_freelist, free.list, ITER_NAME)

static int kas_block_is_free(struct kas_block *bl) {
    assert(bl);

    return kas_block_flags_test(bl, KAS_BLOCK_FREE);
}

static void *kas_block_get_data(struct kas_block *bl) {
    assert(!kas_block_is_free(bl));

    return offptr(bl, KAS_BLOCK_DATA_OFF);
}

static struct kas_block *kas_block_from_data(void *data) {
    return offptr(data, -KAS_BLOCK_DATA_OFF);
}

static size_t kas_block_full_size(size_t data_size) {
    size_t busy_size;
    assert(!chk_add(KAS_BLOCK_DATA_OFF, data_size, &busy_size));
    assert(data_size >= KAS_BLOCK_DATA_SIZE_MIN);
    return busy_size;
}

static struct kas_block *kas_block_get_back(struct kas_block *bl) {
    return bl->common.bl_back;
}

static void kas_block_set_back(struct kas_block *bl, struct kas_block *back) {
    if ( back )
        assert(bl == offptr(back, kas_block_full_size(back->common.bl_size)));

    bl->common.bl_back = back;
}

static struct kas_block *kas_block_get_following(struct kas_block *bl) {
    assert(bl);

    if ( kas_block_flags_test(bl, KAS_BLOCK_END) )
        return NULL;

    return offptr(bl, kas_block_full_size(bl->common.bl_size));
}

static struct kas_block *kas_new_free_block(void *mem, size_t mem_size) {
    assert(mem);

    if ( mem_size < sizeof(struct kas_block))
        return NULL;

    struct kas_block *bl = mem;
    bl->common.bl_size = mem_size - KAS_BLOCK_DATA_OFF;
    kas_block_flags_set(bl, KAS_BLOCK_FREE);
    kas_block_flags_set(bl, KAS_BLOCK_END);
    bl->common.bl_back = NULL;

    kas_freelist_init_block(bl);
    kas_freelist_add_block(bl);

    return bl;
}

static struct kas_block *kas_block_split(struct kas_block *bl, size_t new_size) {
    assert(kas_block_is_free(bl));
    assert(new_size >= KAS_BLOCK_DATA_SIZE_MIN);
    assert(bl->common.bl_size >= kas_block_full_size(new_size));
    assert(bl->common.bl_size - kas_block_full_size(new_size) >= KAS_BLOCK_DATA_SIZE_MIN);

    struct kas_block *following = kas_block_get_following(bl);

    bl->common.bl_size -= kas_block_full_size(new_size);
    struct kas_block *new_bl = offptr(bl, kas_block_full_size(bl->common.bl_size));
    new_bl->common.bl_size = new_size;
    new_bl->common.bl_flags = 0;

    if ( following ) {
        kas_block_set_back(following, new_bl);
    } else {
        assert(kas_block_flags_test(bl, KAS_BLOCK_END));
        kas_block_flags_clear(bl, KAS_BLOCK_END);
        kas_block_flags_set(new_bl, KAS_BLOCK_END);
    }

    kas_block_set_back(new_bl, bl);
    
    return new_bl;
}

static struct kas_block *kas_block_merge(struct kas_block *bl1, struct kas_block *bl2) {
    assert(bl1 && bl2);
    assert(kas_block_is_free(bl2));

    kas_freelist_remove_block(bl2);
    size_t merged_size;
    if ( chk_add(bl1->common.bl_size, kas_block_full_size(bl2->common.bl_size), &merged_size)
            || merged_size > KAS_BLOCK_DATA_SIZE_MAX ) {

        assert(0); // I don't want to handle this edge case
        return NULL;
    }

    bl1->common.bl_size += kas_block_full_size(bl2->common.bl_size);

    struct kas_block *following = kas_block_get_following(bl2);
    if ( following ) {
        kas_block_set_back(following, bl1);
    } else {
        assert(kas_block_flags_test(bl2, KAS_BLOCK_END));
        kas_block_flags_set(bl1, KAS_BLOCK_END);
    }

    return bl1;
}

#ifndef KAS_ALLOC_MEM_POOL_SIZE
#define KAS_ALLOC_MEM_POOL_SIZE (10 * 1024 * 1024)
#endif

[[gnu::constructor]]
static void kas_alloc_init(void) {
    static char kas_alloc_mem_pool[KAS_ALLOC_MEM_POOL_SIZE];

    assert(kas_new_free_block(kas_alloc_mem_pool, sizeof(kas_alloc_mem_pool)));
}


static int kas_block_size_splitable(size_t size, size_t target_size) {
    return size > (kas_block_full_size(target_size) + KAS_BLOCK_SIZE_SPLIT_THRESHOLD);
}

static struct kas_block *kas_use_block(struct kas_block *bl, size_t req_size) {
    assert(kas_block_is_free(bl));

    if ( bl->common.bl_size < req_size )
        return NULL;

    if ( kas_block_size_splitable(bl->common.bl_size, req_size) ) {
        return kas_block_split(bl, req_size);
    } else {
        kas_block_flags_clear(bl, KAS_BLOCK_FREE);
        kas_freelist_remove_block(bl);

        return bl;
    }
}

void *kas_alloc(size_t size, int flags) {
    if ( size == 0 )
        return NULL;

    size = max(size, KAS_BLOCK_DATA_SIZE_MIN);

    KAS_FREELIST_FOREACH_START(iter)
        struct kas_block *res = kas_use_block(iter, size);

        if ( res )
            return kas_block_get_data(res);
    KAS_FREELIST_FOREACH_END(iter);

    if ( !bitmask_test(flags, KAS_ALLOC_ATOMIC) ) {
        size_t newmem_sz = align_div_ceil(size, KAS_MORECORE_ATOM);
        void *newmem = malloc(newmem_sz);
        if ( newmem ) {
            struct kas_block *new_free_bl = kas_new_free_block(newmem, newmem_sz);
            assert(new_free_bl);

            struct kas_block *res = kas_use_block(new_free_bl, size);
            assert(res);

            return kas_block_get_data(res);
        }
    }

    return NULL;
}

void *kas_calloc(size_t member_size, size_t member_nr, int flags) {
    size_t total_size;
    if ( chk_mul(member_size, member_nr, &total_size) )
        return NULL;

    return kas_alloc(total_size, flags);
}

void *kas_realloc(void *mem, size_t size, int flags) {
    struct kas_block *bl = kas_block_from_data(mem);
    assert(!kas_block_is_free(bl));

    if ( bl->common.bl_size >= size) {
        return kas_block_get_data(bl);
    }

    struct kas_block *following = kas_block_get_following(bl);
    if ( following && kas_block_is_free(following) ) {
        if ( bl->common.bl_size + kas_block_full_size(following->common.bl_size) >= size ) {
            struct kas_block *new_bl = kas_block_merge(bl, following);
            return kas_block_get_data(new_bl);
        }
    }

    void *newmem = kas_alloc(size, flags);
    if ( !newmem )
        return NULL;

    memcpy(newmem, mem, bl->common.bl_size);

    kas_free(mem);

    return newmem;
}

void kas_free(void *mem) {
    if ( !mem )
        return;

    struct kas_block *bl = kas_block_from_data(mem);
    assert(!kas_block_is_free(bl));

    kas_freelist_init_block(bl);
    kas_block_flags_set(bl, KAS_BLOCK_FREE);

    struct kas_block *back = kas_block_get_back(bl);
    struct kas_block *following = kas_block_get_following(bl);

    if ( back && kas_block_is_free(back) ) {
        bl = kas_block_merge(back, bl);
        assert(bl);
    }

    if ( following && kas_block_is_free(following) ) {
        bl = kas_block_merge(bl, following);
        assert(bl);
    }
}
