// ========
// Protocol
// ========
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "protocol.h"
#include "chirp.h"
#include "common.h"
#include "remote.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <openssl/err.h>

// Declarations
// ============

// .. c:function::
static void
_ch_pr_abort_all_messages(ch_remote_t* remote, ch_error_t error);
//
//    Abort all messages in queue, because we are closing down.
//
//    :param ch_remote_t* remote: Remote failed to connect.
//    :param ch_error_t error: Status returned by connect.

// .. c:function::
static int
_ch_pr_decrypt_feed(ch_connection_t* conn, ch_buf* buf, size_t read, int* stop);
//
//    Feeds data into the SSL BIO.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//    :param ch_buf* buf:           The buffer containing ``read`` bytes read.
//    :param size_t read:           The number of bytes read.
//    :param int* stop:             (Out) Stop the reading process.

// .. c:function::
static void
_ch_pr_do_handshake(ch_connection_t* conn);
//
//    Do a handshake on the given connection.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//
// .. c:function::
static void
_ch_pr_gc_connections_cb(uv_timer_t* handle);
//
//    Called to cleanup connections/remotes that aren't use for REUSE_TIME+.
//
//    :param uv_timer_t* handle: uv timer handle, data contains chirp

// .. c:function::
static void
_ch_pr_new_connection_cb(uv_stream_t* server, int status);
//
//    Callback from libuv when a stream server has received an incoming
//    connection.
//
//    :param uv_stream_t* server: Pointer to the stream handle (duplex
//                                communication channel) of the server,
//                                containig a chirp object.

// .. c:function::
static void
_ch_pr_read_data_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
//
//    Callback called from libuv when data was read on a stream.
//    Reads nread bytes on either an encrypted or an unencrypted connection
//    coming from the given stream handle.
//
//    :param uv_stream_t* stream: Pointer to the stream that data was read on.
//    :param ssize_t nread: Number of bytes that were read on the stream.
//    :param uv_buf_t* buf: Pointer to a libuv (data-) buffer. When nread < 0,
//                          the buf parameter might not point to a valid
//                          buffer; in that case buf.len and buf.base are both
//                          set to 0.

// .. c:function::
static int
_ch_pr_read_resume(ch_connection_t* conn, ch_resume_state_t* resume);
//
//    Resumes the ch_rd_read based on a given resume state. If the connection
//    is not encrypted we use conn->read_resume, else conn->tls_resume.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//    :param ch_resume_state_t* resume: Pointer to a resume state.

// .. c:function::
static int
_ch_pr_resume(ch_connection_t* conn);
//
//    Resume partial read when the connection was stopped because the last
//    buffer was used. Returns 1 if it ok to restart the reader.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.

// .. c:function::
static inline ch_error_t
_ch_pr_start_socket(
        ch_chirp_t*      chirp,
        int              af,
        uv_tcp_t*        server,
        uint8_t*         bind,
        struct sockaddr* addr,
        int              v6only);
//
//    Since dual stack sockets don't work on all platforms we start a IPv4 and
//    IPv6 socket.

// .. c:function::
static void
_ch_pr_update_resume(
        ch_resume_state_t* resume,
        ch_buf*            buf,
        size_t             nread,
        ssize_t            bytes_handled);
//
//    Update the resume state. Checks if reading was partial and sets resume
//    state that points to the remaining data. If the last buffer was used, it
//    is possible that all that has been read and we can just stop or that
//    there is still a message in the buffer.
//
//    :param ch_resume_state_t* resume: Pointer to resume state.
//    :param ch_buf* buf: Pointer to buffer being checked.
//    :param size_t nread: Total bytes available
//    :param ssize_t bytes_handled: Bytes actually handled

// Definitions
// ===========

// .. c:function::
static void
_ch_pr_abort_all_messages(ch_remote_t* remote, ch_error_t error)
//    :noindex:
//
//    see: :c:func:`_ch_pr_abort_all_messages`
//
// .. code-block:: cpp
//
{
    /* The remote is going away, we need to abort all messages */
    ch_message_t* msg;
    ch_msg_dequeue(&remote->msg_queue, &msg);
    while (msg != NULL) {
        ch_send_cb_t cb = msg->_send_cb;
        if (cb != NULL) {
            msg->_send_cb = NULL;
            cb(remote->chirp, msg, error);
        }
        ch_msg_dequeue(&remote->msg_queue, &msg);
    }
    remote->cntl_msg_queue = NULL;
}

