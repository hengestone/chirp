// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ======
// Reader
// ======
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "reader.h"
#include "chirp.h"
#include "common.h"
#include "connection.h"
#include "remote.h"
#include "util.h"
#include "writer.h"

// Declarations
// ============

// .. c:function::
static void
_ch_rd_handshake(ch_connection_t* conn, ch_buf* buf, size_t read);
//
//    Handle a handshake on the given connection.
//
//    Ensures, that the byte count which shall be read is at least the same as
//    the handshake size.
//
//    The given buffer gets copied into the handshake structure of the reader.
//    Then the port, the maximum time until a timeout happens and the remote
//    identity are applied to the given connection coming from the readers
//    handshake.
//
//    It is then ensured, that the address of the peer connected to the TCP
//    handle (TCP stream) of the connections client can be resolved.
//
//    If the latter was successful, the IP protocol and address are then applied
//    from the resolved address structure to the connection.
//
//    The given connection gets then searched on the protocols pool of
//    connections. If the connection is already known, the found duplicate gets
//    removed from the pool of connections and gets then added to another pool
//    (of the protocol), holding only such old connections.
//
//    Finally, the given connection is added to the protocols pool of
//    connections.
//
//    :param ch_connection_t* conn: Pointer to a connection instance.
//    :param ch_readert* reader:    Pointer to a reader instance. Its handshake
//                                  data structure is the target of this
//                                  handshake
//    :param ch_buf* buf:           Buffer containing bytes read, acts as data
//                                  source
//    :param size_t read:           Count of bytes read

// .. c:function::
static void
_ch_rd_handle_msg(
        ch_connection_t* conn, ch_reader_t* reader, ch_message_t* msg);
//
//    Send ack and call message-handler
//
//    :param ch_connection_t* conn:  Pointer to a connection instance.
//    :param ch_reader_t* reader:    Pointer to a reader instance.
//    :param ch_message_t* msg:      Message that was received
//

// .. c:function::
static ssize_t
_ch_rd_read_buffer(
        ch_connection_t* conn,
        ch_reader_t*     reader,
        ch_message_t*    msg,
        ch_buf*          src_buf,
        size_t           to_read,
        char**           assign_buf,
        ch_buf*          dest_buf,
        size_t           dest_buf_size,
        uint32_t         expected,
        int              free_flag,
        ssize_t*         bytes_handled);
//
//    Read data from the read buffer provided by libuv ``src_buf`` to the field
//    of the message ``assign_buf``. A preallocated buffer will be used if the
//    message is small enough, otherwise a buffer will be allocated. The
//    function also handles partial reads.

// .. c:function::
static inline ssize_t
_ch_rd_read_step(
        ch_connection_t* conn,
        ch_buf*          buf,
        size_t           bytes_read,
        ssize_t          bytes_handled,
        int*             stop,
        int*             cont);
//
//    One step in the reader state machine.
//
//    :param ch_connection_t* conn: Connection the data was read from.
//    :param void* buffer:          The buffer containing ``read`` bytes read.
//    :param size_t bytes_read:     The bytes read.
//    :param size_t bytes_handled:  The bytes handled in the last step
//    :param int* stop:             (Out) Stop the reading process.
//    :param int* cont:             (Out) Request continuation
//

// .. c:function::
static inline ch_error_t
_ch_rd_verify_msg(ch_connection_t* conn, ch_message_t* msg);
//
//    One step in the reader state machine.
//
//    :param ch_connection_t* conn: Connection the message came from
//    :param ch_message_t* msg:     Message to verify

//
// Definitions
// ===========

// .. c:type:: _ch_rd_state_names
//
//    Names of reader states for logging.
//
// .. code-block:: cpp
//
char* _ch_rd_state_names[] = {
        "CH_RD_HANDSHAKE",
        "CH_RD_WAIT",
        "CH_RD_SLOT",
        "CH_RD_HEADER",
        "CH_RD_DATA",
};

