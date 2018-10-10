// ==========
// Connection
// ==========
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "connection.h"
#include "chirp.h"
#include "common.h"
#include "remote.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#ifndef CH_WITHOUT_TLS
#include <openssl/err.h>
#endif

// Data Struct Prototypes
// ======================
//
// .. code-block:: cpp

rb_bind_impl_m(ch_cn, ch_connection_t) CH_ALLOW_NL;

qs_stack_bind_impl_m(ch_cn_st, ch_connection_t) CH_ALLOW_NL;

// Declarations
// ============

// .. c:function::
static ch_error_t
_ch_cn_allocate_buffers(ch_connection_t* conn);
//
//    Allocate the connections communication buffers.
//
//    :param ch_connection_t* conn: Connection
//

// .. c:function::
static void
_ch_cn_closing(ch_connection_t* conn);
//
//    Called by ch_cn_shutdown to enter the closing stage.
//
//    :param ch_connection_t: Connection to close
//

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_partial_write(ch_connection_t* conn);
//
//    Called by libuv when pending data has been sent.
//
//    :param ch_connection_t* conn: Connection
//
#endif

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_send_pending_cb(uv_write_t* req, int status);
//
//    Called during handshake by libuv when pending data is sent.
//
//    :param uv_write_t* req: Write request type, holding the
//                            connection handle
//    :param int status: Send status
//
#endif

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_write_cb(uv_write_t* req, int status);
//
//    Callback used for ch_cn_write.
//
//    :param uv_write_t* req: Write request
//    :param int status: Write status
//
#endif

// .. c:function::
static void
_ch_cn_send_handshake_cb(uv_write_t* req, int status);
//
//    Called when handshake is sent.
//
//    :param uv_write_t* req: Write request type, holding the
//                            connection handle
//    :param int status: Send status

// Definitions
// ===========
//
// .. code-block:: cpp

MINMAX_FUNCS(size_t)

// .. c:function::
void
ch_cn_abort_one_message(ch_remote_t* remote, ch_error_t error)
//    :noindex:
//
//    see: :c:func:`ch_cn_abort_one_message`
//
// .. code-block:: cpp
//
{
    ch_message_t* msg = NULL;
    if (remote->cntl_msg_queue != NULL) {
        ch_msg_dequeue(&remote->cntl_msg_queue, &msg);
    } else if (remote->msg_queue != NULL) {
        ch_msg_dequeue(&remote->msg_queue, &msg);
    }
    if (msg != NULL) {
#ifdef CH_ENABLE_LOGGING
        {
            char id[CH_ID_SIZE * 2 + 1];
            ch_bytes_to_hex(
                    msg->identity, sizeof(msg->identity), id, sizeof(id));
            if (msg->type & CH_MSG_ACK) {
                LC(remote->chirp,
                   "Abort message on queue id: %s\n"
                   "                             "
                   "ch_message_t:%p",
                   id,
                   (void*) msg);
            }
        }
#endif
        ch_send_cb_t cb = msg->_send_cb;
        if (cb != NULL) {
            msg->_send_cb = NULL;
            cb(remote->chirp, msg, error);
        }
    }
}

