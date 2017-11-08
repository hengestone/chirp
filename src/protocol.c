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
#include "util.h"
#include "remote.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <openssl/err.h>


// Declarations
// ============

// .. c:function::
static
void
_ch_pr_update_resume(
        ch_resume_state_t* resume,
        ch_buf* buf,
        int nread,
        int bytes_handled
);
//
//    Update the resume state. Checks if reading was partial and sets resume
//    state that points to the remaining data. If the last buffer was used, it
//    is possible that all that has been read and we can just stop or that
//    there is still a message in the buffer.
//
//    :param ch_resume_state_t* resume: Pointer to resume state.
//    :param ch_buf* buf: Pointer to buffer being checked.
//    :param int nread: Total bytes available
//    :param int bytes_handled: Bytes actually handled

// .. c:function::
static
void
_ch_pr_close_free_connections(ch_chirp_t* chirp);
//
//    Close and free all remaining connections.
//
//    :param ch_chirpt_t* chirp: Chrip object

// .. c:function::
static
int
_ch_pr_decrypt_feed(ch_connection_t* conn, ch_buf* buf, size_t read, int* stop);
//
//    Feeds data into the SSL BIO.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//    :param ch_buf* buf:           The buffer containing ``read`` bytes read.
//    :param size_t read:           The number of bytes read.
//    :param int* stop:             (Out) Stop the reading process.

// .. c:function::
static
void
_ch_pr_do_handshake(ch_connection_t* conn);
//
//    Do a handshake on the given connection.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.

// .. c:function::
static
void
_ch_pr_new_connection_cb(uv_stream_t* server, int status);
//
//    Callback from libuv when a stream server has received an incoming
//    connection.
//
//    :param uv_stream_t* server: Pointer to the stream handle (duplex
//                                communication channel) of the server,
//                                containig a chirp object.

// .. c:function::
static
void
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
static
int
_ch_pr_read_resume(ch_connection_t* conn, ch_resume_state_t* resume);
//
//    Resumes the ch_rd_read based on a given resume state. If the connection
//    is not encrypted we use conn->read_resume, else conn->tls_resume.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.
//    :param ch_resume_state_t* resume: Pointer to a resume state.

// .. c:function::
static
int
_ch_pr_resume(ch_connection_t* conn);
//
//    Resume partial read when the connection was stopped because the last
//    buffer was used. Returns 1 if it ok to restart the reader.
//
//    :param ch_connection_t* conn: Pointer to a connection handle.

// Definitions
// ===========

// .. c:function::
static
void
_ch_pr_update_resume(
        ch_resume_state_t* resume,
        ch_buf* buf,
        int nread,
        int bytes_handled
)
//    :noindex:
//
//    see: :c:func:`_ch_pr_update_resume`
//
// .. code-block:: cpp
//
{
    if(bytes_handled != -1 && bytes_handled != nread) {
        A(
            resume->rest_of_buffer == NULL || resume->bytes_to_read != 0,
            "Last partial read not completed"
        );
        resume->rest_of_buffer = buf + bytes_handled;
        resume->bytes_to_read = nread - bytes_handled;
    }
}

// .. c:function::
static
void
_ch_pr_close_free_connections(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`_ch_pr_close_free_connections`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp = chirp->_;
    ch_protocol_t* protocol = &ichirp->protocol;
    /* We may not change the data-structure during iteration */
    while(protocol->remotes != ch_rm_nil_ptr) {
        ch_remote_t* remote = protocol->remotes;
        if(remote->conn != NULL)
            ch_cn_shutdown(remote->conn, CH_SHUTDOWN);
        ch_rm_delete_node(&protocol->remotes, remote);
        ch_free(remote);
    }
    rb_iter_decl_cx_m(ch_cn_old, old_iter, old_elem);
    rb_for_m(ch_cn_old, protocol->old_connections, old_iter, old_elem) {
        ch_cn_shutdown(old_elem, CH_SHUTDOWN);
    }
    /* Effectively we have cleared the list */
    protocol->old_connections = NULL;
    rb_iter_decl_cx_m(ch_cn, hs_iter, hs_elem);
    rb_for_m(ch_cn, protocol->handshake_conns, hs_iter, hs_elem) {
        ch_cn_shutdown(hs_elem, CH_SHUTDOWN);
    }
    /* Effectively we have cleared the list */
    protocol->handshake_conns = NULL;
}