// .. c:function::
static void
_ch_rd_handshake(ch_connection_t* conn, ch_buf* buf, size_t read)
//    :noindex:
//
//    see: :c:func:`_ch_rd_handshake`
//
// .. code-block:: cpp
//
{
    ch_remote_t       search_remote;
    ch_connection_t*  old_conn = NULL;
    ch_connection_t*  tmp_conn = NULL;
    ch_chirp_t*       chirp    = conn->chirp;
    ch_remote_t*      remote   = NULL;
    ch_chirp_int_t*   ichirp   = chirp->_;
    ch_protocol_t*    protocol = &ichirp->protocol;
    ch_sr_handshake_t hs_tmp;
    uv_timer_stop(&conn->connect_timeout);
    conn->flags |= CH_CN_CONNECTED;
    if (conn->flags & CH_CN_INCOMING) {
        A(ch_cn_delete(&protocol->handshake_conns, conn, &tmp_conn) == 0,
          "Handshake should be tracked");
        A(conn == tmp_conn, "Deleted wrong connection");
    }
    if (read < CH_SR_HANDSHAKE_SIZE) {
        EC(chirp,
           "Illegal handshake size -> shutdown. ",
           "ch_connection_t:%p",
           (void*) conn);
        ch_cn_shutdown(conn, CH_PROTOCOL_ERROR);
        return;
    }
    ch_sr_buf_to_hs(buf, &hs_tmp);
    conn->port = hs_tmp.port;
    memcpy(conn->remote_identity, hs_tmp.identity, CH_ID_SIZE);
    ch_rm_init_from_conn(chirp, &search_remote, conn, 0);
    if (ch_rm_find(protocol->remotes, &search_remote, &remote) != CH_SUCCESS) {
        remote = ch_alloc(sizeof(*remote));
        LC(chirp, "Remote allocated", "ch_remote_t:%p", remote);
        if (remote == NULL) {
            ch_cn_shutdown(conn, CH_ENOMEM);
            return;
        }
        *remote     = search_remote;
        int tmp_err = ch_rm_insert(&protocol->remotes, remote);
        A(tmp_err == 0, "Inserting remote failed");
        (void) (tmp_err);
    }
    conn->remote = remote;
    /* If there is a network race condition we replace the old connection and
     * leave the old one for garbage collection. */
    old_conn     = remote->conn;
    remote->conn = conn;
    /* By definition, the connection that last completes the handshake is the
     * new one, the other the old one. Since we store outgoing connections at
     * the moment we connect, both connections might fight the place in
     * old_conns. It would be possible to prevent this, but removing the new
     * connection from old_conns is easier and just as correct. */
    ch_cn_delete(&protocol->old_connections, conn, &tmp_conn);
    if (old_conn != NULL) {
        /* If we found the current connection everything is ok */
        if (conn != old_conn) {
            L(chirp,
              "ch_connection_t:%p replaced ch_connection_t:%p",
              (void*) conn,
              (void*) old_conn);
            ch_cn_insert(&protocol->old_connections, old_conn);
        }
    }
#ifdef CH_ENABLE_LOGGING
    {
        ch_text_address_t addr;
        uint8_t*          identity = conn->remote_identity;
        char              id[CH_ID_SIZE * 2 + 1];
        uv_inet_ntop(conn->ip_protocol, conn->address, addr.data, sizeof(addr));
        ch_bytes_to_hex(identity, sizeof(identity), id, sizeof(id));
        LC(chirp,
           "Handshake with remote %s:%d (%s) done. ",
           "ch_connection_t:%p",
           addr.data,
           conn->port,
           id,
           (void*) conn);
    }
#endif
    ch_message_t* ack_msg = &conn->ack_msg;
    memcpy(ack_msg->address, conn->address, CH_IP_ADDR_SIZE);
    ack_msg->ip_protocol = conn->ip_protocol;
    ack_msg->port        = conn->port;
    ack_msg->type        = CH_MSG_ACK;
    ack_msg->header_len  = 0;
    ack_msg->data_len    = 0;
    ack_msg->_pool       = conn;
    A(conn->remote != NULL, "The remote has to be set");
    ch_wr_process_queues(conn->remote);
}