// .. c:function::
static ch_error_t
_ch_cn_allocate_buffers(ch_connection_t* conn)
//    :noindex:
//
//    See: :c:func:`_ch_cn_allocate_buffers`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    size_t          size   = ichirp->config.BUFFER_SIZE;
    if (size == 0) {
        size = CH_BUFFER_SIZE;
    }
    conn->buffer_uv   = ch_alloc(size);
    conn->buffer_size = size;
    if (conn->flags & CH_CN_ENCRYPTED) {
        conn->buffer_wtls = ch_alloc(size);
        size              = ch_min_size_t(size, CH_ENC_BUFFER_SIZE);
        conn->buffer_rtls = ch_alloc(size);
    }
    int alloc_nok = 0;
    if (conn->flags & CH_CN_ENCRYPTED) {
        alloc_nok =
                !(conn->buffer_uv && conn->buffer_wtls && conn->buffer_rtls);
    } else {
        alloc_nok = !conn->buffer_uv;
    }
    if (alloc_nok) {
        EC(chirp,
           "Could not allocate memory for libuv and tls. ",
           "ch_connection_t:%p",
           (void*) conn);
        return CH_ENOMEM;
    }
    conn->buffer_rtls_size = size;
    conn->buffer_uv_uv     = uv_buf_init(conn->buffer_uv, conn->buffer_size);
    conn->buffer_wtls_uv   = uv_buf_init(conn->buffer_wtls, conn->buffer_size);
    conn->bufs             = ch_alloc(sizeof(uv_buf_t) * 3);
    conn->bufs_size        = 3;
    conn->flags |= CH_CN_INIT_BUFFERS;
    A((conn->flags & CH_CN_INIT) == CH_CN_INIT,
      "Connection not fully initialized");
    return CH_SUCCESS;
}

// .. c:function::
static void
_ch_cn_closing(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`_ch_cn_closing`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    LC(chirp, "Shutdown callback called. ", "ch_connection_t:%p", (void*) conn);
    conn->flags &= ~CH_CN_CONNECTED;
    uv_handle_t* handle = (uv_handle_t*) &conn->client;
    if (uv_is_closing(handle)) {
        EC(chirp,
           "Connection already closing on closing. ",
           "ch_connection_t:%p",
           (void*) conn);
    } else {
        uv_read_stop((uv_stream_t*) &conn->client);
        if (conn->flags & CH_CN_INIT_READER_WRITER) {
            ch_wr_free(&conn->writer);
            ch_rd_free(&conn->reader);
            conn->flags &= ~CH_CN_INIT_READER_WRITER;
        }
        if (conn->flags & CH_CN_INIT_CLIENT) {
            uv_close(handle, ch_cn_close_cb);
            conn->shutdown_tasks += 1;
            conn->flags &= ~CH_CN_INIT_CLIENT;
        }
        if (conn->flags & CH_CN_INIT_CONNECT_TIMEOUT) {
            uv_close((uv_handle_t*) &conn->connect_timeout, ch_cn_close_cb);
            conn->flags &= ~CH_CN_INIT_CONNECT_TIMEOUT;
            conn->shutdown_tasks += 1;
        }
        LC(chirp,
           "Closing connection after shutdown. ",
           "ch_connection_t:%p",
           (void*) conn);
    }
}

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_partial_write(ch_connection_t* conn)
//    :noindex:
//
//    See: :c:func:`_ch_cn_partial_write`
//
// .. code-block:: cpp
//
{
    size_t      bytes_encrypted = 0;
    size_t      bytes_read      = 0;
    ch_chirp_t* chirp           = conn->chirp;
    ch_chirp_check_m(chirp);
    A(!(conn->flags & CH_CN_BUF_WTLS_USED), "The wtls buffer is still used");
    A(!(conn->flags & CH_CN_WRITE_PENDING), "Another uv write is pending");
#ifdef CH_ENABLE_ASSERTS
    conn->flags |= CH_CN_WRITE_PENDING;
    conn->flags |= CH_CN_BUF_WTLS_USED;
#endif
    for (;;) {
        /* Read all data pending in BIO */
        int can_read_more = 1;
        int pending       = BIO_pending(conn->bio_app);
        while (pending && can_read_more) {
            ssize_t read = BIO_read(
                    conn->bio_app,
                    conn->buffer_wtls + bytes_read,
                    conn->buffer_size - bytes_read);
            A(read > 0, "BIO_read failure unexpected");
            if (read < 1) {
                EC(chirp,
                   "SSL error reading from BIO, shutting down connection. ",
                   "ch_connection_t:%p",
                   (void*) conn);
                ch_cn_shutdown(conn, CH_TLS_ERROR);
                return;
            }
            bytes_read += read;
            can_read_more = bytes_read < conn->buffer_size;

            pending = BIO_pending(conn->bio_app);
        }
        if (!can_read_more) {
            break;
        }
        uv_buf_t* buf      = &conn->bufs[conn->bufs_index];
        int can_write_more = (bytes_encrypted + conn->write_written) < buf->len;
        if (!can_write_more) {
            /* Switch to next buffer if available or break */
            int changed = 0;
            while (conn->bufs_index < (conn->nbufs - 1)) {
                conn->bufs_index += 1;
                changed = 1;
                if (conn->bufs[conn->bufs_index].len != 0) {
                    break;
                }
            }
            if (conn->bufs[conn->bufs_index].len == 0 || !changed) {
                break;
            }
            conn->write_written = 0;
            bytes_encrypted     = 0;
            can_write_more      = 1;
        }
        buf = &conn->bufs[conn->bufs_index];
        /* Write data into BIO */
        if (can_read_more && can_write_more) {
            int tmp_err = SSL_write(
                    conn->ssl,
                    buf->base + bytes_encrypted + conn->write_written,
                    buf->len - bytes_encrypted - conn->write_written);
            bytes_encrypted += tmp_err;
            A(tmp_err > -1, "SSL_write failure unexpected");
            if (tmp_err < 0) {
                EC(chirp,
                   "SSL error writing to BIO, shutting down connection. ",
                   "ch_connection_t:%p",
                   (void*) conn);
                ch_cn_shutdown(conn, CH_TLS_ERROR);
                return;
            }
        }
    }
    conn->buffer_wtls_uv.len = bytes_read;
    uv_write(
            &conn->write_req,
            (uv_stream_t*) &conn->client,
            &conn->buffer_wtls_uv,
            1,
            _ch_cn_write_cb);
    LC(chirp,
       "Called uv_write with %d bytes. ",
       "ch_connection_t:%p",
       (int) bytes_read,
       (void*) conn);
    conn->write_written += bytes_encrypted;
}
#endif

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_send_pending_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_cn_send_pending_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
#ifdef CH_ENABLE_ASSERTS
    conn->flags &= ~CH_CN_WRITE_PENDING;
    conn->flags &= ~CH_CN_BUF_WTLS_USED;
