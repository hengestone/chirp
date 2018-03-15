// =================
// Connection header
// =================
//
// Represents a connection and implements low-level write (low-level read is
// implemented in protocol.h/c).
//
// Connection state machine
// ========================
//
// TODO: Due missing features the state machine is not yet completely followed.
// Please always consider this definitions when changing connection handling.
//
// Variables
// ---------
//
// * C = Connection pointer valid
//
// * Q = A message is queued
//
// * V = Connection timestamp is valid (connection did not timeout)
//
// Actions
// -------
//
// enqu = enqueue
//    Enqueue the message passed to ch_chirp_send().
//
// dequ = dequeue
//    Dequeue a message and send it, or go to idle if there is no message.
//
// bufs = create buffers
//    Create all the data-structures and buffers needed for running a
//    connection.
//
// chck = check
//    Check if a connection has been idle for REUSE_TIME seconds, since the
//    connection is valid: do nothing.
//
// gc = collect connection
//    Check if a connection has been idle for REUSE_TIME seconds, since the
//    connection is not valid: close it if needed and cleanup ch_remote_t.
//
// wait = wait
//    Reconnect after waiting for 1 seconds.
//
// clen = cleanup
//    Cleanup data-structures and buffers, remove pointer to connection from
//    ch_remote_t.
//
//
// States
// ------
//
// null = null
//    ch_remote_t for the destination address in the message does not exist.
//
// clos = closed
//    ch_remote_t does exist but there is no pointer to the connection.
//
// cing = connecting
//    ch_remote_t exists but the connection is still connecting.
//
// idle = idle
//    ch_remote_t exists, the connection is valid and idle.
//
// invalid idle = (Not reachable)
//    Same as idle, but the connection timed out. On next gc it will be removed.
//
// send = sending
//    The connection is blocked, messages will be enqueued and not sent.
//
// Triggers
// --------
//
// send
//    The user of libchirp sends a message
//
// done
//    Defined below
//
// fail
//    Sending of a message fails (all kinds of error are treated the same)
//
// timer
//    The garbage collector timer is triggered (callback from libuv)
//
//
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |                      | send      | done      | fail      | timer      |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |Name         | C| Q| V| act | next| act | next| act | next| act  | next|
// +=============+==+==+==+=====+=====+=====+=====+=====+=====+======+=====+
// |null         | 0| 0| 0| init| cing| init| idle| xxx | xxx | xxx  | xxx |
// |             |  |  |  | enqu|     | bufs|     |     |     |      |     |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |closed       | 0| 0| 1| conn| cing| xxx | xxx | xxx | xxx | chck | clos|
// |             |  |  |  | enqu|     |     |     |     |     |      |     |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |xxx          | 0| 1| 0| xxx | xxx | xxx | xxx | xxx | xxx | xxx  | xxx |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |connecting   | 0| 1| 1| enqu| cing| bufs| send| wait| cing| chck | cing|
// |             |  |  |  |     |     |     |     |     | clos|      |     |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |invalid idle | 1| 0| 0| enqu| send| xxx | xxx | clen| clos| gc   | null|
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |idle         | 1| 0| 1| enqu| send| xxx | xxx | clen| clos| chck | idle|
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |xxx          | 1| 1| 0| xxx | xxx | xxx | xxx | xxx | xxx | xxx  | xxx |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
// |sending      | 1| 1| 1| enqu| send| dequ| send| wait| cing| chck | send|
// |             |  |  |  |     |     |     | idle|     |     |      |     |
// +-------------+--+--+--+-----+-----+-----+-----+-----+-----+------+-----+
//
// Wide version of this table is in doc/chirp_state_machine.ods
//
// Done depnds on current state:
//
// * null: successful accept()
//
// * connecting: successful connect()
//
// * sending: successful send()
//
// One important thing to understand. There can be network races and if the two
// ends of a peer-association open a connection at the same time, while we just
// close one connection when we receive another one, we can end up in a endless
// loop. Since if both nodes do not decide to close the same one, both
// connections are gone and we start fresh. We do not solve that problem by
// randomized reconnect timeouts, but rather by allowing two connections for
// REUSE_TIME + random time. That is the reason, the queues and
// wait_ack_message are in a shared structure called ch_remote_t. If both
// connections are used they will stay.
//
// .. code-block:: cpp
//
#ifndef ch_connection_h
#define ch_connection_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp/chirp.h"
#include "message.h"
#include "qs.h"
#include "rbtree.h"
#include "reader.h"
#include "writer.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <openssl/bio.h>
#include <openssl/ssl.h>

// Declarations
// ============