// .. c:function::
static
void
_ch_pr_do_handshake(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`_ch_pr_do_handshake`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    conn->tls_handshake_state = SSL_do_handshake(conn->ssl);
    if(SSL_is_init_finished(conn->ssl)) {
        conn->flags &= ~CH_CN_TLS_HANDSHAKE;
        /* Last handshake state, since we got that on the last read and have to
         * use it on this read. */
        if(conn->tls_handshake_state) {
            LC(
                chirp,
                "SSL handshake successful. ", "ch_connection_t:%p",
                (void*) conn
            );
        } else {
#           ifndef NDEBUG
                ERR_print_errors_fp(stderr);
#           endif
            EC(
                chirp,
                "SSL handshake failed. ", "ch_connection_t:%p",
                (void*) conn
            );
            ch_cn_shutdown(conn, CH_TLS_ERROR);
            return;
        }
    }
    ch_cn_send_if_pending(conn);
}

// .. c:function::
static
void
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
    ch_chirp_int_t* ichirp  = chirp->_;
    ch_protocol_t* protocol = &ichirp->protocol;
    if (status < 0) {
        L(
            chirp,
            "New connection error %s",
            uv_strerror(status)
        );
        return;
    }

    ch_connection_t* conn = (ch_connection_t*) ch_alloc(sizeof(*conn));
    if(!conn) {
        E(
            chirp,
            "Could not allocate memory for connection",
            CH_NO_ARG
        );
        return;
    }
    LC(
        chirp,
        "Accepted connection. ", "ch_connection_t:%p",
        (void*) conn
    );
    memset(conn, 0, sizeof(*conn));
    ch_cn_node_init(conn);
    ch_cn_insert(&protocol->handshake_conns, conn);
    conn->chirp = chirp;
    conn->client.data = conn;
    uv_tcp_t* client = &conn->client;
    uv_tcp_init(server->loop, client);
    conn->flags |= CH_CN_INIT_CLIENT | CH_CN_INCOMING;

#   begindef ch_pr_parse_ip_addr_m(ip_version, inet_version, in_version)
    {
        struct sockaddr_##in_version* saddr =
            (struct sockaddr_##in_version*) &addr;
        conn->ip_protocol = AF_##inet_version;
        memcpy(
            &conn->address,
            &saddr->s##in_version##_addr,
            sizeof(saddr->s##in_version##_addr)
        );
        uv_ip##ip_version##_name(saddr, taddr.data, sizeof(taddr.data));
    }
#   enddef

    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        struct sockaddr_storage addr;
        int addr_len = sizeof(addr);
        ch_text_address_t taddr;
        if(uv_tcp_getpeername(
                    &conn->client,
                    (struct sockaddr*) &addr,
                    &addr_len
        ) != CH_SUCCESS) {
            EC(
                chirp,
                "Could not get remote address. ", "ch_connection_t:%p",
                (void*) conn
            );
            ch_cn_shutdown(conn, CH_FATAL);
            return;
        };
        if(addr.ss_family == AF_INET6)
            ch_pr_parse_ip_addr_m(6, INET6, in6)
        else
            ch_pr_parse_ip_addr_m(4, INET, in)
        if(!(
                ichirp->config.DISABLE_ENCRYPTION  ||
                ch_is_local_addr(&taddr)
        )) {
            conn->flags |= CH_CN_ENCRYPTED;
        }
        ch_pr_conn_start(chirp, conn, client, 1);
    }
    else
        ch_cn_shutdown(conn, CH_FATAL);
}

// .. c:function::
static
int
_ch_pr_read_resume(ch_connection_t* conn, ch_resume_state_t* resume)
//    :noindex:
//
//    see: :c:func:`_ch_pr_read_resume`
//
// .. code-block:: cpp
//
{
    ch_buf* buf = resume->rest_of_buffer;
    int nread = resume->bytes_to_read;
    resume->rest_of_buffer = NULL;
    resume->bytes_to_read = 0;
    if(buf) {
        int stop;
        int bytes_handled = ch_rd_read(conn, buf, nread, &stop);
        if(stop)
            _ch_pr_update_resume(resume, buf, nread, bytes_handled);
        return !stop;
    }
    return 1;
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
}