#endif
    if (status < 0) {
        LC(chirp,
           "Sending pending data failed. ",
           "ch_connection_t:%p",
           (void*) conn);
        ch_cn_shutdown(conn, CH_WRITE_ERROR);
        return;
    }
    if (conn->flags & CH_CN_SHUTTING_DOWN) {
        LC(chirp,
           "Write shutdown bytes to connection successful. ",
           "ch_connection_t:%p",
           (void*) conn);
    } else {
        LC(chirp,
           "Write handshake bytes to connection successful. ",
           "ch_connection_t:%p",
           (void*) conn);
    }
    ch_cn_send_if_pending(conn);
}
#endif

#ifndef CH_WITHOUT_TLS
// .. c:function::
static void
_ch_cn_write_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_cn_write_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
#ifdef CH_ENABLE_ASSERTS
    conn->flags &= ~CH_CN_WRITE_PENDING;
    conn->flags &= ~CH_CN_BUF_WTLS_USED;
#endif
    if (status < 0) {
        LC(chirp,
           "Sending pending data failed. ",
           "ch_connection_t:%p",
           (void*) conn);
        uv_write_cb cb       = conn->write_callback;
        conn->write_callback = NULL;
        cb(req, CH_WRITE_ERROR);
        ch_cn_shutdown(conn, CH_WRITE_ERROR);
        return;
    }
    /* Check if we can write data */
    uv_buf_t* buf     = &conn->bufs[conn->bufs_index];
    int       pending = BIO_pending(conn->bio_app);
    if (buf->len > conn->write_written || pending) {
        _ch_cn_partial_write(conn);
        LC(chirp,
           "Partially encrypted %d of %d bytes. ",
           "ch_connection_t:%p",
           (int) conn->write_written,
           (int) buf->len,
           (void*) conn);
    } else {
        A(pending == 0, "Unexpected pending data on TLS write");
        LC(chirp,
           "Completely sent %d bytes (unenc). ",
           "ch_connection_t:%p",
           (int) conn->write_written,
           (void*) conn);
        conn->write_written = 0;
#ifdef CH_ENABLE_ASSERTS
        conn->flags &= ~CH_CN_ENCRYPTED_WRITE;
#endif
        if (conn->write_callback != NULL) {
            uv_write_cb cb       = conn->write_callback;
            conn->write_callback = NULL;
            cb(req, status);
        }
    }
}
#endif

