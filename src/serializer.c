// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ==========
// Serializer
// ==========
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "serializer.h"

// Definitions
// ===========
//
// .. code-block:: cpp

// .. c:function::
int
ch_sr_buf_to_msg(ch_buf* buf, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`ch_sr_buf_to_msg`
//
// .. code-block:: cpp
//
{
    CH_SR_WIRE_MESSAGE_LAYOUT;

    memcpy(msg->identity, identity, CH_ID_SIZE);

    msg->type       = *type;
    msg->header_len = ntohs(*header_len);
    msg->data_len   = ntohl(*data_len);
    msg->serial     = ntohl(*serial);
    return CH_SR_WIRE_MESSAGE_SIZE;
}

// .. c:function::
int
ch_sr_msg_to_buf(ch_message_t* msg, ch_buf* buf, uint32_t msg_serial)
//    :noindex:
//
//    see: :c:func:`ch_sr_msg_to_buf`
//
// .. code-block:: cpp
//
{
    CH_SR_WIRE_MESSAGE_LAYOUT;

    memcpy(identity, msg->identity, CH_ID_SIZE);

    *type       = msg->type;
    *header_len = htons(msg->header_len);
    *data_len   = htonl(msg->data_len);
    *serial     = htonl(msg_serial);
    return CH_SR_WIRE_MESSAGE_SIZE;
}

// .. c:function::
int
ch_sr_buf_to_hs(ch_buf* buf, ch_sr_handshake_t* hs)
//    :noindex:
//
//    see: :c:func:`ch_sr_buf_to_hs`
//
// .. code-block:: cpp
//
{
    CH_SR_HANDSHAKE_LAYOUT;

    hs->port = ntohs(*port);
    memcpy(hs->identity, identity, CH_ID_SIZE);

    return CH_SR_HANDSHAKE_SIZE;
}

// .. c:function::
int
ch_sr_hs_to_buf(ch_sr_handshake_t* hs, ch_buf* buf)
//    :noindex:
//
//    see: :c:func:`ch_sr_hs_to_buf`
//
// .. code-block:: cpp
//

{
    CH_SR_HANDSHAKE_LAYOUT;

    *port = htons(hs->port);
    memcpy(identity, hs->identity, CH_ID_SIZE);

    return CH_SR_HANDSHAKE_SIZE;
}
