// =====
// Chirp
// =====
//
// Interface to chirp: Config, startup, closing and sending messages.
//
// .. code-block:: cpp
//
#ifndef ch_libchirp_chirp_h
#define ch_libchirp_chirp_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "callbacks.h"
#include "common.h"
#include "message.h"
#include "wrappers.h"

// Declarations
// ============

// .. c:type:: ch_config_t
//
//    Chirp configuration.
//
//    .. c:member:: float REUSE_TIME
//
//       Time until a connection gets garbage collected. Until then the
//       connection will be reused.
//
//    .. c:member:: float TIMEOUT
//
//       IO related timeout in seconds: Sending messages, connecting to
//       remotes.
//
//    .. c:member:: uint16_t PORT
//
//       Port for listening to connections.
//
//    .. c:member:: uint8_t BACKLOG
//
//       TCP-listen socket backlog.
//
//    .. c:member:: uint8_t MAX_SLOTS
//
//       The count of message-slots used. Allowed values are values between 1
//       and 32. The default is 0: Use 16 slots if SYNCHRONOUS=0 and 1
//       slot if SYNCHRONOUS=1.
//
//    .. c:member:: char SYNCHRONOUS
//
//       Enable connection-synchronous operations. Default 1. See
//       :ref:`modes-of-operation`
//
//    .. c:member:: char DISABLE_SIGNALS
//
//       By default chirp closes on SIGINT (Ctrl-C) and SIGTERM. Defaults to 0.
//
//    .. c:member:: uint32_t BUFFER_SIZE
//
//       Size of the buffer used for a connection. Defaults to 0, which means
//       use the size requested by libuv. Should not be set below 1024.
//
//    .. c:member:: uint32_t MAX_MSG_SIZE
//
//       Max message size accepted by chirp. If you are concerned about memory
//       usage set config.MAX_SLOTS=1 and config.MAX_MSG_SIZE to something
//       small, depending on your use-case. If you do this, a connection will
//       use about:
//
//       conn_buffers_size = config.BUFFER_SIZE +
//          min(config.BUFFER_SIZE, CH_ENC_BUFFER_SIZE) +
//          sizeof(ch_connection_t) +
//          sizeof(ch_message_t) +
//          (memory allocated by TLS implementation)
//
//       conn_size = conn_buffers_size + config.MAX_MSG_SIZE
//
//       With the default config and SSL conn_buffers_size should be about
//       64k + 16k + 2k + 32k -> 114k. Derived from documentation, no
//       measurement done.
//
//    .. c:member:: uint8_t[16] BIND_V6
//
//       Override IPv6 bind address.
//
//    .. c:member:: uint8_t[4] BIND_V4
//
//       Override IPv4 bind address.
//
//    .. c:member:: uint8_t[16] IDENTITY
//
//       Override the chirp-nodes IDENTITY (this chirp instance). By default
//       all chars are 0, which means chirp will generate a IDENTITY.
//
//    .. c:member:: char* CERT_CHAIN_PEM
//
//       Path to the verification certificate.
//
//    .. c:member:: char* DH_PARAMS_PEM
//
//       Path to the file containing DH parameters.
//
//    .. c:member:: char DISABLE_ENCRYPTION
//
//       Disables encryption. Only use if you know what you are doing.
//       Connections to "127.0.0.1" and "::1" aren't encrypted anyways.
//       Defaults to 0.
//
// .. code-block:: cpp
//
struct ch_config_s {
    float    REUSE_TIME;
    float    TIMEOUT;
    uint16_t PORT;
    uint8_t  BACKLOG;
    uint8_t  MAX_SLOTS;
    char     SYNCHRONOUS;
    char     DISABLE_SIGNALS;
    uint32_t BUFFER_SIZE;
    uint32_t MAX_MSG_SIZE;
    uint8_t  BIND_V6[CH_IP_ADDR_SIZE];
    uint8_t  BIND_V4[CH_IP4_ADDR_SIZE];
    uint8_t  IDENTITY[CH_ID_SIZE]; // 16
    char*    CERT_CHAIN_PEM;
    char*    DH_PARAMS_PEM;
    char     DISABLE_ENCRYPTION;
};

// .. c:type:: ch_chirp_int_t
//    :noindex:
//
//    Opaque pointer to internals.
//
//    see: :c:type:`ch_chirp_int_t`
//
// .. code-block:: cpp
//
typedef struct ch_chirp_int_s ch_chirp_int_t;