// .. c:function::
void
ch_cn_close_cb(uv_handle_t* handle)
//    :noindex:
//
//    see: :c:func:`ch_cn_close_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = handle->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    conn->shutdown_tasks -= 1;
    A(conn->shutdown_tasks > -1, "Shutdown semaphore dropped below zero");
    LC(chirp,
       "Shutdown semaphore (%d). ",
       "ch_connection_t:%p",
       conn->shutdown_tasks,
       (void*) conn);
    /* In production we allow the semaphore to drop below 0, but we log an
     * error. */
    if (conn->shutdown_tasks < 0) {
        E(chirp,
          "Shutdown semaphore dropped blow 0. ch_connection_t: %p",
          conn);
    }
    if (conn->shutdown_tasks < 1) {
        if (conn->flags & CH_CN_DO_CLOSE_ACCOUTING) {
            ichirp->closing_tasks -= 1;
            LC(chirp,
               "Closing semaphore (%d). ",
               "uv_handle_t:%p",
               ichirp->closing_tasks,
               (void*) handle);
        }
        if (conn->flags & CH_CN_INIT_BUFFERS) {
            A(conn->buffer_uv, "Initialized buffers inconsistent");
            ch_free(conn->bufs);
            ch_free(conn->buffer_uv);
            if (conn->flags & CH_CN_ENCRYPTED) {
                A(conn->buffer_wtls, "Initialized buffers inconsistent");
                A(conn->buffer_rtls, "Initialized buffers inconsistent");
                ch_free(conn->buffer_wtls);
                ch_free(conn->buffer_rtls);
            }
            conn->flags &= ~CH_CN_INIT_BUFFERS;
        }
#ifndef CH_WITHOUT_TLS
        if (conn->flags & CH_CN_ENCRYPTED) {
            /* The doc says this frees conn->bio_ssl I tested it. let's
             * hope they never change that. */
            if (conn->flags & CH_CN_INIT_ENCRYPTION) {
                A(conn->ssl, "Initialized ssl handles inconsistent");
                A(conn->bio_app, "Initialized ssl handles inconsistent");
                SSL_free(conn->ssl);
                BIO_free(conn->bio_app);
            }
        }
#endif
        /* Since we define a unencrypted connection as CH_CN_INIT_ENCRYPTION. */
        conn->flags &= ~CH_CN_INIT_ENCRYPTION;
        A(!(conn->flags & CH_CN_INIT),
          "Connection resources haven't been freed completely");
        A(!(conn->flags & CH_CN_CONNECTED),
          "Connection not properly disconnected");
        ch_remote_t* remote = conn->delete_remote;
        if (remote != NULL) {
            LC(chirp,
               "Deleted ch_remote_t: %p. ",
               "ch_connection_t:%p",
               (void*) remote,
               (void*) conn);
            ch_rm_free(remote);
        }
        ch_free(conn);
        LC(chirp,
           "Closed connection, closing semaphore (%d). ",
           "ch_connection_t:%p",
           chirp->_->closing_tasks,
           (void*) conn);
    }
}