// .. c:function::
static void
_ch_pr_do_handshake(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`_ch_pr_do_handshake`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp         = conn->chirp;
    conn->tls_handshake_state = SSL_do_handshake(conn->ssl);
    if (SSL_is_init_finished(conn->ssl)) {
        conn->flags &= ~CH_CN_TLS_HANDSHAKE;
        if (conn->tls_handshake_state) {
            LC(chirp,
               "SSL handshake successful. ",
               "ch_connection_t:%p",
               (void*) conn);
        } else {
#ifdef CH_ENABLE_LOGGING
            ERR_print_errors_fp(stderr);
#endif
            EC(chirp,
               "SSL handshake failed. ",
               "ch_connection_t:%p",
               (void*) conn);
            ch_cn_shutdown(conn, CH_TLS_ERROR);
            return;
        }
    }
    ch_cn_send_if_pending(conn);
}
// .. c:function::
static void
_ch_pr_gc_connections_cb(uv_timer_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_pr_gc_connections_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t*  ichirp   = chirp->_;
    ch_protocol_t*   protocol = &ichirp->protocol;
    ch_config_t*     config   = &ichirp->config;
    uint64_t         now      = uv_hrtime();
    uint64_t         then     = now - (1000 * 1000 * 1000 * config->REUSE_TIME);
    ch_remote_t*     rm_del_stack = NULL;
    ch_connection_t* cn_del_stack = NULL;

    L(chirp, "Garbage-collecting connections and remotes", CH_NO_ARG);
    rb_iter_decl_cx_m(ch_cn, cn_iter, cn_elem);
    rb_for_m (ch_cn, protocol->old_connections, cn_iter, cn_elem) {
        if (cn_elem->timestamp < then) {
            ch_cn_st_push(&cn_del_stack, cn_elem);
        }
    }
    rb_for_m (ch_cn_st, cn_del_stack, cn_iter, cn_elem) {
        L(chirp, "Garbage-collecting: shutdown. ch_connection_t:%p", cn_elem);
        ch_cn_shutdown(cn_elem, CH_SHUTDOWN);
    }

    rb_iter_decl_cx_m(ch_rm, rm_iter, rm_elem);
    rb_for_m (ch_rm, protocol->remotes, rm_iter, rm_elem) {
        if (!(rm_elem->flags & CH_RM_CONN_BLOCKED) &&
            rm_elem->timestamp < then) {
            A(rm_elem->next == NULL, "Should not be in reconnect_remotes");
            ch_rm_st_push(&rm_del_stack, rm_elem);
        }
    }
    ch_remote_t* free_it = NULL; /* Free remote after iterator */
    rb_for_m (ch_rm_st, rm_del_stack, rm_iter, rm_elem) {
        _ch_pr_abort_all_messages(rm_elem, CH_SHUTDOWN);
        if (rm_elem->conn != NULL) {
            L(chirp,
              "Garbage-collecting: shutdown. ch_connection_t:%p",
              rm_elem->conn);
            rm_elem->flags = CH_RM_CONN_BLOCKED;
            ch_cn_shutdown(rm_elem->conn, CH_SHUTDOWN);
        }
        L(chirp, "Garbage-collecting: deleting. ch_remote_t:%p", rm_elem);
        ch_rm_delete_node(&protocol->remotes, rm_elem);
        if (free_it != NULL) {
            ch_rm_free(free_it);
        }
        free_it = rm_elem;
    }
    if (free_it != NULL) {
        ch_rm_free(free_it);
    }
    uint64_t start = (config->REUSE_TIME * 1000 / 2);
    start += rand() % start;
    uv_timer_start(&protocol->gc_timeout, _ch_pr_gc_connections_cb, start, 0);
}

