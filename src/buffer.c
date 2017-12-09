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
        ch_free(pool->handlers);
        ch_free(pool);
    }
}

// .. c:function::
ch_error_t
ch_bf_init(ch_buffer_pool_t* pool, ch_connection_t* conn, uint8_t max_buffers)
//    :noindex:
//
//    See: :c:func:`ch_bf_init`
//
// .. code-block:: cpp
//
{
    int i;
    A(max_buffers <= 32, "buffer.c can't handle more than 32 handlers");
    memset(pool, 0, sizeof(*pool));
    pool->conn         = conn;
    pool->refcnt       = 1;
    size_t pool_mem    = max_buffers * sizeof(ch_bf_handler_t);
    pool->used_buffers = 0;
    pool->max_buffers  = max_buffers;
    pool->handlers     = ch_alloc(pool_mem);
    if (!pool->handlers) {
        fprintf(stderr,
                "%s:%d Fatal: Could not allocate memory fo r buffers. "
                "ch_buffer_pool_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) pool);
        return CH_ENOMEM;
    }
    memset(pool->handlers, 0, pool_mem);
    pool->free_buffers = 0xFFFFFFFFU;
    pool->free_buffers <<= (32 - max_buffers);
    for (i = 0; i < max_buffers; ++i) {
        pool->handlers[i].id   = i;
        pool->handlers[i].used = 0;
    }
    return CH_SUCCESS;
}

// .. c:function::
ch_bf_handler_t*
ch_bf_acquire(ch_buffer_pool_t* pool)
//    :noindex:
//
//    See: :c:func:`ch_bf_acquire`
//
// .. code-block:: cpp
//
{
    ch_bf_handler_t* handler_buf;
    if (pool->used_buffers < pool->max_buffers) {
        int free;
        pool->used_buffers += 1;
        pool->refcnt += 1;
        free = ch_msb32(pool->free_buffers);
        /* Reserve the buffer. */
        pool->free_buffers &= ~(1 << (free - 1));
        /* The msb represents the first buffer. So the value is inverted. */
        handler_buf = &pool->handlers[32 - free];
        A(handler_buf->used == 0, "Handler buffer already used.");
        handler_buf->used = 1;
        memset(&handler_buf->msg, 0, sizeof(handler_buf->msg));
        handler_buf->msg._handler = handler_buf->id;
        handler_buf->msg._pool    = pool;
        handler_buf->msg._flags   = CH_MSG_IS_HANDLER;
        return handler_buf;
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
    ch_bf_handler_t* handler_buf = &pool->handlers[id];
    A(handler_buf->used == 1, "Double release of buffer.");
    A(pool->used_buffers > 0, "Buffer pool inconsistent.");
    A(handler_buf->id == id, "Id changed.");
    A(handler_buf->msg._handler == id, "Id changed.");
    int in_pool = pool->free_buffers & (1 << (31 - id));
    A(!in_pool, "Buffer already in pool");
    if (in_pool) {
        fprintf(stderr,
                "%s:%d Fatal: Double release of handler buffer. "
                "ch_buffer_pool_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) pool);
        return;
    }
    ch_bf_free(pool);
    pool->used_buffers -= 1;
    /* Release the buffer. */
    handler_buf->used = 0;
    pool->free_buffers |= (1 << (31 - id));
}