// .. c:function::
ch_error_t
ch_cn_init(ch_chirp_t* chirp, ch_connection_t* conn, uint8_t flags)
//    :noindex:
//
//    see: :c:func:`ch_cn_init`
//
// .. code-block:: cpp
//
{
    int tmp_err;

    ch_chirp_int_t* ichirp = chirp->_;
    conn->chirp            = chirp;
    conn->write_req.data   = conn;
    conn->flags |= flags;
    conn->timestamp = uv_now(ichirp->loop);
    tmp_err         = ch_rd_init(&conn->reader, conn, ichirp);
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }
    tmp_err = ch_wr_init(&conn->writer, conn);
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }
    conn->flags |= CH_CN_INIT_READER_WRITER;
#ifndef CH_WITHOUT_TLS
    if (conn->flags & CH_CN_ENCRYPTED) {
        tmp_err = ch_cn_init_enc(chirp, conn);
    }
#endif
    if (tmp_err != CH_SUCCESS) {
        return tmp_err;
    }
    /* An unencrypted connection also has CH_CN_INIT_ENCRYPTION */
    conn->flags |= CH_CN_INIT_ENCRYPTION;
    return _ch_cn_allocate_buffers(conn);
}

#ifndef CH_WITHOUT_TLS
// .. c:function::
ch_error_t
ch_cn_init_enc(ch_chirp_t* chirp, ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_cn_init_enc`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp = chirp->_;
    conn->ssl              = SSL_new(ichirp->encryption.ssl_ctx);
    if (conn->ssl == NULL) {
#ifdef CH_ENABLE_LOGGING
        ERR_print_errors_fp(stderr);
#endif
        EC(chirp, "Could not create SSL. ", "ch_connection_t:%p", (void*) conn);
        return CH_TLS_ERROR;
    }
    if (BIO_new_bio_pair(&(conn->bio_ssl), 0, &(conn->bio_app), 0) != 1) {
#ifdef CH_ENABLE_LOGGING
        ERR_print_errors_fp(stderr);
#endif
        EC(chirp,
           "Could not create BIO pair. ",
           "ch_connection_t:%p",
           (void*) conn);
        SSL_free(conn->ssl);
        return CH_TLS_ERROR;
    }
    SSL_set_bio(conn->ssl, conn->bio_ssl, conn->bio_ssl);
#ifdef CH_CN_PRINT_CIPHERS
    STACK_OF(SSL_CIPHER)* ciphers = SSL_get_ciphers(conn->ssl);
    while (sk_SSL_CIPHER_num(ciphers) > 0)
        fprintf(stderr,
                "%s\n",
                SSL_CIPHER_get_name(sk_SSL_CIPHER_pop(ciphers)));
    sk_SSL_CIPHER_free(ciphers);

#endif
    LC(chirp, "SSL context created. ", "ch_connection_t:%p", (void*) conn);
    return CH_SUCCESS;
}
#endif

// .. c:function::
void
ch_cn_read_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
//    :noindex:
//
//    see: :c:func:`ch_cn_read_alloc_cb`
//
// .. code-block:: cpp
//
{
    /* That whole suggested size concept doesn't work, we have to allocated
     * consistent buffers. */
    (void) (suggested_size);
    ch_connection_t* conn  = handle->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    A(!(conn->flags & CH_CN_BUF_UV_USED), "UV buffer still used");
#ifdef CH_ENABLE_ASSERTS
    conn->flags |= CH_CN_BUF_UV_USED;
#endif
    buf->base = conn->buffer_uv;
    buf->len  = conn->buffer_size;
}

#ifndef CH_WITHOUT_TLS
// .. c:function::
void
ch_cn_send_if_pending(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_cn_send_if_pending`
//
// .. code-block:: cpp
//
{
    A(!(conn->flags & CH_CN_WRITE_PENDING), "Another write is still pending");
    ch_chirp_t* chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    int pending = BIO_pending(conn->bio_app);
    if (pending < 1) {
        if (!(conn->flags & CH_CN_TLS_HANDSHAKE ||
              conn->flags & CH_CN_SHUTTING_DOWN)) {
            ch_cn_send_handshake(conn);
        }
        return;
    }
    A(!(conn->flags & CH_CN_BUF_WTLS_USED), "The wtls buffer is still used");
#ifdef CH_ENABLE_ASSERTS
    conn->flags |= CH_CN_BUF_WTLS_USED;
    conn->flags |= CH_CN_WRITE_PENDING;
#endif
    ssize_t read =
            BIO_read(conn->bio_app, conn->buffer_wtls, conn->buffer_size);
    conn->buffer_wtls_uv.len = read;
    uv_write(
            &conn->write_req,
            (uv_stream_t*) &conn->client,
            &conn->buffer_wtls_uv,
            1,
            _ch_cn_send_pending_cb);
    if (conn->flags & CH_CN_SHUTTING_DOWN) {
        LC(chirp,
           "Sending %d pending shutdown bytes. ",
           "ch_connection_t:%p",
           read,
           (void*) conn);
    } else {
        LC(chirp,
           "Sending %d pending handshake bytes. ",
           "ch_connection_t:%p",
           read,
           (void*) conn);
    }
}
#endif

// .. c:function::
ch_error_t
ch_cn_shutdown(ch_connection_t* conn, int reason)
//    :noindex:
//
//    see: :c:func:`ch_cn_shutdown`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    if (conn->flags & CH_CN_SHUTTING_DOWN) {
        EC(chirp, "Shutdown in progress. ", "ch_connection_t:%p", (void*) conn);
        return CH_IN_PRORESS;
    }
    LC(chirp, "Shutdown connection. ", "ch_connection_t:%p", (void*) conn);
    conn->flags |= CH_CN_SHUTTING_DOWN;
    ch_pr_debounce_connection(conn);
    ch_chirp_int_t*  ichirp = chirp->_;
    ch_writer_t*     writer = &conn->writer;
    ch_remote_t*     remote = conn->remote;
    ch_connection_t* out;
    /* In case this conn is in handshake_conns remove it, since we aborted the
     * handshake */
    ch_cn_delete(&ichirp->protocol.handshake_conns, conn, &out);
    /* In case this conn is in old_connections remove it, since we now cleaned
     * it up*/
    ch_cn_delete(&ichirp->protocol.old_connections, conn, &out);
    conn->remote = NULL; /* Disassociate from remote */
    if (conn->flags & CH_CN_INIT_CLIENT) {
        uv_read_stop((uv_stream_t*) &conn->client);
    }
    ch_message_t* msg = writer->msg;
    ch_message_t* wam = NULL;
    /* In early handshake remote can empty, since we allocate resources after
     * successful handshake. */
    if (remote) {
        /* We finish the message and therefore set wam to NULL. */
        wam                      = remote->wait_ack_message;
        remote->wait_ack_message = NULL;
        /* We could be a connection from old_connections and therefore we do
         * not want to invalidate an active connection. */
        if (remote->conn == conn) {
            remote->conn = NULL;
        }
        /* Abort all ack messsages */
        remote->cntl_msg_queue = NULL;
    }
    if (wam != NULL) {
        wam->_flags |= CH_MSG_FAILURE;
        ch_chirp_finish_message(chirp, conn, wam, reason);
    }
    if (msg != NULL && msg != wam) {
        msg->_flags |= CH_MSG_FAILURE;
        ch_chirp_finish_message(chirp, conn, msg, reason);
    }
    /* finish vs abort - finish: cancel a message on the current connection.
     * abort: means canceling a message that hasn't been queued yet. If
     * possible we don't want to cancel a message that hasn't been queued
     * yet.*/
    if (wam == NULL && msg == NULL && remote != NULL) {
        /* If we have not finished a message we abort one on the remote. */
        ch_cn_abort_one_message(remote, reason);
    }
