// ======
// Writer
// ======
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "writer.h"
#include "chirp.h"
#include "common.h"
#include "protocol.h"
#include "remote.h"
#include "util.h"

// Declarations
// ============

// .. c:function::
static int
_ch_wr_check_write_error(
        ch_chirp_t*      chirp,
        ch_writer_t*     writer,
        ch_connection_t* conn,
        int              status);
//
//    Check if the given status is erroneous (that is, there was an error
//    during writing) and cleanup if necessary. Cleaning up means shutting down
//    the given connection instance, stopping the timer for the send-timeout.
//
//    :param ch_chirp_t* chirp:      Pointer to a chirp instance.
//    :param ch_writer_t* writer:    Pointer to a writer instance.
//    :param ch_connection_t* conn:  Pointer to a connection instance.
//    :param int status:             Status of the write, of type
//                                   :c:type:`ch_error_t`.
//
//    :return:                       the status, which is of type
//                                   :c:type:`ch_error_t`.
//    :rtype:                        int
//

// .. c:function::
static ch_error_t
_ch_wr_connect(ch_remote_t* remote);
//
//    Connects to a remote peer.
//
//    :param ch_remote_t* remote: Remote to connect to.

// .. c:function::
static void
_ch_wr_connect_cb(uv_connect_t* req, int status);
//
//    Called by libuv after trying to connect. Contains the connection status.
//
//    :param uv_connect_t* req: Connect request, containing the connection.
//    :param int status:        Status of the connection.
//

// .. c:function::
static void
_ch_wr_connect_timeout_cb(uv_timer_t* handle);
//
//    Callback which is called after the connection reaches its timeout for
//    connecting. The timeout is set by the chirp configuration and is 5 seconds
//    by default. When this callback is called, the connection is shutdown.
//
//    :param uv_timer_t* handle: uv timer handle, data contains chirp

// .. c:function::
static void
_ch_wr_write_data_cb(uv_write_t* req, int status);
//
//    Callback which is called after data was written.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

// .. c:function::
static void
_ch_wr_write_finish(
        ch_chirp_t* chirp, ch_writer_t* writer, ch_connection_t* conn);
//
//    Finishes the current write operation, the message store on the writer is
//    set to NULL and :c:func:`ch_chirp_finish_message` is called.
//
//    :param ch_chirp_t* chirp:      Pointer to a chirp instance.
//    :param ch_writer_t* writer:    Pointer to a writer instance.
//    :param ch_connection_t* conn:  Pointer to a connection instance.

// .. c:function::
static void
_ch_wr_write_timeout_cb(uv_timer_t* handle);
//
//    Callback which is called after the writer reaches its timeout for
//    sending. The timeout is set by the chirp configuration and is 5 seconds
//    by default. When this callback is called, the connection is being shut
//    down.
//
//    :param uv_timer_t* handle: uv timer handle, data contains chirp
//
// .. c:function::
static void
_ch_wr_enqeue_probe_if_needed(ch_remote_t* remote);
//
//    If remote wasn't use for 3/4 REUSE_TIME, we send a probe message before
//    the actual message. A probe message will probe the connection before
//    sending the actual message. If there is a garbage-collection race (the
//    remote is already closing the connection), the probe would fail and not
//    the actual message, then the closing of the connection is detected and it
//    will be reestablished on sending the actual message. Probes are a way of
//    preventing gc-races.
//
//    :param ch_remote_t* remote: Remote to send the probe to.

// Definitions
// ===========