// .. c:function::
static void
_ch_pr_new_connection_cb(uv_stream_t* server, int status)
//    :noindex:
//
//    see: :c:func:`_ch_pr_new_connection_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = server->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    /* Do not accept new connections when chirp is closing */
    if (ichirp->flags & CH_CHIRP_CLOSING) {
        return;
    }
    ch_protocol_t* protocol = &ichirp->protocol;
    if (status < 0) {
        L(chirp, "New connection error %s", uv_strerror(status));
        return;
    }

    ch_connection_t* conn = (ch_connection_t*) ch_alloc(sizeof(*conn));
    if (!conn) {
        E(chirp, "Could not allocate memory for connection", CH_NO_ARG);
        return;
    }
    LC(chirp, "Accepted connection. ", "ch_connection_t:%p", (void*) conn);
    memset(conn, 0, sizeof(*conn));
    ch_cn_node_init(conn);
    ch_cn_insert(&protocol->handshake_conns, conn);
    conn->chirp       = chirp;
    conn->client.data = conn;
    uv_tcp_t* client  = &conn->client;
    uv_tcp_init(server->loop, client);
    conn->flags |= CH_CN_INIT_CLIENT | CH_CN_INCOMING;

    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        struct sockaddr_storage addr;
        int                     addr_len = sizeof(addr);
        ch_text_address_t       taddr;
        if (uv_tcp_getpeername(
                    &conn->client, (struct sockaddr*) &addr, &addr_len) !=
            CH_SUCCESS) {
            EC(chirp,
               "Could not get remote address. ",
               "ch_connection_t:%p",
               (void*) conn);
            ch_cn_shutdown(conn, CH_FATAL);
            return;
        };
        conn->ip_protocol = addr.ss_family;
        if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6* saddr = (struct sockaddr_in6*) &addr;
            memcpy(&conn->address, &saddr->sin6_addr, sizeof(saddr->sin6_addr));
            uv_ip6_name(saddr, taddr.data, sizeof(taddr.data));
        } else {
            struct sockaddr_in* saddr = (struct sockaddr_in*) &addr;
            memcpy(&conn->address, &saddr->sin_addr, sizeof(saddr->sin_addr));
            uv_ip4_name(saddr, taddr.data, sizeof(taddr.data));
        }
        if (!(ichirp->config.DISABLE_ENCRYPTION || ch_is_local_addr(&taddr))) {
            conn->flags |= CH_CN_ENCRYPTED;
        }
        ch_pr_conn_start(chirp, conn, client, 1);
    } else {
        ch_cn_shutdown(conn, CH_FATAL);
    }
}

// .. c:function::
static int
_ch_pr_read_resume(ch_connection_t* conn, ch_resume_state_t* resume)
//    :noindex:
//
//    see: :c:func:`_ch_pr_read_resume`
//
// .. code-block:: cpp
//
{
    ch_buf* buf            = resume->rest_of_buffer;
    size_t  nread          = resume->bytes_to_read;
    resume->rest_of_buffer = NULL;
    resume->bytes_to_read  = 0;
    int     stop;
    ssize_t bytes_handled = ch_rd_read(conn, buf, nread, &stop);
    A(resume->bytes_to_read ? resume->rest_of_buffer != NULL : 1,
      "No buffer set");
    if (stop) {
        _ch_pr_update_resume(resume, buf, nread, bytes_handled);
    }
    return !stop;
}

// .. c:function::
static inline ch_error_t
_ch_pr_start_socket(
        ch_chirp_t*      chirp,
        int              af,
        uv_tcp_t*        server,
        uint8_t*         bind,
        struct sockaddr* addr,
        int              v6only)
