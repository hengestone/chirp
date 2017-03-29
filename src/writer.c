// ======
// Writer
// ======
//
// Chirp protocol writer.
// .. todo:: Document purpose
//
// .. code-block:: cpp

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "writer.h"
#include "chirp.h"
#include "protocol.h"
#include "util.h"

// Declarations
// ============

// .. c:function::
static
ch_inline
int
_ch_wr_check_send_error(
    ch_chirp_t* chirp,
    ch_writer_t* writer,
    ch_connection_t* conn,
    int status
);
//
//    Check if the given status is erroneous (that is, there was an error
//    during writing) and cleanup if necessary. Cleaning up means shutting down
//    the given connection instance, stopping the timer for the send-timeout,
//    unlocking the writer mutex and sending a protocol error
//    :c:member:`ch_error_t.CH_PROTOCOL_ERROR` callback over the connections
//    load.
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
static
void _ch_wr_close_failed_conn_cb(uv_handle_t* handle);
//
//    Called by libuv when failed connection is close.
//
//    :param uv_handle_t* handle: The libuv handle holding the
//                                connection


// .. c:function::
void
ch_wr_connect_cb(uv_connect_t* req, int status);
//
//    Called by libuv after trying to connect. Contains the connection status.
//
//    :param uv_connect_t* req: Connect request, containing the connection.
//    :param int status:        Status of the connection.
//

// .. c:function::
static
void
_ch_wr_send_actor_cb(uv_write_t* req, int status);
//
//    Callback which is called after an actor was written.
//
//    This callback is called, when (message-) data is being sent over a
//    connection. Upon successful sending the (message-) data, the message
//    header callback is called.
//
//    The message header callback writes the message header and calls then the
//    chirp header callback, which, in its turn, calls this callback when an
//    actor was present and written. If a message has no header, the message
//    header callback writes the actor directly --- if a such is present ---
//    and then calls this callback.
//
//    Cancels (void) if the sending was erroneous. If the sending was
//    successful and the writers message contains data, the data is being
//    written.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

// .. c:function::
static
void
_ch_wr_send_chirp_header_cb(uv_write_t* req, int status);
//
//    Callback which is called after the messages header was written.
//
//    The successful sending of a message over a connection triggers the
//    message header callback, which, in its turn, then calls this callback ---
//    if a header is present.
//
//    Cancels (void) if the sending was erroneous. If the sending was
//    successful an actor is being written, if the writers message has an actor
//    set. Otherwise, if the writers message has data set, the data is being
//    written. If no actor and no data is set, sending is being finished.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

// .. c:function::
static
void
_ch_wr_send_data_cb(uv_write_t* req, int status);
//
//    Callback which is called after data was written.
//
//    This callback is called after the successful write of either an actor or
//    a message header.
//
//    Cancels (void) if the writing was erroneous, finishes sending otherwise.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

// .. c:function::
static
ch_inline
void
_ch_wr_send_finish(
    ch_chirp_t* chirp,
    ch_writer_t* writer,
    ch_connection_t* conn
);
//
//    Sends a protocol error over the connections load when acknowledging of
//    messages is disable. Does nothing if not.
//
//    If acknowledging is disabled (by configuration), the writers send-timeout
//    timer is being stopped, the writers mutex is unlocked and a protocol
//    error :c:member:`ch_error_t.CH_PROTOCOL_ERROR` callback over the
//    connections load is being sent.
//
//    :param ch_chirp_t* chirp:      Pointer to a chirp instance.
//    :param ch_writer_t* writer:    Pointer to a writer instance.
//    :param ch_connection_t* conn:  Pointer to a connection instance.

// .. c:function::
static
void
_ch_wr_send_msg_header_cb(uv_write_t* req, int status);
//
//    Callback which is called after the messages was successfully written.
//
//    Cancels (void) if the writing was erroneous, finishes sending otherwise.
//    If the message (coming from the writer) has a header set, the header is
//    written. Otherwise, if the message has an actor set, the actor is being
//    written. Else, if the message has data attached, that data is being
//    written. In all other cases the sending is being finished using
//    :c:func:`_ch_wr_send_finish`.
//
//    :param uv_write_t* req:  Write request.
//    :param int status:       Write status.