// .. c:function::
static void
_ch_rd_handle_msg(ch_connection_t* conn, ch_reader_t* reader, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`_ch_rd_handle_msg`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = conn->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
#ifdef CH_ENABLE_LOGGING
    {
        ch_text_address_t addr;
        uint8_t*          identity = conn->remote_identity;
        char              id[CH_ID_SIZE * 2 + 1];
        uv_inet_ntop(conn->ip_protocol, conn->address, addr.data, sizeof(addr));
        ch_bytes_to_hex(identity, sizeof(identity), id, sizeof(id));
        LC(chirp,
           "Read message with id: %s\n"
           "                             "
           "serial:%u\n"
           "                             "
           "from %s:%d type:%d data_len:%u. ",
           "ch_connection_t:%p",
           id,
           msg->serial,
           addr.data,
           conn->port,
           msg->type,
           msg->data_len,
           (void*) conn);
    }
#endif

    reader->state   = CH_RD_WAIT;
    reader->slot    = NULL;
    conn->timestamp = uv_now(ichirp->loop);
    if (conn->remote != NULL) {
        conn->remote->timestamp = conn->timestamp;
    }

    /* Only increase refcnt if we know ch_chirp_release_msg_slot is called */
    reader->pool->refcnt += 1;
    if (ichirp->recv_cb != NULL) {
        ichirp->recv_cb(chirp, msg);
    } else {
        ch_chirp_release_msg_slot(chirp, msg, NULL);
    }
}

static ssize_t
_ch_rd_read_buffer(
        ch_connection_t* conn,
        ch_reader_t*     reader,
        ch_message_t*    msg,
        ch_buf*          src_buf,
        size_t           to_read,
        char**           assign_buf,
        ch_buf*          dest_buf,
        size_t           dest_buf_size,
        uint32_t         expected,
        int              free_flag,
        ssize_t*         bytes_handled)
//    :noindex:
//
//    see: :c:func:`_ch_rd_read_buffer`
//
// .. code-block:: cpp
//
{
    if (reader->bytes_read == 0) {
        if (expected <= dest_buf_size) {
            /* Preallocated buf is large enough */
            *assign_buf = dest_buf;
        } else {
            *assign_buf = ch_alloc(expected);
            if (*assign_buf == NULL) {
                EC(conn->chirp,
                   "Could not allocate memory for message. ",
                   "ch_connection_t:%p",
                   (void*) conn);
                ch_cn_shutdown(conn, CH_ENOMEM);
                return CH_ENOMEM;
            }
            msg->_flags |= free_flag;
        }
    }
    if ((to_read + reader->bytes_read) >= expected) {
        /* We can read everything */
        size_t reading = expected - reader->bytes_read;
        memcpy(*assign_buf + reader->bytes_read, src_buf, reading);
        *bytes_handled += reading;
        reader->bytes_read = 0; /* Reset partial buffer reads */
        return CH_SUCCESS;
    } else {
        /* Only partial read possible */
        memcpy(*assign_buf + reader->bytes_read, src_buf, to_read);
        *bytes_handled += to_read;
        reader->bytes_read += to_read;
        return CH_MORE;
    }
}

// .. c:function::
static inline ssize_t
_ch_rd_read_step(
        ch_connection_t* conn,
        ch_buf*          buf,
        size_t           bytes_read,
        ssize_t          bytes_handled,
        int*             stop,
        int*             cont)