//    :noindex:
//
//    see: :c:func:`_ch_pr_start_socket`
//
// .. code-block:: cpp
//
{
    ch_text_address_t tmp_addr;
    int               tmp_err;
    ch_chirp_int_t*   ichirp = chirp->_;
    ch_config_t*      config = &ichirp->config;
    uv_tcp_init(ichirp->loop, server);
    server->data = chirp;

    tmp_err = uv_inet_ntop(af, bind, tmp_addr.data, sizeof(tmp_addr.data));
    if (tmp_err != CH_SUCCESS) {
        return CH_VALUE_ERROR;
    }

    tmp_err = ch_textaddr_to_sockaddr(
            af, &tmp_addr, config->PORT, (struct sockaddr_storage*) addr);
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }

    tmp_err = uv_tcp_bind(server, addr, v6only);
    if (tmp_err != CH_SUCCESS) {
        fprintf(stderr,
                "%s:%d Fatal: cannot bind port (IPv%d:%d)\n",
                __FILE__,
                __LINE__,
                af == AF_INET6 ? 6 : 4,
                config->PORT);
        return CH_EADDRINUSE;
    }

    tmp_err = uv_tcp_nodelay(server, 1);
    if (tmp_err != CH_SUCCESS) {
        return CH_UV_ERROR;
    }

    tmp_err = uv_listen(
            (uv_stream_t*) server, config->BACKLOG, _ch_pr_new_connection_cb);
    if (tmp_err != CH_SUCCESS) {
        fprintf(stderr,
                "%s:%d Fatal: cannot listen port (IPv%d:%d)\n",
                __FILE__,
                __LINE__,
                af == AF_INET6 ? 6 : 4,
                config->PORT);
        return CH_EADDRINUSE;
    }
    return CH_SUCCESS;
}

// .. c:function::
static void
_ch_pr_update_resume(
        ch_resume_state_t* resume,
        ch_buf*            buf,
        size_t             nread,
        ssize_t            bytes_handled)
//    :noindex:
//
//    see: :c:func:`_ch_pr_update_resume`
//
// .. code-block:: cpp
//
{
    if (bytes_handled != -1 && bytes_handled != (ssize_t) nread) {
        A(resume->rest_of_buffer == NULL || resume->bytes_to_read != 0,
          "Last partial read not completed");
        resume->rest_of_buffer = buf + bytes_handled;
        resume->bytes_to_read  = nread - bytes_handled;
    }
}

// .. c:function::
ch_error_t
ch_pr_conn_start(
        ch_chirp_t* chirp, ch_connection_t* conn, uv_tcp_t* client, int accept)
//    :noindex:
//
//    see: :c:func:`ch_pr_conn_start`
//
// .. code-block:: cpp
//
{
    int tmp_err = ch_cn_init(chirp, conn, conn->flags);
    if (tmp_err != CH_SUCCESS) {
        E(chirp, "Could not initialize connection (%d)", tmp_err);
        ch_cn_shutdown(conn, tmp_err);
        return tmp_err;
    }
    tmp_err = uv_tcp_nodelay(client, 1);
    if (tmp_err != CH_SUCCESS) {
        E(chirp, "Could not set tcp nodelay on connection (%d)", tmp_err);
        ch_cn_shutdown(conn, CH_UV_ERROR);
        return CH_UV_ERROR;
    }

    tmp_err = uv_tcp_keepalive(client, 1, CH_TCP_KEEPALIVE);
    if (tmp_err != CH_SUCCESS) {
        E(chirp, "Could not set tcp keepalive on connection (%d)", tmp_err);
        ch_cn_shutdown(conn, CH_UV_ERROR);
        return CH_UV_ERROR;
    }

    uv_read_start(
            (uv_stream_t*) client, ch_cn_read_alloc_cb, _ch_pr_read_data_cb);
    if (conn->flags & CH_CN_ENCRYPTED) {
        if (accept) {
            SSL_set_accept_state(conn->ssl);
        } else {
            SSL_set_connect_state(conn->ssl);
            _ch_pr_do_handshake(conn);
        }
        conn->flags |= CH_CN_TLS_HANDSHAKE;
    } else {
        int stop;
        ch_rd_read(conn, NULL, 0, &stop); /* Start reader */
    }
    return CH_SUCCESS;
}