// .. c:function::
ch_error_t
ch_pr_conn_start(
        ch_chirp_t* chirp,
        ch_connection_t* conn,
        uv_tcp_t* client,
        int accept
)
//    :noindex:
//
//    see: :c:func:`ch_pr_conn_start`
//
// .. code-block:: cpp
//
{
#   begindef ch_pr_conn_start_handle_error_m(msg)
        if(tmp_err != CH_SUCCESS)
        {
            E(
                chirp,
                msg " connection (%d)",
                tmp_err
            );
            ch_cn_shutdown(conn, CH_FATAL);
            return tmp_err;
        }
#   enddef

    conn->flags  |= CH_CN_CONNECTED;
    int tmp_err = ch_cn_init(chirp, conn, conn->flags);
    ch_pr_conn_start_handle_error_m("Could not initialize")

    tmp_err = uv_tcp_nodelay(client, 1);
    ch_pr_conn_start_handle_error_m("Could not set tcp nodelay on");

    tmp_err = uv_tcp_keepalive(client, 1, CH_TCP_KEEPALIVE);
    ch_pr_conn_start_handle_error_m("Could not set tcp keepalive on ");

    uv_read_start(
        (uv_stream_t*) client,
        ch_cn_read_alloc_cb,
        _ch_pr_read_data_cb
    );
    if(conn->flags & CH_CN_ENCRYPTED) {
        if(accept)
            SSL_set_accept_state(conn->ssl);
        else {
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
static
int
_ch_pr_decrypt_feed(ch_connection_t* conn, ch_buf* buf, size_t nread, int* stop)
//    :noindex:
//
//    see: :c:func:`ch_pr_decrypt_feed`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    size_t bytes_handled = 0;
    *stop = 0;
    do {
        int tmp_err;
        tmp_err = BIO_write(
            conn->bio_app,
            buf + bytes_handled,
            nread - bytes_handled
        );
        if(tmp_err < 1) {
            if(!(conn->flags & CH_CN_STOPPED)) {
                EC(
                    chirp,
                    "SSL error writing to BIO, shutting down connection. ",
                    "ch_connection_t:%p",
                    (void*) conn
                );
                ch_cn_shutdown(conn, CH_TLS_ERROR);
                return -1;
            }
        } else
            bytes_handled += tmp_err;
        if(conn->flags & CH_CN_TLS_HANDSHAKE)
            _ch_pr_do_handshake(conn);
        else {
            ch_pr_decrypt_read(conn, stop);
            if(*stop)
                return bytes_handled;
        }
    } while(bytes_handled < nread);
    return bytes_handled;
}

// .. c:function::
void
ch_pr_decrypt_read(ch_connection_t* conn, int *stop)
//    :noindex:
//
//    see: :c:func:`ch_pr_decrypt_read`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    int tmp_err;
    *stop = 0;
    while((tmp_err = SSL_read(
            conn->ssl,
            conn->buffer_rtls,
            conn->buffer_rtls_size
    )) > 0) {;
        LC(
            chirp,
            "Read %d bytes. (unenc). ", "ch_connection_t:%p",
            tmp_err,
            (void*) conn
        );
        int bytes_handled = ch_rd_read(conn, conn->buffer_rtls, tmp_err, stop);
        if(*stop) {
            _ch_pr_update_resume(
                    &conn->tls_resume,
                    conn->buffer_rtls,
                    tmp_err,
                    bytes_handled
            );
            return;
        }
    }
    tmp_err = SSL_get_error(conn->ssl, tmp_err);
    if(tmp_err != SSL_ERROR_WANT_READ) {
        if(tmp_err < 0) {
#           ifndef NDEBUG
                ERR_print_errors_fp(stderr);
#           endif
            EC(
                chirp,
                "SSL operation fatal error. ", "ch_connection_t:%p",
                (void*) conn
            );
        } else {
            LC(
                chirp,
                "SSL operation failed. ", "ch_connection_t:%p",
                (void*) conn
            );
        }
        ch_cn_shutdown(conn, CH_TLS_ERROR);
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
    if(conn != NULL && (conn->flags & CH_CN_STOPPED)) {
        LC(conn->chirp, "Resume reading", "ch_connection_t:%p", conn);
        if(_ch_pr_resume(conn)) {
            conn->flags &= ~CH_CN_STOPPED;
            LC(conn->chirp, "Restart stream", "ch_connection_t:%p", conn);
            uv_read_start(
                (uv_stream_t*) &conn->client,
                ch_cn_read_alloc_cb,
                _ch_pr_read_data_cb
            );
        }
    }
}

// .. c:function::
static
int
_ch_pr_resume(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_pr_resume`
//
// .. code-block:: cpp
//
{
    if(conn->flags & CH_CN_ENCRYPTED) {
        int stop;
        int ret = _ch_pr_read_resume(conn, &conn->tls_resume);
        if(!ret)
            return ret;
        ch_resume_state_t* resume = &conn->read_resume;
        int bytes_handled;
        ch_buf* buf = resume->rest_of_buffer;
        int nread = resume->bytes_to_read;
        resume->rest_of_buffer = NULL;
        resume->bytes_to_read = 0;
        bytes_handled = _ch_pr_decrypt_feed(conn, buf, nread, &stop);
        if(stop)
            _ch_pr_update_resume(resume, buf, nread, bytes_handled);
        return !stop;
    } else
        return _ch_pr_read_resume(conn, &conn->read_resume);
}

// .. c:function::
static
void
_ch_pr_read_data_cb(
        uv_stream_t* stream,
        ssize_t nread,
        const uv_buf_t* buf
)
//    :noindex:
//
//    see: :c:func:`ch_pr_read_data_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = stream->data;
    ch_chirp_t* chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    int bytes_handled = 0;
#   ifndef NDEBUG
        conn->flags &= ~CH_CN_BUF_UV_USED;
#   endif
    if(nread == UV_EOF) {
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return;
    }

#   begindef ch_pr_log_nread_m(msg)
        LC(
            chirp,
            msg " ", "ch_connection_t:%p",
            (int) nread,
            (void*) conn
        );
#   enddef

    if(nread == 0) {
        ch_pr_log_nread_m("Unexpected emtpy read (%d) from libuv.");
        return;
    }
    if(nread < 0) {
        ch_pr_log_nread_m("Reader got error %d -> shutdown.");
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return;
    }
    ch_pr_log_nread_m("%d available bytes.");
    int stop;
    if(conn->flags & CH_CN_ENCRYPTED)
        bytes_handled = _ch_pr_decrypt_feed(conn, buf->base, nread, &stop);
    else {
        bytes_handled = ch_rd_read(conn, buf->base, nread, &stop);
    }
    if(stop)
        _ch_pr_update_resume(
            &conn->read_resume,
            buf->base,
            nread,
            bytes_handled
        );
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
    int tmp_err;
    ch_text_address_t tmp_addr;
    ch_chirp_t* chirp = protocol->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
    ch_config_t* config = &ichirp->config;
#   begindef ch_pr_log_listen_error_m(ip_version)
        fprintf(
            stderr,
            "%s:%d Fatal: cannot bind port (ipv%d:%d)\n",
            __FILE__,
            __LINE__,
            ip_version,
            config->PORT
        );
#   enddef

#   begindef ch_pr_listen_sock_m(ip_version, af_inet, v6only)
    {
        uv_tcp_init(ichirp->loop, &protocol->serverv##ip_version);
        protocol->serverv##ip_version.data = chirp;

        if(uv_inet_ntop(
                af_inet,
                config->BIND_V##ip_version,
                tmp_addr.data,
                sizeof(tmp_addr.data)
        ) < 0) {
            return CH_VALUE_ERROR;
        }

        if(uv_ip##ip_version##_addr(
                tmp_addr.data,
                config->PORT,
                &protocol->addrv##ip_version
        ) < 0) {
            return CH_VALUE_ERROR;
        }

        tmp_err = ch_uv_error_map(uv_tcp_bind(
            &protocol->serverv##ip_version,
            (const struct sockaddr*)&protocol->addrv##ip_version,
            v6only
        ));
        if(tmp_err != CH_SUCCESS) {
            ch_pr_log_listen_error_m(ip_version)
            return tmp_err;
        }

        if(uv_tcp_nodelay(&protocol->serverv##ip_version, 1) < 0) {
            return CH_UV_ERROR;
        }

        if(uv_listen(
                (uv_stream_t*) &protocol->serverv##ip_version,
                config->BACKLOG,
                _ch_pr_new_connection_cb
        ) < 0) {
            ch_pr_log_listen_error_m(ip_version)
            return CH_EADDRINUSE;
        }
    }
#   enddef

    ch_pr_listen_sock_m(4, AF_INET, 0);
    ch_pr_listen_sock_m(6, AF_INET6, UV_TCP_IPV6ONLY);

    ch_rm_tree_init(&protocol->remotes);
    protocol->old_connections = NULL;
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
    _ch_pr_close_free_connections(chirp);
    uv_close((uv_handle_t*) &protocol->serverv4, ch_chirp_close_cb);
    uv_close((uv_handle_t*) &protocol->serverv6, ch_chirp_close_cb);
    chirp->_->closing_tasks += 2;
    return CH_SUCCESS;
}