#ifndef CH_WITHOUT_TLS
    if (conn->flags & CH_CN_ENCRYPTED && conn->flags & CH_CN_INIT_ENCRYPTION) {
        int tmp_err = SSL_get_verify_result(conn->ssl);
        if (tmp_err != X509_V_OK) {
            EC(chirp,
               "Connection has cert verification error: %d. ",
               "ch_connection_t:%p",
               tmp_err,
               (void*) conn);
        }
    }
#endif
    if (ichirp->flags & CH_CHIRP_CLOSING) {
        conn->flags |= CH_CN_DO_CLOSE_ACCOUTING;
        ichirp->closing_tasks += 1;
    }
    _ch_cn_closing(conn);
    return CH_SUCCESS;
}

// .. c:function::
void
ch_cn_write(
        ch_connection_t* conn,
        const uv_buf_t   bufs[],
        unsigned int     nbufs,
        uv_write_cb      callback)
//    :noindex:
//
//    see: :c:func:`ch_cn_write`
//
// .. code-block:: cpp
//
{
    size_t buf_list_size = sizeof(uv_buf_t) * nbufs;
    if (nbufs > conn->bufs_size) {
        conn->bufs      = ch_realloc(conn->bufs, buf_list_size);
        conn->bufs_size = nbufs;
    }
    memcpy(conn->bufs, bufs, buf_list_size);
#ifndef CH_WITHOUT_TLS
    if (conn->flags & CH_CN_ENCRYPTED) {
        conn->write_callback = callback;
        conn->write_written  = 0;
        conn->bufs_index     = 0;
        conn->nbufs          = nbufs;
#ifdef CH_ENABLE_ASSERTS
        A(!(conn->flags & CH_CN_ENCRYPTED_WRITE), "Encrypted write pending");
        conn->flags |= CH_CN_ENCRYPTED_WRITE;
        int pending = BIO_pending(conn->bio_app);
        A(pending == 0, "There is still pending data in SSL BIO");
#endif
        _ch_cn_partial_write(conn);
        return;
    }
#endif
    uv_write(
            &conn->write_req,
            (uv_stream_t*) &conn->client,
            conn->bufs,
            nbufs,
            callback);
#ifdef CH_ENABLE_LOGGING
    ch_chirp_t* chirp = conn->chirp;
    for (unsigned int i = 0; i < nbufs; i++) {
        LC(chirp,
           "Wrote %d bytes. ",
           "ch_connection_t:%p",
           (int) conn->bufs[i].len,
           (void*) conn);
    }
#endif
}