//    :noindex:
//
//    see: :c:func:`ch_rd_free`
//
// .. code-block:: cpp
//
{
    ch_message_t*   msg;
    ch_bf_slot_t*   slot;
    ch_chirp_t*     chirp   = conn->chirp;
    ch_chirp_int_t* ichirp  = chirp->_;
    ch_reader_t*    reader  = &conn->reader;
    int             to_read = bytes_read - bytes_handled;

    LC(chirp,
       "Reader state: %s. ",
       "ch_connection_t:%p",
       _ch_rd_state_names[reader->state],
       (void*) conn);

    switch (reader->state) {
    case CH_RD_HANDSHAKE: {
        if (bytes_read == 0)
            return -1;
        /* We expect that complete handshake arrives at once,
         * check in _ch_rd_handshake */
        _ch_rd_handshake(conn, buf + bytes_handled, to_read);
        bytes_handled += sizeof(ch_sr_handshake_t);
        reader->state = CH_RD_WAIT;
        break;
    }
    case CH_RD_WAIT: {
        if (bytes_read == 0)
            return -1;
        ch_message_t* wire_msg = &reader->wire_msg;
        ssize_t       reading  = CH_SR_WIRE_MESSAGE_SIZE - reader->bytes_read;
        if (to_read >= reading) {
            /* We can read everything */
            memcpy(reader->net_msg + reader->bytes_read,
                   buf + bytes_handled,
                   reading);
            reader->bytes_read = 0; /* Reset partial buffer reads */
            bytes_handled += reading;
        } else {
            memcpy(reader->net_msg + reader->bytes_read,
                   buf + bytes_handled,
                   to_read);
            reader->bytes_read += to_read;
            bytes_handled += to_read;
            return bytes_handled;
        }
        ch_sr_buf_to_msg(reader->net_msg, wire_msg);
        int tmp_err = _ch_rd_verify_msg(conn, wire_msg);
        if (tmp_err != CH_SUCCESS) {
            ch_cn_shutdown(conn, tmp_err);
            return -1; /* Shutdown */
        }
        if (wire_msg->type & CH_MSG_NOOP) {
            LC(chirp, "Received NOOP.", "ch_connection_t", conn);
            conn->timestamp = uv_now(ichirp->loop);
            if (conn->remote != NULL) {
                conn->remote->timestamp = conn->timestamp;
            }
            break;
        } else if (wire_msg->type & CH_MSG_ACK) {
            ch_message_t* wam = conn->remote->wait_ack_message;
            /* Since we abort wam on shutdown, we can receive acks for a old
             * wam */
            if (wam != NULL) {
                if (memcmp(wam->identity, wire_msg->identity, CH_ID_SIZE) ==
                    0) {
                    wam->_flags |= CH_MSG_ACK_RECEIVED;
                    conn->remote->wait_ack_message = NULL;
                    ch_chirp_finish_message(chirp, conn, wam, CH_SUCCESS);
                }
            }
            break;
        } else {
            reader->state = CH_RD_SLOT;
        }
        /* Since we do not read any data in CH_RD_SLOT, we need to ask for
         * continuation. I wanted to solve this with a fall-though-case, but
         * linters and compilers are complaining. */
        *cont = 1;
        break;
    }
    case CH_RD_SLOT: {
        ch_message_t* wire_msg = &reader->wire_msg;
        if (reader->slot == NULL) {
            reader->slot = ch_bf_acquire(reader->pool);
            if (reader->slot == NULL) {
                LC(chirp, "Stop reading", "ch_connection_t:%p", conn);
                if (!(conn->flags & CH_CN_STOPPED)) {
                    LC(chirp, "Stop stream", "ch_connection_t:%p", conn);
                    uv_read_stop((uv_stream_t*) &conn->client);
                }
                conn->flags |= CH_CN_STOPPED;
                *stop = 1;
                return bytes_handled;
            }
        }
        slot = reader->slot;
        msg  = &slot->msg;
        /* Copy the wire message */
        memcpy(msg, wire_msg, ((char*) &wire_msg->header) - ((char*) wire_msg));
        msg->ip_protocol = conn->ip_protocol;
        msg->port        = conn->port;
        memcpy(msg->remote_identity, conn->remote_identity, CH_ID_SIZE);
        memcpy(msg->address,
               conn->address,
               (msg->ip_protocol == AF_INET6) ? CH_IP_ADDR_SIZE
                                              : CH_IP4_ADDR_SIZE);
        if (msg->type & CH_MSG_REQ_ACK) {
            msg->_flags |= CH_MSG_SEND_ACK;
        }
        /* Direct jump to next read state */
        if (msg->header_len > 0) {
            reader->state = CH_RD_HEADER;
        } else if (msg->data_len > 0) {
            reader->state = CH_RD_DATA;
        } else {
            _ch_rd_handle_msg(conn, reader, msg);
        }
        break;
    }
    case CH_RD_HEADER: {
        if (bytes_read == 0)
            return -1;
        slot = reader->slot;
        msg  = &slot->msg;
        if (_ch_rd_read_buffer(
                    conn,
                    reader,
                    msg,
                    buf + bytes_handled,
                    to_read,
                    &msg->header,
                    slot->header,
                    CH_BF_PREALLOC_HEADER,
                    msg->header_len,
                    CH_MSG_FREE_HEADER,
                    &bytes_handled) != CH_SUCCESS) {
            return -1; /* Shutdown */
        }
        /* Direct jump to next read state */
        if (msg->data_len > 0) {
            reader->state = CH_RD_DATA;
        } else {
            _ch_rd_handle_msg(conn, reader, msg);
        }
        break;
    }
    case CH_RD_DATA: {
        if (bytes_read == 0)
            return -1;
        slot = reader->slot;
        msg  = &slot->msg;
        if (_ch_rd_read_buffer(
                    conn,
                    reader,
                    msg,
                    buf + bytes_handled,
                    to_read,
                    &msg->data,
                    slot->data,
                    CH_BF_PREALLOC_DATA,
                    msg->data_len,
                    CH_MSG_FREE_DATA,
                    &bytes_handled) != CH_SUCCESS) {
            return -1; /* Shutdown */
        }
        _ch_rd_handle_msg(conn, reader, msg);
        break;
    }
    default:
        A(0, "Unknown reader state");
        break;
    }
    return bytes_handled;
}