// .. c:function::
void
ch_pr_close_free_remotes(ch_chirp_t* chirp, int only_conns)
//    :noindex:
//
//    see: :c:func:`ch_pr_close_free_remotes`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp   = chirp->_;
    ch_protocol_t*  protocol = &ichirp->protocol;
    if (only_conns) {
        rb_iter_decl_cx_m(ch_rm, rm_iter, rm_elem);
        rb_for_m (ch_rm, protocol->remotes, rm_iter, rm_elem) {
            if (rm_elem->conn != NULL) {
                ch_cn_shutdown(rm_elem->conn, CH_SHUTDOWN);
                ch_wr_process_queues(rm_elem);
            }
        }
    } else {
        while (protocol->remotes != ch_rm_nil_ptr) {
            ch_remote_t* remote = protocol->remotes;
            /* Leaves the queue empty and there prevents reconnects. */
            _ch_pr_abort_all_messages(remote, CH_SHUTDOWN);
            if (remote->conn != NULL) {
                ch_cn_shutdown(remote->conn, CH_SHUTDOWN);
            }
            ch_rm_delete_node(&protocol->remotes, remote);
            ch_rm_free(remote);
        }
        /* Remove all remotes, sync with reconnect_remotes */
        protocol->reconnect_remotes = NULL;
    }
    while (protocol->old_connections != ch_cn_nil_ptr) {
        ch_cn_shutdown(protocol->old_connections, CH_SHUTDOWN);
    }
    while (protocol->handshake_conns != ch_cn_nil_ptr) {
        ch_cn_shutdown(protocol->handshake_conns, CH_SHUTDOWN);
    }
}