// .. c:type:: ch_cn_flags_t
//
//    Represents connection flags.
//
//    .. c:member:: CH_CN_SHUTTING_DOWN
//
//       Indicates that the connection is shutting down.
//
//    .. c:member:: CH_CN_WRITE_PENDING
//
//       Indicates that There is a write pending.
//
//    .. c:member:: CH_CN_TLS_HANDSHAKE
//
//      Indicates that a handshake is running.
//
//    .. c:member:: CH_CN_ENCRYPTED
//
//       Indicates whether the connection is encrypted or not.
//
//    .. c:member:: CH_CN_BUF_WTLS_USED
//
//       Indicates if TLS for writing is used.
//
//    .. c:member:: CH_CN_BUF_RTLS_USED
//
//       Indicates if TLS for reading is used.
//
//    .. c:member:: CH_CN_BUF_UV_USED
//
//       Indicates that the connection buffer is currently used by libuv.
//
//    .. c:member:: CH_CN_DO_CLOSE_ACCOUTING
//
//       Indicates that this close should happened during chirp close and
//       therefore has to be accounted for in closeing_tasks.
//
//    .. c:member:: CH_CN_INIT_CLIENT
//
//       The uv tcp client has been initialized.
//
//    .. c:member:: CH_CN_INIT_READER_WRITER
//
//       The reader/writer has been initialized.
//
//    .. c:member:: CH_CN_INIT_SHUTDOWN_TIMEOUT
//
//       The shutdown timeout has been initialized.
//
//    .. c:member:: CH_CN_INIT_CONNECT_TIMEOUT
//
//       The connect timeout has been initialized.
//
//    .. c:member:: CH_CN_INIT_ENCRYPTION
//
//       The encryption has been initialized.
//
//    .. c:member:: CH_CN_INIT_BUFFERS
//
//       The buffers have been initialized.
//
// .. code-block:: cpp

typedef enum {
    CH_CN_SHUTTING_DOWN        = 1 << 0,
    CH_CN_CONNECTED            = 1 << 1,
    CH_CN_WRITE_PENDING        = 1 << 2,
    CH_CN_TLS_HANDSHAKE        = 1 << 3,
    CH_CN_ENCRYPTED            = 1 << 4,
    CH_CN_BUF_WTLS_USED        = 1 << 5,
    CH_CN_BUF_RTLS_USED        = 1 << 6,
    CH_CN_BUF_UV_USED          = 1 << 7,
    CH_CN_DO_CLOSE_ACCOUTING   = 1 << 8,
    CH_CN_STOPPED              = 1 << 9,
    CH_CN_INCOMING             = 1 << 10,
    CH_CN_INIT_CLIENT          = 1 << 11,
    CH_CN_INIT_READER_WRITER   = 1 << 12,
    CH_CN_INIT_CONNECT_TIMEOUT = 1 << 13,
    CH_CN_INIT_ENCRYPTION      = 1 << 14,
    CH_CN_INIT_BUFFERS         = 1 << 15,
    CH_CN_INIT =
            (CH_CN_INIT_CLIENT | CH_CN_INIT_READER_WRITER |
             CH_CN_INIT_ENCRYPTION | CH_CN_INIT_BUFFERS)
} ch_cn_flags_t;

// .. c:type:: ch_resume_state_t
//
//    Defines the state of a reader.
//
//    .. c:member:: void* rest_of_buffer:
//
//       Buffer active before stop. Point to the rest of the data, that aren't
//       handled yet.
//
//    .. c:member:: size_t bytes_to_read:
//
//       Bytes that still needs to be read after a stopping the reader.
//
// .. code-block:: cpp
//
typedef struct ch_resume_state_s {
    void*  rest_of_buffer;
    size_t bytes_to_read;
} ch_resume_state_t;