// .. c:type:: ch_chirp_t
//
//    Chirp object. It has no public members except user_data and uses an
//    opaque pointer to its internal data structures.
//
//    .. c:member:: void* user_data;
//
//       Pointer to user-data, which can be accessed in :c:type`ch_start_cb_t`
//       and :c:type`ch_done_cb_t` or any other callback that gives access to
//       ch_chirp_t*.
//
// .. code-block:: cpp
//
struct ch_chirp_s {
    void*           user_data;
    ch_chirp_int_t* _;
    uv_thread_t     _thread;
    ch_log_cb_t     _log;
    int             _init;
};

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_close_ts(ch_chirp_t* chirp);
//
//    Clean up chirp object. Close all connections. The chirp object can be
//    freed after the done callback passed to :c:func:`ch_chirp_init` was
//    called.
//
//    This function is thread-safe.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//
//    :return: A chirp error. See: :c:type:`ch_error_t`.
//    :rtype: ch_error_t

// .. c:function::
CH_EXPORT
void
ch_chirp_config_init(ch_config_t* config);
//
//    Initialize chirp configuration with defaults.
//
//    :param ch_config_t* config: Pointer to a chirp configuration.

// .. c:function::
CH_EXPORT
ch_identity_t
ch_chirp_get_identity(ch_chirp_t* chirp);
//
//    Get the identity of the given chirp instance. Is thread-safe after chirp
//    has been initialized.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//
//    :return: a chirp identity. See: :c:type:`ch_identity_t`.
//    :rtype:  ch_identity_t

// .. c:function::
CH_EXPORT
uv_loop_t*
ch_chirp_get_loop(ch_chirp_t* chirp);
//
//    Get the loop of the given chirp object. Is thread-safe after chirp has
//    been initialized.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//
//    :return: a pointer to a libuv event loop object.
//    :rtype:  uv_loop_t*

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_init(
        ch_chirp_t*        chirp,
        const ch_config_t* config,
        uv_loop_t*         loop,
        ch_recv_cb_t       recv_cb,
        ch_start_cb_t      start_cb,
        ch_done_cb_t       done_cb,
        ch_log_cb_t        log_cb);
//
//    Initialiaze a chirp object. Memory is provided by the caller. You must
//    call :c:func:`ch_chirp_close_ts` to cleanup the chirp object.
//
//    You can free **chirp**, **config** and **loop** either after the **done**
//    callback has been called or if chirp is set to auto-stop, after the loop
//    has finished.
//
//    For any other return value than CH_ENOMEM, please await the done_cb
//    before freeing chirp. Config can be freed after ch_chirp_init returns.
//
//    Please call `ch_loop_close(loop)` before freeing the loop. Of course if
//    the loop will continue to run, feel free not to close/free the loop.
//
//    :param ch_chirp_t* chirp: Out: Pointer to a chirp object.
//    :param ch_config_t* config: Pointer to a chirp configration.
//    :param uv_loop_t* loop: Reference to a libuv loop.
//    :param ch_recv_cb_t recv_cb: Called when chirp receives a message.
//    :param ch_start_cb_t start_cb: Called when chirp is started, can be NULL.
//    :param ch_done_cb_t done_cb: Called when chirp is finished, can be NULL.
//    :param ch_log_cb_t log_cb: Callback to the logging facility, can be NULL.
//
//    :return: A chirp error. See: :c:type:`ch_error_t`.
//    :rtype: ch_error_t
//

// .. c:function::
CH_EXPORT
void
ch_chirp_release_msg_slot(
        ch_chirp_t* rchirp, ch_message_t* msg, ch_release_cb_t release_cb);
//
//    Release the internal message-slot and acknowledge the message if the
//    remote requested an acknowledge-message. Must be called when the message
//    isn't needed anymore, afterwards the message may NOT be used anymore. The
//    message may belong to another chirp instance.
//
//    IMPORTANT: Neglecting to release the slot will lockup chirp. Never ever
//    change a messages identity.
//
//    Before closing chirp you should await release_cb otherwise the remote
//    might not get the requested acknowledge-message.
//
//    :param ch_chirp_t* rchirp: Chirp instances for release_cb
//    :param ch_message_t* msg: The message representing the slot.
//    :param ch_release_cb_t release_cb: Called once the message is released.
//

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_release_msg_slot_ts(
        ch_chirp_t* rchirp, ch_message_t* msg, ch_release_cb_t release_cb);
//
//    Release the internal message-slot and acknowledge the message if the
//    remote requested an acknowledge-message. Must be called when the message
//    isn't needed anymore, afterwards the message may NOT be used anymore. The
//    message may belong to another chirp instance.
//
//    IMPORTANT: Neglecting to release the slot will lockup chirp. Never ever
//    change a messages identity.
//
//    Before closing chirp you should await release_cb otherwise the remote
//    might not get the requested acknowledge-message.
//
//    This function is thread-safe.
//
//    :param ch_chirp_t* rchirp: Chirp instances for release_cb
//    :param ch_message_t* msg: The message representing the slot.
//    :param ch_release_cb_t release_cb: Called once the message is released.
//

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_run(
        const ch_config_t* config,
        ch_chirp_t**       chirp,
        ch_recv_cb_t       recv_cb,
        ch_start_cb_t      start_cb,
        ch_done_cb_t       done_cb,
        ch_log_cb_t        log_cb);
