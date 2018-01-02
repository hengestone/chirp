// ===============
// Protocol Header
// ===============
//
// Handles connections (global) and low-level reading per connection.
//
// .. code-block:: cpp

#ifndef ch_protocol_h
#define ch_protocol_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "connection.h"
#include "libchirp/chirp.h"
#include "rbtree.h"

// Declarations
// ============

// .. c:type:: ch_protocol_t
//
//    Protocol object.
//
//    .. c:member:: struct sockaddr_in addrv4
//
//       BIND_V4 address converted to a sockaddr_in.
//
//    .. c:member:: struct sockaddr_in addrv6
//
//       BIND_V6 address converted to a sockaddr_in6.
//
//    .. c:member:: uv_tcp_t serverv4
//
//       Reference to the libuv tcp server handle, IPv4.
//
//    .. c:member:: uv_tcp_t serverv6
//
//       Reference to the libuv tcp server handle, IPv6.
//
//    .. c:member:: ch_remote_t* remotes
//
//       Pointer to tree of remotes. They can have a connection.
//
//    .. c:member:: ch_remote_t* reconnect_remotes
//
//       A stack of remotes that should be reconnected after a timeout.
//
//    .. c:member:: uv_timer_t reconnect_timeout
//
//       Timeout after which we try to reconnect remotes.
//
//    .. c:member:: ch_connection_t* old_connections
//
//       Pointer to old connections. This is mainly used when there is a
//       network race condition. The then current connections will be replaced
//       and saved as old connections for garbage collection.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Pointer to the chirp object. See: :c:type:`ch_chirp_t`.
//
// .. code-block:: cpp
//
struct ch_protocol_s {
    struct sockaddr_in  addrv4;
    struct sockaddr_in6 addrv6;
    uv_tcp_t            serverv4;
    uv_tcp_t            serverv6;
    ch_remote_t*        remotes;
    ch_remote_t*        reconnect_remotes;
    uv_timer_t          reconnect_timeout;
    ch_connection_t*    old_connections;
    ch_connection_t*    handshake_conns;
    ch_chirp_t*         chirp;
};

// .. c:function::
ch_error_t
ch_pr_conn_start(
        ch_chirp_t* chirp, ch_connection_t* conn, uv_tcp_t* client, int accept);
//
//    Start the given connection
//
//    :param ch_chirp_t* chirp: Chirp object
//    :param ch_connection_t* conn: Connection object
//    :param uv_tcp_t* client: Client to start
//    :param int accept: Is accepted connection
//
//    :return: Void since only called from callbacks.

// .. c:function::
void
ch_pr_close_free_remotes(ch_chirp_t* chirp, int only_conns);
//
//    Close and free all remaining connections.
//
//    :param ch_chirpt_t* chirp: Chrip object
//    :paramint only_conns: (bool) Only close / free connections. Do not touch
//                          remotes
//

// .. c:function::
void
ch_pr_debounce_connection(ch_connection_t* conn);
//
//    Disallow reconnect for 50 - 550ms, in order to prevent network races and
//    give the remote time to recover.
//
//    :param ch_connection_t* conn: Connection object
//

// .. c:function::
void
ch_pr_decrypt_read(ch_connection_t* conn, int* stop);
//
//    Reads data over SSL on the given connection. Returns 1 if something was
//    read, 0 otherwise.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//    :param int* stop:             (Out) Stop the reading process.

// .. c:function::
void
ch_pr_reconnect_remotes_cb(uv_timer_t* handle);
//
//    Reconnects remotes that had connect failure after a timeout
//
//    :param uv_timer_t* handle: uv timer handle, data contains chirp

// .. c:function::
void
ch_pr_restart_stream(ch_connection_t* conn);
//
//    Try to restart the current stream on this connection.
//
//    :param ch_connection_t* conn: Connection to restart.

// .. c:function::
ch_error_t
ch_pr_start(ch_protocol_t* protocol);
//
//    Start the given protocol.
//
//    :param ch_protocol_t* protocol: Protocol which shall be started..
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:function::
ch_error_t
ch_pr_stop(ch_protocol_t* protocol);
//
//    Stop the given protocol.
//
//    :param ch_protocol_t* protocol: Protocol which shall be stopped.
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:function::
void
ch_pr_init(ch_chirp_t* chirp, ch_protocol_t* protocol);
//
//    Initialize the protocol structure.
//
//    :param ch_chirp_t* chirp: Chirp instance.
//    :param ch_protocol_t* protocol: Protocol to initialize.
//
// .. code-block:: cpp
//
#endif // ch_protocol_h
