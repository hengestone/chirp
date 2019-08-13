// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =============
// Writer Header
// =============
//
// Chirp protocol writer. Everything about putting a message on the wire.
//
// .. code-block:: cpp
//
#ifndef ch_writer_h
#define ch_writer_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"
#include "libchirp/callbacks.h"
#include "libchirp/message.h"
#include "serializer.h"

// Declarations
// ============
//
// Direct declarations
// -------------------
//
// .. c:type:: ch_writer_t
//
//    The chirp protocol writer data strcture.
//
//    .. c:member:: ch_send_cb_t send_cb
//
//       Callback that will be called upon successful sending of a message.
//
//    .. c:member:: uv_timer_t send_timeout
//
//       Libuv timer handle for setting a timeout when trying to send a
//       message. At the end of the defined timeout time, the timer triggers
//       the :c:func:`_ch_wr_write_timeout_cb` callback.
//
//    .. c:member:: ch_message_t* msg
//
//       Pointer to a message. The message is set when sending through
//       :c:func:`ch_wr_write` and being read again during the callbacks.
//
//    .. c:member:: ch_msg_message_t net_msg
//
//       Used to serialize the wire message to.
//
// .. code-block:: cpp
//
typedef struct ch_writer_s {
    uv_timer_t    send_timeout;
    ch_message_t* msg;
    ch_buf        net_msg[CH_SR_WIRE_MESSAGE_SIZE];
} ch_writer_t;

// .. c:function::
void
ch_wr_free(ch_writer_t* writer);
//
//    Free the writer data structure.
//
//    :param ch_writer_t* writer: Pointer to writer (data-) structure.
//

// .. c:function::
ch_error_t
ch_wr_init(ch_writer_t* writer, ch_connection_t* conn);
//
//    Initialize the writer data structure.
//
//    Initializes the writers mutex and the libuv timer for handling timeouts
//    when sending. The connection gets set as data pointer for the sending
//    timeout.
//
//    :param ch_chirp_t* chirp:      Pointer to a chirp instance.
//    :param ch_connection_t* conn:  Pointer to a connection instance.

// .. c:function::
ch_error_t
ch_wr_process_queues(ch_remote_t* remote);
//
//    Sends queued message according to the following priorities:
//
//    1. Do nothing if the writer still has an active message
//    2. Send messages that don't require an ack (which might be acks)
//    3. Send messages that require an ack
//    4. Do nothing
//
//    :param ch_remote_t* remote: The remote to process queues

// .. c:function::
ch_error_t
ch_wr_send(ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb);
//    Same as ch_chirp_send just for internal use.
//
//    see: :c:func:`ch_chirp_send`
//

// .. c:function::
void
ch_wr_send_ts_cb(uv_async_t* handle);
//
//    Send all messages in the message queue.
//
//    :param uv_async_t* handle: Async handler used to trigger sending message
//                               queue.

// .. c:function::
void
ch_wr_write(ch_connection_t* conn, ch_message_t* msg);
//
//    Send the message after a connection has been established.
//
//    :param ch_connection_t* conn:  Connection to send the message over.
//    :param ch_message_t msg:       The message to send. The memory of the
//                                   message must stay valid until the callback
//                                   is called.

#endif // ch_writer_h
