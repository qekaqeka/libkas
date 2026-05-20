#include "kas_kreader_wrap.h"
#include "kas_alloc.h"
#include "kas_def_utils.h"
#include <string.h>
#include <assert.h>

#define KAS_KREADER_BUFFER_CAPACITY 4096

struct kas_kreader_buffered_arg {
    struct kreader base_kreader;

    bool external_buff;
    void *buff;

    bool is_buffered;
    kaddr_t buffered_addr;

    // Actual buffed bytes length
    size_t len;
    // The size of buffer
    size_t capacity;
};

static bool kas_check_buffer_contains(kaddr_t buffered_addr, size_t buffered_len, kaddr_t addr, size_t len) {
    return buffered_addr <= addr && buffered_addr + buffered_len >= addr + len;
}

static ssize_t kas_kreader_buffered_func(void *arg_, kaddr_t addr, size_t len, void *ubuff) {
    assert(arg_ && ubuff);

    struct kas_kreader_buffered_arg *arg = arg_;

    kaddr_t aligned_addr = align_div_floor(addr, arg->capacity);

    size_t bytes_to_read = addr + len - aligned_addr;

    // Buffering is useless in this case
    if ( bytes_to_read >= arg->capacity ) {
        return kreader_read(&arg->base_kreader, addr, len, ubuff);
    }


    if ( arg->is_buffered && kas_check_buffer_contains(arg->buffered_addr, arg->len, addr, len) ) {
        ptrdiff_t buff_off = addr - arg->buffered_addr;
        memcpy(ubuff, offptr(arg->buff, buff_off), len);

        return len;
    } else {
        ssize_t bytes_read = kreader_read(&arg->base_kreader, aligned_addr, arg->capacity, arg->buff);

        // Buffer is unchanged so we can keep things the same. Even if it was buffered, we can keep
        // it buffered on the old address
        if ( bytes_to_read <= 0 )
            return -1;

        if ( kas_check_buffer_contains(aligned_addr, bytes_read, addr, len) ) {
            arg->is_buffered = true;
            arg->buffered_addr = aligned_addr;
            arg->len = bytes_to_read;

            ptrdiff_t buff_off = addr - arg->buffered_addr;
            memcpy(ubuff, offptr(arg->buff, buff_off), len);

            return len;
        } else {
            arg->is_buffered = false;

            return -1;
        }
    }
}

bool kas_kreader_buffered_wrap(struct kreader *kreader, void *buff, size_t capacity) {
    assert(kreader);

    struct kas_kreader_buffered_arg *arg = kas_alloc_atomic(sizeof(struct kas_kreader_buffered_arg));
    if ( !arg )
        goto error;

    bool external_buff = buff != NULL;

    if ( !external_buff ) {
        buff = kas_alloc_atomic(KAS_KREADER_BUFFER_CAPACITY);
        if ( !buff ) 
            goto free_arg;

        capacity = KAS_KREADER_BUFFER_CAPACITY;
    }

    arg->base_kreader = *kreader;
    arg->external_buff = external_buff;
    arg->buff = buff;
    arg->is_buffered = false;
    arg->buffered_addr = 0;
    arg->capacity = capacity;
    arg->len = 0;

    kreader->func = kas_kreader_buffered_func;
    kreader->private_arg = arg;

    return true;

free_arg:
    kas_free(arg);
error:
    return false;
}

void kas_kreader_buffered_unwrap(struct kreader *kreader) {
    assert(kreader->func == kas_kreader_buffered_func);

    struct kas_kreader_buffered_arg *arg = kreader->private_arg;

    struct kreader base_kreader = arg->base_kreader;

    if ( !arg->external_buff ) {
        assert(arg->capacity == KAS_KREADER_BUFFER_CAPACITY);

        kas_free(arg->buff);
    }

    *kreader = base_kreader;
}