// .. c:function::
static
void
_ch_wr_send_timeout_cb(uv_timer_t* handle);
//
//    Callback which is called after the writer reaches its timeout for
//    sending. The timeout is set by the chirp configuration and is 5 seconds
//    by default. When this callback is called, the connection is being shut
//    down, the writers mutex is being unlocked and a timeout error
//    :c:member:`ch_error_t.CH_TIMEOUT` is being sent over the connections
//    load.
//
//    :param uv_timer_t* handle: Pointer to a timer handle to schedule
//                               callback.

// Definitions
// ===========

// .. c:function::
static
ch_inline
int
_ch_wr_check_send_error(
    ch_chirp_t* chirp,
    ch_writer_t* writer,
    ch_connection_t* conn,
    int status
)
//    :noindex:
//
//    see: :c:func:`_ch_wr_check_send_error`
//
// .. code-block:: cpp
//
{
    if(status != CH_SUCCESS) {
        L(
            chirp,
            "Write failed with uv status: %d. ch_chirp_t:%p, "
            "ch_connection_t:%p",
            status,
            (void*) chirp,
            (void*) conn
        );
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        uv_timer_stop(&writer->send_timeout);
        return CH_PROTOCOL_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
static
void _ch_wr_close_failed_conn_cb(uv_handle_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_wr_close_failed_conn_cb`
//
// .. code-block:: cpp
//
{
    ch_message_t* msg = handle->data;
    ch_chirp_t* chirp = msg->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_connection_t* conn = msg->_conn;
    ch_free(conn);
    if(msg->_send_cb) {
        /* The user may free the message in the cb. */
        ch_send_cb_t cb = msg->_send_cb;
        msg->_send_cb = NULL;
        cb(msg, CH_CANNOT_CONNECT, -1);
    }
}

// .. c:function::
void
ch_wr_connect_cb(uv_connect_t* req, int status)
//    :noindex:
//
//    see: :c:func:`ch_wr_connect_cb`
//
// .. code-block:: cpp
//
{
    ch_text_address_t taddr;
    ch_message_t* msg = req->data;
    ch_chirp_t* chirp = msg->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_connection_t* conn = msg->_conn;
    ch_msg_get_address(msg, &taddr);
    if(status == CH_SUCCESS) {
        L(
            chirp,
            "Connected to remote %s:%d. ch_chirp_t:%p, ch_connection_t:%p",
            taddr.data,
            msg->port,
            (void*) chirp,
            (void*) conn
        );
        /* Here we join the code called on accept. */
        conn->writer.msg = msg;
        conn->writer.send_msg = ch_wr_send;
        ch_pr_conn_start(chirp, conn, &conn->client, 0);
    } else {
        E(
            chirp,
            "Connection to remote failed %s:%d (%d). ch_chirp_t:%p, "
            "ch_connection_t:%p",
            taddr.data,
            msg->port,
            status,
            (void*) chirp,
            (void*) conn
        );
        conn->client.data = msg;
        uv_close((uv_handle_t*) &conn->client, _ch_wr_close_failed_conn_cb);
    }
}

// .. c:function::
void
ch_wr_send(ch_connection_t* conn, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send`
//
// .. code-block:: cpp
//
{
    //int tmp_err;

    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    //ch_chirp_int_t* ichirp = chirp->_;
    ch_writer_t* writer = &conn->writer;
    /* Use the writers net message structure to write the actual message over
     * the connection. The net message structure is of type
     * :c:type:`ch_msg_message_t`, which is actually :c:macro:`CH_WIRE_MESSAGE`.
     * The difference between ``msg`` and ``net_msg`` is, that ``msg`` is of
     * type :c:type:`ch_message_t` and ``net_msg`` of type
     * :c:macro:`CH_WIRE_MESSAGE`. That means ``net_msg`` is stripped down to
     * essentially only the identity, the serial number, the message type and
     * the lengths of the header, the actor and the data.
     */
    ch_msg_message_t* net_msg = &writer->net_msg;
    writer->msg = msg;
    // TODO reenable timeout
    /*tmp_err = uv_timer_start(
        &writer->send_timeout,
        _ch_wr_send_timeout_cb,
        ichirp->config.TIMEOUT * 1000,
        0
    );
    if(tmp_err != CH_SUCCESS) {
        E(
            chirp,
            "Starting send timeout failed: %d. ch_connection_t:%p,"
            " ch_chirp_t:%p",
            tmp_err,
            (void*) conn,
            (void*) chirp
        );
    }*/
    memcpy(
        net_msg->serial,
        msg->serial,
        sizeof(net_msg->serial)
    );
    memcpy(
        net_msg->identity,
        msg->identity,
        sizeof(net_msg->identity)
    );
    net_msg->message_type = msg->message_type;
    net_msg->header_len   = htons(msg->header_len);
    net_msg->actor_len    = htons(msg->actor_len);
    net_msg->data_len     = htonl(msg->data_len);
    ch_cn_write(
        conn,
        net_msg,
        sizeof(ch_msg_message_t),
        _ch_wr_send_msg_header_cb
    );
}

// .. c:function::
static
void
_ch_wr_send_actor_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_actor_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = req->data;
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_writer_t* writer = &conn->writer;
    ch_message_t* msg = writer->msg;
    if(_ch_wr_check_send_error(chirp, writer, conn, status)) return;
    if(msg->data_len > 0)
        ch_cn_write(
            conn,
            msg->data,
            msg->data_len,
            _ch_wr_send_data_cb
        );
    else
        _ch_wr_send_finish(chirp, writer, conn);
}

// .. c:function::
static
void
_ch_wr_send_data_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_data_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = req->data;
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_writer_t* writer = &conn->writer;
    if(_ch_wr_check_send_error(chirp, writer, conn, status)) return;
    _ch_wr_send_finish(chirp, writer, conn);
}

// .. c:function::
static
ch_inline
void
_ch_wr_send_finish(
    ch_chirp_t* chirp,
    ch_writer_t* writer,
    ch_connection_t* conn
)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_finish`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp  = chirp->_;
    if(ichirp->config.ACKNOWLEDGE == 0) {
        uv_timer_stop(&writer->send_timeout);
        ch_message_t* msg = writer->msg;
        A(msg != NULL, "Writer has no message");
        writer->msg = NULL;
        if(msg->_send_cb) {
            /* The user may free the message in the cb. */
            ch_send_cb_t cb = msg->_send_cb;
            msg->_send_cb = NULL;
            cb(
                writer->msg,
                CH_SUCCESS,
                conn->load
            );
        }
    }
}

// .. c:function::
static
void
_ch_wr_send_chirp_header_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_chirp_header_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = req->data;
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_writer_t* writer = &conn->writer;
    ch_message_t* msg = writer->msg;
    if(_ch_wr_check_send_error(chirp, writer, conn, status)) return;
    if(msg->actor_len > 0)
        ch_cn_write(
            conn,
            msg->actor,
            msg->actor_len,
            _ch_wr_send_actor_cb
        );
    else if(msg->data_len > 0)
        ch_cn_write(
            conn,
            msg->data,
            msg->data_len,
            _ch_wr_send_data_cb
        );
    else
        _ch_wr_send_finish(chirp, writer, conn);
}

// .. c:function::
static
void
_ch_wr_send_msg_header_cb(uv_write_t* req, int status)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_msg_header_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = req->data;
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_writer_t* writer = &conn->writer;
    ch_message_t* msg = writer->msg;
    if(_ch_wr_check_send_error(chirp, writer, conn, status)) return;
    if(msg->header_len > 0)
        ch_cn_write(
            conn,
            msg->header,
            msg->header_len,
            _ch_wr_send_chirp_header_cb
        );
    else if(msg->actor_len > 0)
        ch_cn_write(
            conn,
            msg->actor,
            msg->actor_len,
            _ch_wr_send_actor_cb
        );
    else if(msg->data_len > 0)
        ch_cn_write(
            conn,
            msg->data,
            msg->data_len,
            _ch_wr_send_data_cb
        );
    else
        _ch_wr_send_finish(chirp, writer, conn);
}

// .. c:function::
static
void
_ch_wr_send_timeout_cb(uv_timer_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_wr_send_timeout_cb`
//
// .. code-block:: cpp
//
{
    ch_connection_t* conn = handle->data;
    ch_writer_t* writer = &conn->writer;
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    L(
        chirp,
        "Write timed out. ch_chirp_t:%p, ch_connection_t:%p",
        (void*) chirp,
        (void*) conn
    );
    ch_cn_shutdown(conn, CH_TIMEOUT);
    uv_timer_stop(&writer->send_timeout);
}

//
// .. c:function::
CH_EXPORT
void
ch_chirp_send(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_send`
//
// .. code-block:: cpp
//
{
    int tmp_err;
    ch_connection_t search_conn;
    ch_connection_t* conn;
    msg->_send_cb     = send_cb;
    msg->message_type = CH_MSG_REQ_ACK;

    /* TODO
     * 1. Create ichirp->_connecting_port, _connecting_addr, _message_queue
     * 2. If connecting_port is not zero queue the message by port/addr key
     * 3. Of course cleanup and reset port on connect_cb
     */

    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_chirp_int_t* ichirp  = chirp->_;
    ch_protocol_t* protocol = &ichirp->protocol;
    search_conn.ip_protocol = msg->ip_protocol;
    search_conn.port        = msg->port;
    memcpy(
        search_conn.address,
        msg->address,
        msg->ip_protocol == CH_IPV6 ? CH_IP_ADDR_SIZE : CH_IP4_ADDR_SIZE
    );
    conn = sglib_ch_connection_t_find_member(
        protocol->connections,
        &search_conn
    );
    ch_random_ints_as_bytes(msg->serial, sizeof(msg->serial));
    if(conn == NULL) {
        conn = ch_alloc(sizeof(ch_connection_t));
        if(!conn) {
            E(
                chirp,
                "Could not allocate memory for connection. ch_chirp_t:%p",
                (void*) chirp
            );
            if(send_cb != NULL)
                send_cb(msg, CH_ENOMEM, -1);
            return;
        }
        memset(conn, 0, sizeof(ch_connection_t));
        msg->_conn         = conn;
        conn->port         = msg->port;
        conn->ip_protocol  = msg->ip_protocol;
        conn->connect.data = msg;
        ch_text_address_t taddr;
        ch_msg_get_address(msg, &taddr);
        if(!(
                ichirp->config.DISABLE_ENCRYPTION  ||
                ch_is_local_addr(&taddr)
        )) {
            conn->flags |= CH_CN_ENCRYPTED;
        }
        memcpy(
            &conn->address,
            &msg->address,
            CH_IP_ADDR_SIZE
        );
        uv_tcp_init(ichirp->loop, &conn->client);
        if(msg->ip_protocol == CH_IPV6) {
            struct sockaddr_in6 addr;
            uv_ip6_addr(taddr.data, msg->port, &addr);
            tmp_err = uv_tcp_connect(
                &conn->connect,
                &conn->client,
                (struct sockaddr*) &addr,
                ch_wr_connect_cb
            );
        } else {
            A(msg->ip_protocol == CH_IPV4, "Unknown IP protocol");
            struct sockaddr_in addr;
            uv_ip4_addr(taddr.data, msg->port, &addr);
            tmp_err = uv_tcp_connect(
                &conn->connect,
                &conn->client,
                (struct sockaddr*) &addr,
                ch_wr_connect_cb
            );
        }
        L(
            chirp,
            "Connecting to remote %s:%d. ch_chirp_t:%p, ch_connection_t:%p",
            taddr.data,
            msg->port,
            (void*) chirp,
            (void*) conn
        );
        if(tmp_err != CH_SUCCESS) {
            E(
                chirp,
                "Failed to connect to host: %s:%d (%d). ch_chirp_t:%p",
                taddr.data,
                msg->port,
                tmp_err,
                (void*) chirp
            );
        }
    } else
        ch_wr_send(conn, msg);
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
    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    uv_close((uv_handle_t*) &writer->send_timeout, ch_cn_close_cb);
    conn->shutdown_tasks += 1;
}

// .. c:function::
void
ch_wr_init(ch_writer_t* writer, ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_wr_init`
//
// .. code-block:: cpp
//
{
    int tmp_err;

    ch_chirp_t* chirp = conn->chirp;
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_chirp_int_t* ichirp = chirp->_;
    tmp_err = uv_timer_init(ichirp->loop, &writer->send_timeout);
    if(tmp_err != CH_SUCCESS) {
        E(
            chirp,
            "Initializing send timeout failed: %d. ch_chirp_t:%p",
            tmp_err,
            (void*) chirp
        );
    }
    writer->send_timeout.data = conn;
}