// .. c:function::
static int
_ch_wr_check_write_error(
        ch_chirp_t*      chirp,
        ch_writer_t*     writer,
        ch_connection_t* conn,
        int              status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_check_write_error`
//
// .. code-block:: cpp
//
{
    A(writer->msg != NULL, "ⱳriter->msg should be set on callback");
    (void) (writer);
    if (status != CH_SUCCESS) {
        LC(chirp,
           "Write failed with uv status: %d. ",
           "ch_connection_t:%p",
           status,
           (void*) conn);
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return CH_PROTOCOL_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
static ch_error_t
_ch_wr_connect(ch_remote_t* remote)
//    :noindex:
//
//    see: :c:func:`_ch_wr_connect`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*      chirp  = remote->chirp;
    ch_chirp_int_t*  ichirp = chirp->_;
    ch_connection_t* conn   = ch_alloc(sizeof(*conn));
    if (!conn) {
        return CH_ENOMEM;
    }
    remote->conn = conn;
    memset(conn, 0, sizeof(*conn));
    ch_cn_node_init(conn);
    conn->chirp        = chirp;
    conn->port         = remote->port;
    conn->ip_protocol  = remote->ip_protocol;
    conn->connect.data = conn;
    conn->remote       = remote;
    conn->client.data  = conn;
    int tmp_err        = uv_timer_init(ichirp->loop, &conn->connect_timeout);
    if (tmp_err != CH_SUCCESS) {
        EC(chirp,
           "Initializing connect timeout failed: %d. ",
           "ch_connection_t:%p",
           tmp_err,
           (void*) conn);
        ch_cn_shutdown(conn, CH_UV_ERROR);
        return CH_INIT_FAIL;
    }
    conn->connect_timeout.data = conn;
    conn->flags |= CH_CN_INIT_CONNECT_TIMEOUT;
    tmp_err = uv_timer_start(
            &conn->connect_timeout,
            _ch_wr_connect_timeout_cb,
            ichirp->config.TIMEOUT * 1000,
            0);
    if (tmp_err != CH_SUCCESS) {
        EC(chirp,
           "Starting connect timeout failed: %d. ",
           "ch_connection_t:%p",
           tmp_err,
           (void*) conn);
        ch_cn_shutdown(conn, CH_UV_ERROR);
        return CH_UV_ERROR;
    }

    ch_text_address_t taddr;
    uv_inet_ntop(
            remote->ip_protocol,
            remote->address,
            taddr.data,
            sizeof(taddr.data));
#ifndef CH_WITHOUT_TLS
    if (!(ichirp->config.DISABLE_ENCRYPTION || ch_is_local_addr(&taddr))) {
        conn->flags |= CH_CN_ENCRYPTED;
    }
#endif
    memcpy(&conn->address, &remote->address, CH_IP_ADDR_SIZE);
    if (uv_tcp_init(ichirp->loop, &conn->client) < 0) {
        EC(chirp,
           "Could not initialize tcp. ",
           "ch_connection_t:%p",
           (void*) conn);
        ch_cn_shutdown(conn, CH_CANNOT_CONNECT);
        return CH_INIT_FAIL;
    }
    conn->flags |= CH_CN_INIT_CLIENT;
    struct sockaddr_storage addr;
    /* No error can happen, the address was taken from a binary format */
    ch_textaddr_to_sockaddr(remote->ip_protocol, &taddr, remote->port, &addr);
    tmp_err = uv_tcp_connect(
            &conn->connect,
            &conn->client,
            (struct sockaddr*) &addr,
            _ch_wr_connect_cb);
    if (tmp_err != CH_SUCCESS) {
        E(chirp,
          "Failed to connect to host: %s:%d (%d)",
          taddr.data,
          remote->port,
          tmp_err);
        ch_cn_shutdown(conn, CH_CANNOT_CONNECT);
        return CH_CANNOT_CONNECT;
    }
    LC(chirp,
       "Connecting to remote %s:%d. ",
       "ch_connection_t:%p",
       taddr.data,
       remote->port,
       (void*) conn);
    return CH_SUCCESS;
}

// .. c:function::
void
_ch_wr_connect_cb(uv_connect_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_connect_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_text_address_t taddr;
    A(chirp == conn->chirp, "Chirp on connection should match");
    uv_inet_ntop(
            conn->ip_protocol, conn->address, taddr.data, sizeof(taddr.data));
    if (status == CH_SUCCESS) {
        LC(chirp,
           "Connected to remote %s:%d. ",
           "ch_connection_t:%p",
           taddr.data,
           conn->port,
           (void*) conn);
        /* A connection is created at either _ch_pr_new_connection_cb
         * (incoming) or _ch_wr_connect (outgoing). After connect
         * (_ch_wr_connection_cb) both code-paths will continue at
         * ch_pr_conn_start. From there on incoming and outgoing connections
         * are handled the same way. */
        ch_pr_conn_start(chirp, conn, &conn->client, 0);
    } else {
        EC(chirp,
           "Connection to remote failed %s:%d (%d). ",
           "ch_connection_t:%p",
           taddr.data,
           conn->port,
           status,
           (void*) conn);
        ch_cn_shutdown(conn, CH_CANNOT_CONNECT);
    }
}

// .. c:function::
static void
_ch_wr_connect_timeout_cb(uv_timer_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_wr_connect_timeout_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = handle->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    LC(chirp, "Connect timed out. ", "ch_connection_t:%p", (void*) conn);
    ch_cn_shutdown(conn, CH_TIMEOUT);
    uv_timer_stop(&conn->connect_timeout);
    /* We have waited long enough, we send the next message */
    ch_remote_t  key;
    ch_remote_t* remote = NULL;
    ch_rm_init_from_conn(chirp, &key, conn, 1);
    if (ch_rm_find(chirp->_->protocol.remotes, &key, &remote) == CH_SUCCESS) {
        ch_wr_process_queues(remote);
    }
}

// .. c:function::
static void
_ch_wr_enqeue_probe_if_needed(ch_remote_t* remote)
//    :noindex:
//
//    see: :c:func:`_ch_wr_enqeue_probe_if_needed`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = remote->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
    ch_config_t*    config = &ichirp->config;
    ch_message_t*   noop   = remote->noop;
    uint64_t        now    = uv_now(ichirp->loop);
    uint64_t        delta  = (1000 * config->REUSE_TIME / 4 * 3);
    if (now - remote->timestamp > delta) {
        if (noop == NULL) {
            remote->noop = ch_alloc(sizeof(*remote->noop));
            if (remote->noop == NULL) {
                return; /* ENOMEM: Noop are not important, we don't send it. */
            }
            noop = remote->noop;
            memset(noop, 0, sizeof(*noop));
            memcpy(noop->address, remote->address, CH_IP_ADDR_SIZE);
            noop->ip_protocol = remote->ip_protocol;
            noop->port        = remote->port;
            noop->type        = CH_MSG_NOOP;
        }
        /* The noop is not enqueued yet, enqueue it */
        if (!(noop->_flags & CH_MSG_USED) && noop->_next == NULL) {
            LC(chirp, "Sending NOOP.", "ch_remote_t:%p", remote);
            ch_msg_enqueue(&remote->cntl_msg_queue, noop);
        }
    }
}

// .. c:function::
static void
_ch_wr_write_data_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_write_data_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_writer_t* writer = &conn->writer;
    if (_ch_wr_check_write_error(chirp, writer, conn, status)) {
        return;
    }
    _ch_wr_write_finish(chirp, writer, conn);
}

// .. c:function::
static void
_ch_wr_write_finish(
        ch_chirp_t* chirp, ch_writer_t* writer, ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`_ch_wr_write_finish`
//
// .. code-block:: cpp
//
{
    ch_message_t* msg = writer->msg;
    A(msg != NULL, "Writer has no message");
    if (!(msg->type & CH_MSG_REQ_ACK)) {
        msg->_flags |= CH_MSG_ACK_RECEIVED; /* Emulate ACK */
    }
    msg->_flags |= CH_MSG_WRITE_DONE;
    writer->msg     = NULL;
    conn->timestamp = uv_now(chirp->_->loop);
    if (conn->remote != NULL) {
        conn->remote->timestamp = conn->timestamp;
    }
    ch_chirp_finish_message(chirp, conn, msg, CH_SUCCESS);
}

// .. c:function::
static void
_ch_wr_write_timeout_cb(uv_timer_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_wr_write_timeout_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = handle->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    LC(chirp, "Write timed out. ", "ch_connection_t:%p", (void*) conn);
    ch_cn_shutdown(conn, CH_TIMEOUT);
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_send(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb)
//    :noindex:
//
//    see: :c:func:`ch_wr_send`
//
// .. code-block:: cpp
//
{
    ch_chirp_check_m(chirp);
    if (chirp->_->config.SYNCHRONOUS != 0) {
        msg->type = CH_MSG_REQ_ACK;
    } else {
        msg->type = 0;
    }
    return ch_wr_send(chirp, msg, send_cb);
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_send_ts(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_send_ts`
//
// .. code-block:: cpp
//
{
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_chirp_int_t* ichirp = chirp->_;
    uv_mutex_lock(&ichirp->send_ts_queue_lock);
    if (msg->_flags & CH_MSG_USED) {
        EC(chirp, "Message already used. ", "ch_message_t:%p", (void*) msg);
        return CH_USED;
    }
    msg->_send_cb = send_cb;
    ch_msg_enqueue(&ichirp->send_ts_queue, msg);
    uv_mutex_unlock(&ichirp->send_ts_queue_lock);
    if (uv_async_send(&ichirp->send_ts) < 0) {
        E(chirp, "Could not call send_ts callback", CH_NO_ARG);
        return CH_UV_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
void
ch_wr_free(ch_writer_t* writer)
//    :noindex:
//
//    see: :c:func:`ch_wr_free`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = writer->send_timeout.data;
    uv_timer_stop(&writer->send_timeout);
    uv_close((uv_handle_t*) &writer->send_timeout, ch_cn_close_cb);
    conn->shutdown_tasks += 1;
}

// .. c:function::
ch_error_t
ch_wr_init(ch_writer_t* writer, ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_wr_init`
//
// .. code-block:: cpp
//
{

    ch_chirp_t*     chirp  = conn->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
    int             tmp_err;
    tmp_err = uv_timer_init(ichirp->loop, &writer->send_timeout);
    if (tmp_err != CH_SUCCESS) {
        EC(chirp,
           "Initializing send timeout failed: %d. ",
           "ch_connection_t:%p",
           tmp_err,
           (void*) conn);
        return CH_INIT_FAIL;
    }
    writer->send_timeout.data = conn;
    return CH_SUCCESS;
}

// .. c:function::
ch_error_t
ch_wr_process_queues(ch_remote_t* remote)
//    :noindex:
//
//    see: :c:func:`ch_wr_process_queues`
//
// .. code-block:: cpp
//
{
    AP(ch_at_allocated(remote), "Remote (%p) not allocated", (void*) remote);
    ch_chirp_t* chirp = remote->chirp;
    ch_chirp_check_m(chirp);
    ch_connection_t* conn = remote->conn;
    ch_message_t*    msg  = NULL;
    if (conn == NULL) {
        if (remote->flags & CH_RM_CONN_BLOCKED) {
            return CH_BUSY;
        } else {
            /* Only connect of the queue is not empty */
            if (remote->msg_queue != NULL || remote->cntl_msg_queue != NULL) {
                ch_error_t tmp_err;
                tmp_err = _ch_wr_connect(remote);
                if (tmp_err == CH_ENOMEM) {
                    ch_cn_abort_one_message(remote, CH_ENOMEM);
                }
                return tmp_err;
            }
        }
    } else {
        AP(ch_at_allocated(conn), "Conn (%p) not allocated", (void*) conn);
        if (!(conn->flags & CH_CN_CONNECTED) ||
            conn->flags & CH_CN_SHUTTING_DOWN) {
            return CH_BUSY;
        } else if (conn->writer.msg != NULL) {
            return CH_BUSY;
        } else if (remote->cntl_msg_queue != NULL) {
            ch_msg_dequeue(&remote->cntl_msg_queue, &msg);
            A(msg->type & CH_MSG_ACK || msg->type & CH_MSG_NOOP,
              "ACK/NOOP expected");
            ch_wr_write(conn, msg);
            return CH_SUCCESS;
        } else if (remote->msg_queue != NULL) {
            if (chirp->_->config.SYNCHRONOUS) {
                if (remote->wait_ack_message == NULL) {
                    ch_msg_dequeue(&remote->msg_queue, &msg);
                    remote->wait_ack_message = msg;
                    ch_wr_write(conn, msg);
                    return CH_SUCCESS;
                } else {
                    return CH_BUSY;
                }
            } else {
                ch_msg_dequeue(&remote->msg_queue, &msg);
                A(!(msg->type & CH_MSG_REQ_ACK), "REQ_ACK unexpected");
                ch_wr_write(conn, msg);
                return CH_SUCCESS;
            }
        }
    }
    return CH_EMPTY;
}

// .. c:function::
ch_error_t
ch_wr_send(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb)
//    :noindex:
//
//    see: :c:func:`ch_wr_send`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp = chirp->_;
    if (ichirp->flags & CH_CHIRP_CLOSING || ichirp->flags & CH_CHIRP_CLOSED) {
        if (send_cb != NULL) {
            send_cb(chirp, msg, CH_SHUTDOWN);
        }
        return CH_SHUTDOWN;
    }
    ch_remote_t  search_remote;
    ch_remote_t* remote;
    msg->_send_cb = send_cb;
    A(!(msg->_flags & CH_MSG_USED), "Message should not be used");
    A(!((msg->_flags & CH_MSG_ACK_RECEIVED) ||
        (msg->_flags & CH_MSG_WRITE_DONE)),
      "No write state should be set");
    msg->_flags |= CH_MSG_USED;
    ch_protocol_t* protocol = &ichirp->protocol;

    ch_rm_init_from_msg(chirp, &search_remote, msg, 0);
    if (ch_rm_find(protocol->remotes, &search_remote, &remote) != CH_SUCCESS) {
        remote = ch_alloc(sizeof(*remote));
        LC(chirp, "Remote allocated", "ch_remote_t:%p", remote);
        if (remote == NULL) {
            if (send_cb != NULL) {
                send_cb(chirp, msg, CH_ENOMEM);
            }
            return CH_ENOMEM;
        }
        *remote     = search_remote;
        int tmp_err = ch_rm_insert(&protocol->remotes, remote);
        A(tmp_err == 0, "Inserting remote failed");
        (void) (tmp_err);
    }
    /* Remote isn't used for 3/4 REUSE_TIME we send a probe, before the
     * acutal message */
    _ch_wr_enqeue_probe_if_needed(remote);

    int queued = 0;
    if (msg->type & CH_MSG_ACK || msg->type & CH_MSG_NOOP) {
        queued = remote->cntl_msg_queue != NULL;
        ch_msg_enqueue(&remote->cntl_msg_queue, msg);
    } else {
        queued = remote->msg_queue != NULL;
        ch_msg_enqueue(&remote->msg_queue, msg);
    }

    ch_wr_process_queues(remote);
    if (queued)
        return CH_QUEUED;
    else
        return CH_SUCCESS;
}

// .. c:function::
void
ch_wr_send_ts_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`ch_wr_send_ts_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    if (ichirp->flags & CH_CHIRP_CLOSING) {
        return;
    }
    uv_mutex_lock(&ichirp->send_ts_queue_lock);

    ch_message_t* cur;
    ch_msg_dequeue(&ichirp->send_ts_queue, &cur);
    while (cur != NULL) {
        ch_chirp_send(chirp, cur, cur->_send_cb);
        ch_msg_dequeue(&ichirp->send_ts_queue, &cur);
    }
    uv_mutex_unlock(&ichirp->send_ts_queue_lock);
}

// .. c:function::
void
ch_wr_write(ch_connection_t* conn, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`ch_wr_write`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = conn->chirp;
    ch_writer_t*    writer = &conn->writer;
    ch_remote_t*    remote = conn->remote;
    ch_chirp_int_t* ichirp = chirp->_;
    A(writer->msg == NULL, "Message should be null on new write");
    writer->msg = msg;
    int tmp_err = uv_timer_start(
            &writer->send_timeout,
            _ch_wr_write_timeout_cb,
            ichirp->config.TIMEOUT * 1000,
            0);
    if (tmp_err != CH_SUCCESS) {
        EC(chirp,
           "Starting send timeout failed: %d. ",
           "ch_connection_t:%p",
           tmp_err,
           (void*) conn);
    }

    remote->serial += 1;
    ch_sr_msg_to_buf(msg, writer->net_msg, remote->serial);
    uv_buf_t buf[3];
    buf[0].base = writer->net_msg;
    buf[0].len  = CH_SR_WIRE_MESSAGE_SIZE;
    buf[1].base = msg->header;
    buf[1].len  = msg->header_len;
    buf[2].base = msg->data;
    buf[2].len  = msg->data_len;
    ch_cn_write(conn, buf, 3, _ch_wr_write_data_cb);
}