// .. c:type:: ch_connection_t
//
//    Connection dictionary implemented as red-black tree.
//
//    .. c:member:: uint8_t ip_protocol
//
//       What IP protocol (IPv4 or IPv6) shall be used for connections.
//
//    .. c:member:: uint8_t[16] address
//
//       IPv4/6 address of the sender if the message was received.  IPv4/6
//       address of the recipient if the message is going to be sent.
//
//    .. c:member:: int32_t port
//
//       The port that shall be used for connections.
//
//    .. c:member:: uint8_t[16] remote_identity
//
//       The identity of the remote target. This is used for getting the remote
//       address.
//
//    .. c:member:: uv_tcp_t client
//
//       The TCP handle (TCP stream) of the client, which is used to get the
//       address of the peer connected to the handle.
//
//    .. c:member:: uv_connect_t connect
//
//       Libuv connect handler.
//
//    .. c:member:: uv_buf* buffer_uv
//
//       Pointer to the libuv (data-) buffer data type.
//
//    .. c:member:: uv_buf* buffer_wtls
//
//       Pointer to the libuv buffer data type for writing data over TLS.
//
//    .. c:member:: uv_buf* buffer_rtls
//
//       Pointer to the libuv buffer data type for reading data over TLS.
//
//    .. c:member:: uv_buf_t buffer_uv_uv
//
//       The actual libuv (data-) buffer (using the buffer_uv data type).
//
//    .. c:member:: uv_buf_t buffer_wtls_uv
//
//       The actual libuv buffer for writing data over TLS (using the
//       buffer_wtls data type).
//
//    .. c:member:: uv_buf_t buffer_any_uv
//
//       Generic libuv (data-) buffer used for writing over a connection. The
//       data type of the buffer must be provided when writing.
//
//    .. c:member:: size_t buffer_size
//
//       The size of libuv (data-) buffers.
//
//    .. c:member:: size_t buffer_tls_size
//
//       The size of the tls buffers.
//
//    .. c:member:: uv_write_cb write_callback
//
//       A callback which will be called after a sucessful write over a
//       connection.
//
//    .. c:member:: size_t write_written
//
//       Holds how many bytes have been written over a connection. This is
//       typically zero at first and gets increased with each partial write.
//
//    .. c:member:: size_t write_size
//
//       Indicates how many bytes shall in total be written over a connection.
//
//    .. c:member:: ch_buf* write_buffer
//
//       Pointer to the write buffer. The buffer that will be written to.
//
//    .. c:member:: ch_remote_t* remote
//
//       Pointer to the remote of this connection.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Pointer to the chirp object. See: :c:type:`ch_chirp_t`.
//
//    .. c:member:: uv_write_t write_req
//
//       Write request objet, which is used to write data on a handle.
//
//    .. c:member:: uv_timer_t connect_timeout
//
//       Connect timeout. Used to stop connecting if there is no answer or
//       handshake get stalled.
//
//    .. c:member:: int8_t shutdown_tasks
//
//       Counter for tasks that need to be done when shutting down a connection.
//       Tasks may be calling closing-callbacks for example, on a request handle
//       or the shutdown timer handle. This acts as semaphore.
//
//    .. c:member:: uint32_t flags
//
//       Flags indicating the state of a connection, e.g. shutting down, write
//       pending, TLS handshake, whether the connection is encrypted or not and
//       so on, see :c:type:`ch_cn_flags_t`.
//
//    .. c:member:: SSL* ssl
//
//       Pointer to a SSL (data-) structure. This is used when using an
//       encrypted connection over SSL.
//
//    .. c:member:: BIO* bio_ssl
//
//       Pointer to the BIO structure of SSL. BIO is an I/O stream abstraction
//       and essentially OpenSSL's answer to the C library's FILE pointer. This
//       is used to create a connected BIO pair alongside with
//       :c:member:`bio_app` and is only used for the SSL connection.
//
//    .. c:member:: BIO* bio_app
//
//       Pointer to the applications BIO structure. This is used to read and
//       write (partial) data over TLS.
//
//    .. c:member:: int tls_handshake_state
//
//       Holds the current state of the SSL handshake when using an encrypted
//       connection and TLS handshakes. This is used within the protocol, see
//       :c:func:`_ch_pr_do_handshake`.
//
//    .. c:member:: ch_reader_t reader
//
//       Handle to a chirp reader, handles handshakes and reads (buffers) on a
//       connection.
//
//    .. c:member:: ch_writer_t writer
//
//       Handle to a chirp writer, handles sending and writing on a connection.
//
//    .. c:member:: uint64_t timestamp
//
//       Timestamp when the connection was last used. Used to determine
//       garbage-collection.
//
//    .. c:member:: uint32_t release_serial
//
//       The serial of the message currently released.
//
//    .. c:member:: ch_message_t ack_msg
//
//       Buffer used for ack message
//
//    .. c:member:: char color
//
//       rbtree member
//
//    .. c:member:: ch_connection_t* left
//
//       rbtree member
//
//    .. c:member:: ch_connection_t* right
//
//       rbtree member
//
//    .. c:member:: ch_connection_t* next
//
//       stack member
//
// .. code-block:: cpp
//
struct ch_connection_s {
    uint8_t           ip_protocol;
    uint8_t           address[CH_IP_ADDR_SIZE];
    int32_t           port;
    uint8_t           remote_identity[CH_ID_SIZE];
    ch_chirp_t*       chirp;
    ch_remote_t*      remote;
    uv_tcp_t          client;
    uv_connect_t      connect;
    ch_buf*           buffer_uv;
    ch_buf*           buffer_wtls;
    ch_buf*           buffer_rtls;
    uv_buf_t          buffer_uv_uv;
    uv_buf_t          buffer_wtls_uv;
    uv_buf_t          buffer_any_uv;
    size_t            buffer_size;
    size_t            buffer_rtls_size;
    uv_write_cb       write_callback;
    size_t            write_written;
    size_t            write_size;
    ch_buf*           write_buffer;
    ch_resume_state_t read_resume;
    ch_resume_state_t tls_resume;
    uv_write_t        write_req;
    uv_timer_t        connect_timeout;
    int8_t            shutdown_tasks;
    uint32_t          flags;
    SSL*              ssl;
    BIO*              bio_ssl;
    BIO*              bio_app;
    int               tls_handshake_state;
    ch_reader_t       reader;
    ch_writer_t       writer;
    uint64_t          timestamp;
    uint32_t          release_serial;
    ch_message_t      ack_msg;
    char              color;
    ch_connection_t*  parent;
    ch_connection_t*  left;
    ch_connection_t*  right;
    ch_connection_t*  next;
};