//
//    Initializes, runs and cleans everything. Everything being:
//
//      * chirp object
//      * uv-loop
//      * uv-sockets
//      * callbacks
//
//     The method blocks, but chirp paramenter will be set. Can be used to run
//     chirp in a user defined thread. Use :c:func:`ch_chirp_close_ts` to close
//     chirp in any other thread.
//
//    :param ch_config_t* config: Pointer to a chirp configuration.
//    :param ch_chirp_t** chirp: Out: Pointer to a chirp object pointer. Can be
//                               NULL.
//
//    :param ch_recv_cb_t recv_cb: Called when chirp receives a message,
//                                 can be NULL.
//    :param ch_start_cb_t start_cb: Called when chirp is started, can be NULL.
//    :param ch_done_cb_t done_cb: Called when chirp is finished, can be NULL.
//    :param ch_log_cb_t log_cb: Callback to the logging facility, can be NULL.
//    :return: A chirp error. See: :c:type:`ch_error_t`.
//    :rtype: ch_error_t

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_send(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb);
//
//    Send a message. Messages can be sent concurrently to different nodes.
//    Messages to the same remote node will be queued if you don't wait for the
//    callback.
//
//    If you don't want to allocate messages on sending, we recommend to use a
//    pool of messages.
//
//    Returns CH_SUCCESS when has being sent and CH_QUEUED when the message has
//    been placed in the send queue. CH_USED if the message is already used
//    elsewhere, the message will not be sent.
//
//    The send message queue is not bounded, the user has to pause sending.
//
//    Sending only one message at the same time while having multiple peers
//    would prevent concurrency, but since chirp is quite fast, it is a valid
//    solution.
//
//    A simple pattern is counting the open :c:type:`ch_send_cb_t` and stop
//    sending at a certain threshold.
//
//    The optimal pattern would be sending a message at the time per remote. A
//    remote is defined by the (ip_protocol, address, port) tuple. This is
//    more complex to implement.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//    :param ch_message_t* msg: The message to send. The memory of the message
//                              must stay valid until the callback is called.
//    :param ch_send_cb_t send_cb: The callback, that will be called after
//                                 sending.

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_send_ts(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb);
//
//    Send a message. Messages can be sent in concurrently to different nodes.
//    Messages to the same remote node will be queued if you don't wait for the
//    callback.
//
//    See :c:func:`ch_chirp_send`
//
//    This function is thread-safe. ATTENTION: Callback will be called by the
//    uv-loop-thread.
//
//    Returns CH_SUCCESS when the message has been successfully queue and
//    CH_USED if the message is already used elsewhere, the message will not be
//    sent. This differs from ch_chirp_send, since the message is always
//    queued to be passed to the uv-loop-thread.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//    :param ch_message_t msg: The message to send. The memory of the message
//                             must stay valid until the callback is called.
//    :param ch_send_cb_t send_cb: The callback, that will be called after
//                                 sending.

// .. c:function::
CH_EXPORT
void
ch_chirp_set_always_encrypt(void);
//
//    Also encrypt local connections. This is set globally to all chirp
//    instances.

// .. c:function::
CH_EXPORT
void
ch_chirp_set_auto_stop_loop(ch_chirp_t* chirp);
//
//    Tells chirp to stop the uv-loop when closing (by setting the
//    corresponding flag).
//
//    After this function is called, :c:func:`ch_chirp_close_ts` will also stop
//    the loop.
//
//    This function is thread-safe.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.

// .. c:function::
CH_EXPORT
void
ch_chirp_set_log_callback(ch_chirp_t* chirp, ch_log_cb_t log_cb);
//
//    Set a callback for sending log messages.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//    :param ch_log_cb_t   log: Callback to be called when logging messages.

// .. c:function::
CH_EXPORT
void
ch_chirp_set_public_port(ch_chirp_t* chirp, uint16_t port);
//
//    Set a different public port. Used if your are behind a firewall/NAT that
//    will change public port from the actual port.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//    :param uint16_t port: New public port

// .. c:function::
CH_EXPORT
void
ch_chirp_set_recv_callback(ch_chirp_t* chirp, ch_recv_cb_t recv_cb);
//
//    Set a callback for receiving a message.
//
//    :param ch_chirp_t* chirp: Pointer to a chirp object.
//    :param ch_recv_cb_t recv_cb: Called when chirp receives a message,
//                                 can be NULL.
//
//
//    .. code-block:: cpp

#endif // ch_libchirp_chirp_h
