// ======
// Buffer
// ======

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "buffer.h"
#include "util.h"

// Definitions
// ===========
//
// .. c:function::
static inline int
ch_msb32(uint32_t x)
//
//    Get the most significant bit set of a set of bits.
//
//    :param uint32_t x:  The set of bits.
//
//    :return:            the most significant bit set.
//    :rtype:             uint32_t
//
// .. code-block:: cpp
//
{
    static const uint32_t bval[] = {
            0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

    uint32_t r = 0;
    if (x & 0xFFFF0000) {
        r += 16 / 1;
        x >>= 16 / 1;
    }
    if (x & 0x0000FF00) {
        r += 16 / 2;
        x >>= 16 / 2;
    }
    if (x & 0x000000F0) {
        r += 16 / 4;
        x >>= 16 / 4;
    }
    return r + bval[x];
}

// .. c:function::
void
ch_bf_free(ch_buffer_pool_t* pool)
//    :noindex:
//
//    See: :c:func:`ch_bf_free`
//
// .. code-block:: cpp
//
{
    pool->refcnt -= 1;
    if (pool->refcnt == 0) {
        ch_free(pool->slots);
        ch_free(pool);
    }
}

// .. c:function::
ch_error_t
ch_bf_init(ch_buffer_pool_t* pool, ch_connection_t* conn, uint8_t max_slots)
//    :noindex:
//
//    See: :c:func:`ch_bf_init`
//
// .. code-block:: cpp
//
{
    int i;
    A(max_slots <= 32, "can't handle more than 32 slots");
    memset(pool, 0, sizeof(*pool));
    pool->conn       = conn;
    pool->refcnt     = 1;
    size_t pool_mem  = max_slots * sizeof(ch_bf_slot_t);
    pool->used_slots = 0;
    pool->max_slots  = max_slots;
    pool->slots      = ch_alloc(pool_mem);
    if (!pool->slots) {
        fprintf(stderr,
                "%s:%d Fatal: Could not allocate memory for buffers. "
                "ch_buffer_pool_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) pool);
        return CH_ENOMEM;
    }
    memset(pool->slots, 0, pool_mem);
    pool->free_slots = 0xFFFFFFFFU;
    pool->free_slots <<= (32 - max_slots);
    for (i = 0; i < max_slots; ++i) {
        pool->slots[i].id   = i;
        pool->slots[i].used = 0;
    }
    return CH_SUCCESS;
}

// .. c:function::
ch_bf_slot_t*
ch_bf_acquire(ch_buffer_pool_t* pool)
//    :noindex:
//
//    See: :c:func:`ch_bf_acquire`
//
// .. code-block:: cpp
//
{
    ch_bf_slot_t* slot_buf;
    if (pool->used_slots < pool->max_slots) {
        int free;
        pool->used_slots += 1;
        free = ch_msb32(pool->free_slots);
        /* Reserve the buffer. */
        pool->free_slots &= ~(1 << (free - 1));
        /* The msb represents the first buffer. So the value is inverted. */
        slot_buf = &pool->slots[32 - free];
        A(slot_buf->used == 0, "Slot already used.");
        slot_buf->used = 1;
        memset(&slot_buf->msg, 0, sizeof(slot_buf->msg));
        slot_buf->msg._slot  = slot_buf->id;
        slot_buf->msg._pool  = pool;
        slot_buf->msg._flags = CH_MSG_HAS_SLOT;
        return slot_buf;
    }
    return NULL;
}

// .. c:function::
void
ch_bf_release(ch_buffer_pool_t* pool, int id)
//    :noindex:
//
//    See: :c:func:`ch_bf_release`
//
// .. code-block:: cpp
//
{
    ch_bf_slot_t* slot_buf = &pool->slots[id];
    A(slot_buf->used == 1, "Double release of slot.");
    A(pool->used_slots > 0, "Buffer pool inconsistent.");
    A(slot_buf->id == id, "Id changed.");
    A(slot_buf->msg._slot == id, "Id changed.");
    int in_pool = pool->free_slots & (1 << (31 - id));
    A(!in_pool, "Buffer already in pool");
    if (in_pool) {
        fprintf(stderr,
                "%s:%d Fatal: Double release of slot. "
                "ch_buffer_pool_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) pool);
        return;
    }
    pool->used_slots -= 1;
    /* Release the buffer. */
    slot_buf->used = 0;
    pool->free_slots |= (1 << (31 - id));
}
