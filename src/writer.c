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
_ch_wr_write_chirp_header_cb(uv_write_t* req, int status);
//
//    Callback which is called after the messages header was written.
//
//    The successful sending of a message over a connection triggers the
//    message header callback, which, in its turn, then calls this callback ---
//    if a header is present.
//
//    Cancels (void) if the sending was erroneous. Next data will be written if
//    the message has data.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

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
_ch_wr_write_msg_header_cb(uv_write_t* req, int status);
//
//    Callback which is called after the messages header was written.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

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
_ch_wr_enqeue_noop_if_needed(ch_remote_t* remote);
//
//    If remote wasn't use for 3/4 REUSE_TIME, we send a noop message
//
//    :param ch_remote_t* remote: Remote to send the noop to.

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
        return CH_FATAL;
    }

    ch_text_address_t taddr;
    uv_inet_ntop(
            remote->ip_protocol,
            remote->address,
            taddr.data,
            sizeof(taddr.data));
    if (!(ichirp->config.DISABLE_ENCRYPTION || ch_is_local_addr(&taddr))) {
        conn->flags |= CH_CN_ENCRYPTED;
    }
    memcpy(&conn->address, &remote->address, CH_IP_ADDR_SIZE);
    uv_tcp_init(ichirp->loop, &conn->client);
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
        /* Here we join the code called on accept. */
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
_ch_wr_enqeue_noop_if_needed(ch_remote_t* remote)
//    :noindex:
//
//    see: :c:func:`_ch_wr_enqeue_noop_if_needed`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = remote->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
    ch_config_t*    config = &ichirp->config;
    uint64_t        now    = uv_hrtime();
    uint64_t then = now - (1000 * 1000 * 1000 * config->REUSE_TIME / 4 * 3);
    ch_message_t* noop = remote->noop;
    if (remote->timestamp < then) {
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
        } else {
            /* The noop is not enqueued yet, enqueue it */
            if (!(noop->_flags & CH_MSG_USED) && noop->_next == NULL) {
                LC(chirp, "Sending NOOP.", "ch_remote_t:%p", remote);
                ch_msg_enqueue(&remote->cntl_msg_queue, noop);
            }
        }
    }
}

// .. c:function::
void
_ch_wr_send_ts_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_ts_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
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
static void
_ch_wr_write_chirp_header_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_write_chirp_header_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_writer_t*  writer = &conn->writer;
    ch_message_t* msg    = writer->msg;
    if (_ch_wr_check_write_error(chirp, writer, conn, status)) {
        return;
    }
    if (msg->data_len > 0) {
        ch_cn_write(conn, msg->data, msg->data_len, _ch_wr_write_data_cb);
    } else {
        _ch_wr_write_finish(chirp, writer, conn);
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
    ch_chirp_int_t* ichirp = chirp->_;
    ch_message_t*   msg    = writer->msg;
    A(msg != NULL, "Writer has no message");
    if ((ichirp->config.ACKNOWLEDGE == 0 || !(msg->type & CH_MSG_REQ_ACK))) {
        msg->_flags |= CH_MSG_ACK_RECEIVED; /* Emulate ACK */
    }
    msg->_flags |= CH_MSG_WRITE_DONE;
    writer->msg     = NULL;
    conn->timestamp = uv_hrtime();
    if (conn->remote != NULL) {
        conn->remote->timestamp = conn->timestamp;
    }
    ch_chirp_finish_message(chirp, conn, msg, CH_SUCCESS);
}

// .. c:function::
static void
_ch_wr_write_msg_header_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_write_msg_header_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn  = req->data;
    ch_chirp_t*      chirp = conn->chirp;
    ch_chirp_check_m(chirp);
    ch_writer_t*  writer = &conn->writer;
    ch_message_t* msg    = writer->msg;
    if (_ch_wr_check_write_error(chirp, writer, conn, status)) {
        return;
    }
    if (msg->header_len > 0) {
        ch_cn_write(
                conn,
                msg->header,
                msg->header_len,
                _ch_wr_write_chirp_header_cb);
    } else if (msg->data_len > 0) {
        ch_cn_write(conn, msg->data, msg->data_len, _ch_wr_write_data_cb);
    } else {
        _ch_wr_write_finish(chirp, writer, conn);
    }
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
    if (chirp->_->config.ACKNOWLEDGE != 0) {
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
    uv_async_send(&ichirp->send_ts);
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
    A(ch_at_allocated(remote), "Remote not allocated");
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
                return _ch_wr_connect(remote);
            }
        }
    } else {
        A(ch_at_allocated(conn), "Conn not allocated");
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
            if (chirp->_->config.ACKNOWLEDGE) {
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
    remote->serial += 1;
    msg->serial = remote->serial;
    /* Remote isn't used for 3/4 REUSE_TIME we send a noop */
    _ch_wr_enqeue_noop_if_needed(remote);

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

    ch_sr_msg_to_buf(msg, writer->net_msg);
    ch_cn_write(
            conn,
            writer->net_msg,
            CH_SR_WIRE_MESSAGE_SIZE,
            _ch_wr_write_msg_header_cb);
}