// .. c:function::
static inline ch_error_t
_ch_rd_verify_msg(ch_connection_t* conn, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`_ch_rd_verify_msg`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp               = conn->chirp;
    ch_chirp_int_t* ichirp              = chirp->_;
    uint32_t        total_wire_msg_size = msg->header_len + msg->data_len;
    if (total_wire_msg_size > ichirp->config.MAX_MSG_SIZE) {
        EC(chirp,
           "Message size exceeds hardlimit. ",
           "ch_connection_t:%p",
           (void*) conn);
        return CH_ENOMEM;
    }
    if ((msg->type & CH_MSG_ACK) || (msg->type & CH_MSG_NOOP)) {
        if (msg->header_len != 0 || msg->data_len != 0) {
            EC(chirp,
               "A ack/noop may not have header or data set. ",
               "ch_connection_t:%p",
               (void*) conn);
            return CH_PROTOCOL_ERROR;
        }
        if (msg->type & CH_MSG_REQ_ACK) {
            EC(chirp,
               "A ack/noop may not require an ack. ",
               "ch_connection_t:%p",
               (void*) conn);
            return CH_PROTOCOL_ERROR;
        }
    }
    return CH_SUCCESS;
}

// .. c:function::
void
ch_rd_free(ch_reader_t* reader)
//    :noindex:
//
//    see: :c:func:`ch_rd_free`
//
// .. code-block:: cpp
//
{
    /* Remove the reference to connection, since it is now invalid */
    reader->pool->conn = NULL;
    ch_bf_free(reader->pool);
}

// .. c:function::
ch_error_t
ch_rd_init(ch_reader_t* reader, ch_connection_t* conn, ch_chirp_int_t* ichirp)
//    :noindex:
//
//    see: :c:func:`ch_rd_init`
//
// .. code-block:: cpp
//
{
    reader->state = CH_RD_HANDSHAKE;
    reader->pool  = ch_alloc(sizeof(*reader->pool));
    if (reader->pool == NULL) {
        return CH_ENOMEM;
    }
    return ch_bf_init(reader->pool, conn, ichirp->config.MAX_SLOTS);
}

// .. c:function::
ssize_t
ch_rd_read(ch_connection_t* conn, ch_buf* buf, size_t bytes_read, int* stop)
//    :noindex:
//
//    see: :c:func:`ch_rd_read`
//
// .. code-block:: cpp
//
{
    *stop = 0;

    /* Bytes handled is used when multiple writes (of the remote) come in a
     * single read and the reader switches between various states as for
     * example CH_RD_HANDSHAKE, CH_RD_WAIT or CH_RD_HEADER. */
    ssize_t bytes_handled = 0;
    int     cont;

    /* Ignore reads while shutting down */
    if (conn->flags & CH_CN_SHUTTING_DOWN) {
        return bytes_read;
    }

    do {
        cont          = 0;
        bytes_handled = _ch_rd_read_step(
                conn, buf, bytes_read, bytes_handled, stop, &cont);
        if (*stop || bytes_handled == -1) {
            return bytes_handled;
        }
    } while (bytes_handled < (ssize_t) bytes_read || cont);
    return bytes_handled;
}