// .. c:function::
static void
_ch_cn_send_handshake_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_cn_send_handshake_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    if (status < 0) {
        LC(chirp,
           "Sending handshake failed. ",
           "ch_connection_t:%p",
           (void*) conn);
        ch_cn_shutdown(conn, CH_WRITE_ERROR);
        return;
    }
#ifndef CH_WITHOUT_TLS
    /* Check if we already have a message (just after handshake)
     * this is here so we have no overlapping ch_cn_write. If the read causes a
     * ack message to be sent and the write of the handshake is not finished,
     * chirp would assert or be in undefined state. */
    if (conn->flags & CH_CN_ENCRYPTED) {
        int stop;
        ch_pr_decrypt_read(conn, &stop);
    }
#endif
}

// .. c:function::
void
ch_cn_send_handshake(ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`_ch_cn_send_handshake`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = conn->chirp;
    LC(chirp, "Sending handshake to. ", "ch_connection_t:%p", (void*) conn);
    ch_chirp_int_t*   ichirp = chirp->_;
    ch_sr_handshake_t hs_tmp;
    ch_buf            hs_buf[CH_SR_HANDSHAKE_SIZE];
    hs_tmp.port = ichirp->public_port;
    memcpy(hs_tmp.identity, ichirp->identity, CH_ID_SIZE);
    ch_sr_hs_to_buf(&hs_tmp, hs_buf);
    uv_buf_t buf;
    buf.base = hs_buf;
    buf.len  = CH_SR_HANDSHAKE_SIZE;
    ch_cn_write(conn, &buf, 1, _ch_cn_send_handshake_cb);
}