// TODO: Timestamp has to be in ch_connection_t because of old connections

// Data Struct Prototypes
// ----------------------
//
// .. code-block:: cpp

#define ch_cn_cmp_m(x, y) rb_pointer_cmp_m(x, y)
rb_bind_decl_m(ch_cn, ch_connection_t) CH_ALLOW_NL;

qs_stack_bind_decl_m(ch_cn_st, ch_connection_t) CH_ALLOW_NL;

// .. c:function::
void
ch_cn_abort_one_message(ch_remote_t* remote, ch_error_t error);
//
//    Abort one message in queue, because connecting failed.
//
//    :param ch_remote_t* remote: Remote failed to connect.
//    :param ch_error_t error: Status returned by connect.

// .. c:function::
void
ch_cn_close_cb(uv_handle_t* handle);
//
//    Called by libuv after closing a connection handle.
//
//    :param uv_handle_t* handle: The libuv handle holding the
//                                connection

// .. c:function::
void
ch_cn_read_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
//
//    Allocates buffers on the connection and reuses it for each subsequent
//    reads. Also allocates the buffer for TLS since it has to be the same
//    size.
//
//    :param uv_handle_t* handle: The libuv handle holding the
//                                connection
//    :param size_t suggested_size: The size of the connection buffer
//                                  in bytes.
//    :param uv_buf_t* buf: Libuv buffer which will hold the
//                          connection

// .. c:function::
ch_error_t
ch_cn_shutdown(ch_connection_t* conn, int reason);
//
//    Shutdown this connection.
//
//    :param ch_connection_t* conn: Connection dictionary holding a
//                                  chirp instance.
//    :param int reason: The error code that caused the shutdown, it will be
//                       reported to the user.
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype: ch_error_t

// .. c:function::
ch_error_t
ch_cn_init(ch_chirp_t* chirp, ch_connection_t* conn, uint8_t flags);
//
//    Initialize a connection. Please do:
//
//    .. code-block:
//
//       memset(conn, 0, sizeof(*conn));
//
//    before calling this. We usually do this, but this is an exception.
//
//    :param ch_chirp_t* chirp: Chirp instance
//    :param ch_connection_t* conn: Connection to initialize
//    :param uint8_t flags: Pass CH_CN_ENCRYPTED for a encrypted connection, 0
//                          otherwise
//

// .. c:function::
ch_error_t
ch_cn_init_enc(ch_chirp_t* chirp, ch_connection_t* conn);
//
//    Initialize encryption.
//
//    :param ch_chirp_t* chirp: Chirp instance
//    :param ch_connection_t* conn: Connection to initialize
//

// .. c:function::
void
ch_cn_send_if_pending(ch_connection_t* conn);
//
//    Send all pending handshake data from SSL
//
//    :param ch_connection_t* conn: Connection
//
//
// .. c:function::
void
ch_cn_write(
        ch_connection_t* conn, void* buf, size_t size, uv_write_cb callback);
//
//    Send data to remote
//
//    :param ch_connection_t* conn: Connection
//    :param void* buf: Buffer to send. It must stay valid till the callback is
//                      called.
//    :param size_t size: Size of data to send
//    :param uv_write_cb: Callback when data is written, can be NULL
//
//
// .. code-block:: cpp

#endif // ch_connection_h
