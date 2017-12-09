// =============
// Reader Header
// =============
//
// Reader state machine and buffer pool. Everything about reading a message
// from the wire.
//
// .. code-block:: cpp
//
#ifndef ch_reader_h
#define ch_reader_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "buffer.h"
#include "common.h"
#include "message.h"
#include "serializer.h"

// Declarations
// ============

// .. c:type:: ch_rd_state_t
//
//    Possible states of a reader.
//
//    .. c:member:: CH_RD_START
//
//       Initial state, chirp handshake has to be done.
//
//    .. c:member:: CH_RD_WAIT
//
//       Wait for the next message.
//
//    .. c:member:: CH_RD_HANDLER
//
//       Acquire a handler.
//
//    .. c:member:: CH_RD_HEADER
//
//       Read header.
//
//    .. c:member:: CH_RD_DATA
//
//       Read data.
//
// .. code-block:: cpp
//
typedef enum {
    CH_RD_START     = 0,
    CH_RD_HANDSHAKE = 1,
    CH_RD_WAIT      = 2,
    CH_RD_HANDLER   = 3,
    CH_RD_HEADER    = 4,
    CH_RD_DATA      = 5,
} ch_rd_state_t;

// .. c:type:: ch_reader_t
//
//    Defines the state of a reader.
//
//    .. c:member:: ch_rd_state_t state
//
//       Current state of the reader (finite-state machine).
//
//    .. c:member:: ch_message_t* msg
//
//       Current message
//
//    .. c:member:: ch_bf_handler_t* handler
//
//       Current handler buffer
//
//    .. c:member:: ch_message_t ack_msg
//
//       Buffer used for ack message
//
//    .. c:member:: ch_message_t wire_msg
//
//       Buffer used store wire message
//
//    .. c:member:: size_t bytes_read
//
//       Counter for how many bytes were already read by the reader. This is
//       used when :c:func:`ch_rd_read` is called with a buffer of
//       :c:member:`ch_rd_read.read` bytes to read but not enough bytes are
//       being delivered over the connection :c:member:`ch_rd_read.conn`.
//
//    .. c:member:: ch_buffer_pool_t pool
//
//       Data structure containing preallocated buffers for the chirp handlers.
//
// .. code-block:: cpp
//
typedef struct ch_reader_s {
    ch_rd_state_t     state;
    ch_bf_handler_t*  handler;
    ch_message_t      ack_msg;
    ch_message_t      wire_msg;
    size_t            bytes_read;
    ch_buf            net_msg[CH_SR_WIRE_MESSAGE_SIZE];
    ch_buffer_pool_t* pool;
} ch_reader_t;

// .. c:function::
void
ch_rd_free(ch_reader_t* reader);
//
//    Free the (data-) buffer pool of the given reader instance.
//
//    :param ch_reader_t* reader: The reader instance whose buffer
//                                pool shall be freed.

// .. c:function::
ch_error_t
ch_rd_init(ch_reader_t* reader, ch_connection_t* conn, ch_chirp_int_t* ichirp);
//
//    Initialize the reader structure.
//
//    :param ch_reader_t* reader: The reader instance whose buffer pool shall
//                                be initialized with ``max_buffers``.
//    :param ch_connection_t* conn: Connection that owns the reader
//    :param ch_chirp_int_t ichirp: Internal chirp instance
//    :rtype: ch_error_t

// .. c:function::
ssize_t
ch_rd_read(ch_connection_t* conn, ch_buf* buffer, size_t bytes_read, int* stop);
//
//    Implements the wire protocol reader part. Returns bytes handled.
//
//    :param ch_connection_t* conn: Connection the data was read from.
//    :param void* buffer:          The buffer containing ``read`` bytes read.
//    :param size_t bytes_read:     The number of bytes read.
//    :param int* stop:             (Out) Stop the reading process.
//
// .. code-block:: cpp

#endif // ch_reader_h
