// ============
// Chirp header
// ============
//
// Features
// ========
//
// * Fully automatic connection setup
//
// * TLS support
//
//   * Connections to 127.0.0.1 and ::1 aren't encrypted
//
// * Easy message routing
//
// * Robust
//
//   * No message can be lost without an error (or it is a bug)
//
// * Very thin API
//
// * Minimal code-base, all additional features will be implemented as modules
//   in an upper layer
//
// * Fast
//
//   * Up to 50'000 msg/s on a single-connection (encrypted 35'000 msg/s)
//   * Up to 100'000 msg/s in star-topology (encrypted same)
//
//     * Which shows that chirp is highly optimized, but still if the network
//       delay is bigger star-topology is the way to go.
//
// .. code-block:: cpp
//
// Structures
// ==========
//
// default is composition where the child is part of the parent
//
// -> means aggregation. It also means the parent structure is not the owner of
// the child structure.
//
// \* means composition with a pointer (can be replaced)
//
// .. code-block:: text
//
//    ch_chirp_t
//        ch_chirp_int_t (pimpl)
//            ch_protocol_t (connecting / accept)
//                remote dictionary (ch_remote_t)
//                old connection dictionary (ch_connection_t)
//            ch_encryption_t (interface to \*ssl)
//            ch_buffer_pool_t
//
//    ch_remote_t (allows replacing connection to remote note)
//        \*ch_connection_t (can be NULL)
//            ch_writer_t
//            ch_reader_t
//        message-queue (-> ch_message_t)
//
//    ch_message_t
//
#ifndef ch_chirp_h
#define ch_chirp_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "encryption.h"
#include "libchirp.h"
#include "protocol.h"

// Declarations
// ============

// .. c:type:: ch_chirp_flags_t
//
//    Represents chirps flags.
//
//    .. c:member:: CH_CHIRP_AUTO_STOP
//
//       Stop the loop on closing. This is useful if the loop is only used by
//       chirp.
//
//    .. c:member:: CH_CHIRP_CLOSED
//
//       Chirp is closed.
//
//    .. c:member:: CH_CHIRP_CLOSING
//
//       Chirp is being closed.
//
// .. code-block:: cpp
//
typedef enum {
    CH_CHIRP_AUTO_STOP = 1 << 0,
    CH_CHIRP_CLOSED    = 1 << 1,
    CH_CHIRP_CLOSING   = 1 << 2,
} ch_chirp_flags_t;

// .. c:type:: ch_chirp_int_t
//
//    Chirp object.
//
//    .. c:member:: ch_config_t config
//
//       The current chirp configuration.
//
//    .. c:member:: int closing_tasks
//
//       Counter for the number of tasks when closing a connection (e.g.
//       shutdown). This acts as semaphore.
//
//    .. c:member:: uint8_t flags
//
//       Holds the flags from :c:type:`ch_chirp_flags_t`, hence indicates
//       whether chirp is closing, already closed and if the loop shall be
//       stopped when closing.
//
//    .. c:member:: uv_async_t start
//
//       Asynchronous handler called when chirp is ready.
//
//    .. c:member:: ch_start_cb_t start_cb
//
//       User supplied callback called when chirp is ready.
//
//    .. c:member:: uv_async_t close
//
//       Asynchronous handler to close chirp on the main-loop.
//
//    .. c:member:: uv_signal_t signals[2]
//
//       Libuv handles for managing unix signals. We currently register two
//       handlers, one for SIGINT, one for SIGTERM.
//
//    .. c:member:: uv_prepare_t close_check
//
//       Handle which will run the given callback (close callback, closes chirp
//       when the closing semaphore reaches zero) once per loop iteration.
//
//    .. c:member:: ch_protocol_t protocol
//
//       Reference to protocol object. Provides access to connection and data
//       specific functions.
//
//    .. c:member:: ch_encryption_t encryption
//
//       Reference to encryption object. Is used when a encrypted connection is
//       used.
//
//    .. c:member:: uv_loop_t* loop
//
//       Pointer to the libuv (main) event loop. The event loop is the central
//       part of libuv’s functionality. It takes care of polling for i/o and
//       scheduling callbacks to be run based on different sources of events.
//
//    .. c:member:: uint8_t identity[16]
//
//       Array holding the identity of this chirp configuration.
//
//    .. c:member:: uint16_t public_port
//
//       The public port which this chirp configuration uses for connections.
//
//    .. c:member:: ch_message_t* send_ts_queue
//
//       The send queue for ch_chirp_send_ts
//
//    .. c:member:: uv_async_t send_ts
//
//       Async event for waking up _ch_wr_send_ts_cb
//
//    .. c:member:: uv_mutex_t send_ts_queue_lock
//
//       Mutex to lock the send_ts_queue
//
//    .. c:member:: ch_recv_cb_t recv_cb
//
//       Callback when message is received
//
// .. code-block:: cpp
//
struct ch_chirp_int_s {
    ch_config_t     config;
    int             closing_tasks;
    uint8_t         flags;
    uv_async_t      close;
    uv_async_t      start;
    ch_start_cb_t   start_cb;
    uv_signal_t     signals[2];
    uv_prepare_t    close_check;
    ch_protocol_t   protocol;
    ch_encryption_t encryption;
    uv_loop_t*      loop;
    uint8_t         identity[CH_ID_SIZE];
    uint16_t        public_port;
    ch_message_t*   send_ts_queue;
    uv_async_t      send_ts;
    uv_mutex_t      send_ts_queue_lock;
    ch_recv_cb_t    recv_cb;
    uv_async_t      done;
    ch_done_cb_t    done_cb;
};

#ifndef NDEBUG
extern int _ch_tst_fail_init_at_end;
#endif

// .. c:function::
void
ch_chirp_close_cb(uv_handle_t* handle);
//
//    Reduce closing callback semaphore.
//
//    :param uv_handle_t* handle: A libuv handle containing the chirp object
//

// .. c:function::
void
ch_chirp_finish_message(
        ch_chirp_t*      chirp,
        ch_connection_t* conn,
        ch_message_t*    msg,
        int              status);
//
//    Call the user callback and then check the message queue for further
//    messages and send them. It will finish once the writer is done and the
//    ACK as been received.
//
//    :param ch_chirp_t* writer: Chirp instance
//    :param ch_connection_t* conn: Pointer to connection
//    :param ch_message_t* msg: Pointer to the message
//    :param int status: Error code
//

#endif // ch_chirp_h