// .. c:function::
void
ch_pr_debounce_connection(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_pr_conn_start`
//
// .. code-block:: cpp
//
{
    ch_remote_t     key;
    ch_chirp_t*     chirp    = conn->chirp;
    ch_chirp_int_t* ichirp   = chirp->_;
    ch_remote_t*    remote   = NULL;
    ch_protocol_t*  protocol = &ichirp->protocol;
    ch_rm_init_from_conn(chirp, &key, conn, 1);
    if (ch_rm_find(protocol->remotes, &key, &remote) == CH_SUCCESS) {
        if (protocol->reconnect_remotes == NULL) {
            uv_timer_start(
                    &protocol->reconnect_timeout,
                    ch_pr_reconnect_remotes_cb,
                    50 + (rand() % 500),
                    0);
        }
        if (!(remote->flags & CH_RM_CONN_BLOCKED)) {
            remote->flags |= CH_RM_CONN_BLOCKED;
            ch_rm_st_push(&protocol->reconnect_remotes, remote);
        }
    }
}

// .. c:function::
static int
_ch_pr_decrypt_feed(ch_connection_t* conn, ch_buf* buf, size_t nread, int* stop)
//    :noindex:
//
//    see: :c:func:`ch_pr_decrypt_feed`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp         = conn->chirp;
    size_t      bytes_handled = 0;
    *stop                     = 0;
    do {
        if (nread > 0) {
            ssize_t tmp_err = 0;
            tmp_err         = BIO_write(
                    conn->bio_app, buf + bytes_handled, nread - bytes_handled);
            if (tmp_err < 1) {
                if (!(conn->flags & CH_CN_STOPPED)) {
                    EC(chirp,
                       "SSL error writing to BIO, shutting down connection. ",
                       "ch_connection_t:%p",
                       (void*) conn);
                    ch_cn_shutdown(conn, CH_TLS_ERROR);
                    return -1;
                }
            } else {
                bytes_handled += tmp_err;
            }
        }
        if (conn->flags & CH_CN_TLS_HANDSHAKE) {
            _ch_pr_do_handshake(conn);
        } else {
            ch_pr_decrypt_read(conn, stop);
            if (*stop) {
                return bytes_handled;
            }
        }
    } while (bytes_handled < nread);
    return bytes_handled;
}

// .. c:function::
void
ch_pr_decrypt_read(ch_connection_t* conn, int* stop)
//    :noindex:
//
//    see: :c:func:`ch_pr_decrypt_read`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    ssize_t     tmp_err;
    *stop = 0;
    while ((tmp_err = SSL_read(
                    conn->ssl, conn->buffer_rtls, conn->buffer_rtls_size)) >
           0) {
        ;
        LC(chirp,
           "Read %d bytes. (unenc). ",
           "ch_connection_t:%p",
           tmp_err,
           (void*) conn);
        ssize_t bytes_handled =
                ch_rd_read(conn, conn->buffer_rtls, tmp_err, stop);
        if (*stop) {
            _ch_pr_update_resume(
                    &conn->tls_resume,
                    conn->buffer_rtls,
                    tmp_err,
                    bytes_handled);
            return;
        }
    }
    tmp_err = SSL_get_error(conn->ssl, tmp_err);
    if (tmp_err != SSL_ERROR_WANT_READ) {
        if (tmp_err < 0) {
#ifdef CH_ENABLE_LOGGING
            ERR_print_errors_fp(stderr);
#endif
            EC(chirp,
               "SSL operation fatal error. ",
               "ch_connection_t:%p",
               (void*) conn);
        } else {
            LC(chirp,
               "SSL operation failed. ",
               "ch_connection_t:%p",
               (void*) conn);
        }
        ch_cn_shutdown(conn, CH_TLS_ERROR);
    }
}

// .. c:function::
void
ch_pr_init(ch_chirp_t* chirp, ch_protocol_t* protocol)
//    :noindex:
//
//    see: :c:func:`ch_pr_init`
//
// .. code-block:: cpp
//
{
    memset(protocol, 0, sizeof(*protocol));
    protocol->chirp = chirp;
    ch_cn_tree_init(&protocol->handshake_conns);
    ch_cn_tree_init(&protocol->old_connections);
    ch_rm_tree_init(&protocol->remotes);
    protocol->reconnect_remotes = NULL;
}

// .. c:function::
void
ch_pr_reconnect_remotes_cb(uv_timer_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_pr_reconnect_remotes_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*    chirp    = handle->data;
    ch_protocol_t* protocol = &chirp->_->protocol;
    ch_chirp_check_m(chirp);
    if (protocol->reconnect_remotes != NULL) {
        int count = 0;
        rb_iter_decl_cx_m(ch_rm_st, rm_iter, rm_elem);
        rb_for_m (ch_rm_st, protocol->reconnect_remotes, rm_iter, rm_elem) {
            count += 1;
        }
        ch_remote_t** remotes = ch_alloc(count * sizeof(remotes));
        ch_remote_t*  remote;
        /* ch_wr_process_queues can add remotes to protocol->reconnect_remotes
         * so we need to copy it first */
        int idx = 0;
        ch_rm_st_pop(&protocol->reconnect_remotes, &remote);
        while (remote != NULL) {
            remote->flags &= ~CH_RM_CONN_BLOCKED;
            remotes[idx] = remote;
            idx += 1;
            ch_rm_st_pop(&protocol->reconnect_remotes, &remote);
        }
        A(idx == count, "Index error while copying remotes");
        for (idx = 0; idx < count; idx++) {
            ch_wr_process_queues(remotes[idx]);
        }
        ch_free(remotes);
    }
}

// .. c:function::
void
ch_pr_restart_stream(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_pr_resume`
//
// .. code-block:: cpp
//
{
    if (conn != NULL) {
        LC(conn->chirp, "Resume reading", "ch_connection_t:%p", conn);
        if (_ch_pr_resume(conn)) {
            if (conn->flags & CH_CN_STOPPED) {
                conn->flags &= ~CH_CN_STOPPED;
                LC(conn->chirp, "Restart stream", "ch_connection_t:%p", conn);
                uv_read_start(
                        (uv_stream_t*) &conn->client,
                        ch_cn_read_alloc_cb,
                        _ch_pr_read_data_cb);
            }
        }
    }
}

// .. c:function::
static int
_ch_pr_resume(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_pr_resume`
//
// .. code-block:: cpp
//
{
    if (conn->flags & CH_CN_ENCRYPTED) {
        int stop;
        int ret = _ch_pr_read_resume(conn, &conn->tls_resume);
        if (!ret) {
            return ret;
        }
        ch_resume_state_t* resume = &conn->read_resume;

        ssize_t bytes_handled;
        ch_buf* buf            = resume->rest_of_buffer;
        size_t  nread          = resume->bytes_to_read;
        resume->rest_of_buffer = NULL;
        resume->bytes_to_read  = 0;

        bytes_handled = _ch_pr_decrypt_feed(conn, buf, nread, &stop);
        if (stop) {
            _ch_pr_update_resume(resume, buf, nread, bytes_handled);
        }
        return !stop;
    } else {
        return _ch_pr_read_resume(conn, &conn->read_resume);
    }
}

// .. c:function::
static void
_ch_pr_read_data_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
//    :noindex:
//
//    see: :c:func:`ch_pr_read_data_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = stream->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    /* Ignore reads while shutting down */
    if (conn->flags & CH_CN_SHUTTING_DOWN) {
        return;
    }
    ssize_t bytes_handled = 0;
#ifdef CH_ENABLE_ASSERTS
    conn->flags &= ~CH_CN_BUF_UV_USED;
#endif
    if (nread == UV_EOF) {
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return;
    }

    if (nread == 0) {
        LC(chirp,
           "Unexpected emtpy read (%d) from libuv. ",
           "ch_connection_t:%p",
           (int) nread,
           (void*) conn);
        return;
    }
    if (nread < 0) {
        LC(chirp,
           "Reader got error %d -> shutdown. ",
           "ch_connection_t:%p",
           (int) nread,
           (void*) conn);
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return;
    }
    LC(chirp,
       "%d available bytes.",
       "ch_connection_t:%p",
       (int) nread,
       (void*) conn);
    int stop;
    if (conn->flags & CH_CN_ENCRYPTED) {
        bytes_handled = _ch_pr_decrypt_feed(conn, buf->base, nread, &stop);
    } else {
        bytes_handled = ch_rd_read(conn, buf->base, nread, &stop);
    }
    if (stop) {
        _ch_pr_update_resume(
                &conn->read_resume, buf->base, nread, bytes_handled);
    }
}

// .. c:function::
ch_error_t
ch_pr_start(ch_protocol_t* protocol)
//    :noindex:
//
//    see: :c:func:`ch_pr_start`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = protocol->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
    ch_config_t*    config = &ichirp->config;
    int             tmp_err;
    tmp_err = _ch_pr_start_socket(
            chirp,
            AF_INET,
            &protocol->serverv4,
            config->BIND_V4,
            (struct sockaddr*) &protocol->addrv4,
            0);
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }
    tmp_err = _ch_pr_start_socket(
            chirp,
            AF_INET6,
            &protocol->serverv6,
            config->BIND_V6,
            (struct sockaddr*) &protocol->addrv6,
            UV_TCP_IPV6ONLY);
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }
    tmp_err = uv_timer_init(ichirp->loop, &protocol->reconnect_timeout);
    if (tmp_err != CH_SUCCESS) {
        return CH_INIT_FAIL;
    }
    protocol->reconnect_timeout.data = chirp;
    tmp_err = uv_timer_init(ichirp->loop, &protocol->gc_timeout);
    if (tmp_err != CH_SUCCESS) {
        return CH_INIT_FAIL;
    }
    protocol->gc_timeout.data = chirp;

    uint64_t start = (config->REUSE_TIME * 1000 / 2);
    start += rand() % start;
    uv_timer_start(&protocol->gc_timeout, _ch_pr_gc_connections_cb, start, 0);
    return CH_SUCCESS;
}

// .. c:function::
ch_error_t
ch_pr_stop(ch_protocol_t* protocol)
//    :noindex:
//
//    see: :c:func:`ch_pr_stop`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = protocol->chirp;
    L(chirp, "Closing protocol", CH_NO_ARG);
    ch_pr_close_free_remotes(chirp, 0);
    uv_close((uv_handle_t*) &protocol->serverv4, ch_chirp_close_cb);
    uv_close((uv_handle_t*) &protocol->serverv6, ch_chirp_close_cb);
    uv_timer_stop(&protocol->reconnect_timeout);
    uv_close((uv_handle_t*) &protocol->reconnect_timeout, ch_chirp_close_cb);
    uv_timer_stop(&protocol->gc_timeout);
    uv_close((uv_handle_t*) &protocol->gc_timeout, ch_chirp_close_cb);
    chirp->_->closing_tasks += 4;
    return CH_SUCCESS;
}
