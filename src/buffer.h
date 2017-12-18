// =============
// Buffer header
// =============
//
// Implements a buffer pool. There is header and data buffer per chrip
// handler.
//
// .. code-block:: cpp
//
#ifndef ch_buffer_h
#define ch_buffer_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"
#include "libchirp-config.h"
#include "message.h"

// Declarations
// ============

// .. c:type:: ch_bf_handler_t
//
//    Preallocated buffer for a chirp handler.
//
//    .. c:member:: ch_message_t
//
//       Preallocated message.
//
//    .. c:member:: ch_buf* header
//
//       Preallocated buffer for the chirp header.
//
//    .. c:member:: ch_buf* data
//
//       Preallocated buffer for the data.
//
//    .. c:member:: uint8_t id
//
//       Identifier of the buffer.
//
//    .. c:member:: uint8_t used
//
//       Indicates how many times the buffer is/was used.
//
// .. code-block:: cpp
//
typedef struct ch_bf_handler_s {
    ch_message_t msg;
    ch_buf       header[CH_BF_PREALLOC_HEADER];
    ch_buf       data[CH_BF_PREALLOC_DATA];
    uint8_t      id;
    uint8_t      used;
} ch_bf_handler_t;

// .. c:type:: ch_buffer_pool_t
//
//    Contains the preallocated buffers for the chirp handlers.
//
//    .. c:member:: unsigned int refcnf
//
//       Reference count
//
//    .. c:member:: uint8_t max_buffers
//
//       Defines the maximum number of buffers.
//
//    .. c:member:: uint8_t used_buffers
//
//       Defines how many buffers are currently used.
//
//    .. c:member:: uint32_t free_buffers
//
//       Defeines how many buffers are currently free (and therefore may be
//       used).
//
//    .. c:member:: ch_bf_handler_t* handlers
//
//       Pointer of type ch_bf_handler_t to the actual handlers. See
//       :c:type:`ch_bf_handler_t`.
//
//    .. c:member:: ch_connection_t*
//
//       Pointer to connection that owns the pool
//
// .. code-block:: cpp
//
typedef struct ch_buffer_pool_s {
    unsigned int     refcnt;
    uint8_t          max_buffers;
    uint8_t          used_buffers;
    uint32_t         free_buffers;
    ch_bf_handler_t* handlers;
    ch_connection_t* conn;
} ch_buffer_pool_t;

// .. c:function::
void
ch_bf_free(ch_buffer_pool_t* pool);
//
//    Free the given buffer structure.
//
//    :param ch_buffer_pool_t* pool: The buffer pool structure to free
//

// .. c:function::
ch_error_t
ch_bf_init(ch_buffer_pool_t* pool, ch_connection_t* conn, uint8_t max_buffers);
//
//    Initialize the given buffer pool structure using given max. buffers.
//
//    :param ch_buffer_pool_t* pool: The buffer pool object
//    :param ch_connection_t* conn: Connection that owns the pool
//    :param uint8_t max_buffers: Buffers to allocate
//

// .. c:function::
ch_bf_handler_t*
ch_bf_acquire(ch_buffer_pool_t* pool);
//
//    Acquire and return a new handler buffer from the pool. If no handler can
//    be reserved NULL is returned.
//
//    :param ch_buffer_pool_t* pool: The buffer pool structure which the
//                                   reservation shall be made from.
//    :return: a pointer to a reserved handler buffer from the given buffer
//            pool. See :c:type:`ch_bf_handler_t`
//    :rtype:  ch_bf_handler_t
//
// .. c:function::
static inline int
ch_bf_is_exhausted(ch_buffer_pool_t* pool)
//
//    Returns 1 if the pool is exhausted.
//
//    :param ch_buffer_pool_t* pool: The buffer pool object
{
    return pool->used_buffers >= pool->max_buffers;
}

// .. c:function::
void
ch_bf_release(ch_buffer_pool_t* pool, int id);
//
//    Set given handler buffer as unused in the buffer pool structure and
//    (re-)add it to the list of free buffers.
//
//    :param int id: The id of the buffer that should be marked free
//
// .. code-block:: cpp
//
#endif // ch_buffer_h
