// =================
// Serializer header
// =================
//
// * Convert the wire message to a buffer in network order
// * Convert a buffer in network order to the wire message
//
// Wire message refers to a part of ch_message_t before the ch_buf* header
// member.
//
// * Convert ch_sr_handshake_t to a buffer in network order
// * Convert a buffer in network order to ch_sr_handshake_t
//
// .. code-block:: cpp

#ifndef ch_serializer_h
#define ch_serializer_h

// Project includes
// ================
//
// .. code-block:: cpp

#include "common.h"
#include "libchirp/message.h"

// Declarations
// ============

// .. c:type:: ch_sr_handshake_t
//
//    Handshake data structure.
//
//    .. c:member:: uint16_t port
//
//       Public port which is passed to a connection on a successful handshake.
//
//    .. c:member:: uint8_t[16] identity
//
//       The identity of the remote target which is passed to a connection upon
//       a successful handshake. It is used by the connection for getting the
//       remote address.
//
// .. code-block:: cpp

typedef struct ch_sr_handshake_s {
    uint16_t port;
    uint8_t  identity[CH_ID_SIZE];
} ch_sr_handshake_t;

#define CH_SR_WIRE_MESSAGE_SIZE 27

#ifdef CH_ENABLE_ASSERTS
#define CH_SR_WIRE_MESSAGE_CHECK                                               \
    pos += 4;                                                                  \
    A(pos == CH_SR_WIRE_MESSAGE_SIZE, "Bad message serialization size");
#else
#define CH_SR_WIRE_MESSAGE_CHECK
#endif

#define CH_SR_WIRE_MESSAGE_LAYOUT                                              \
    size_t   pos      = 0;                                                     \
    uint8_t* identity = (void*) &buf[pos];                                     \
    pos += CH_ID_SIZE;                                                         \
                                                                               \
    uint32_t* serial = (void*) &buf[pos];                                      \
    pos += 4;                                                                  \
                                                                               \
    uint8_t* type = (void*) &buf[pos];                                         \
    pos += 1;                                                                  \
                                                                               \
    uint16_t* header_len = (void*) &buf[pos];                                  \
    pos += 2;                                                                  \
                                                                               \
    uint32_t* data_len = (void*) &buf[pos];                                    \
    CH_SR_WIRE_MESSAGE_CHECK

// .. c:function::
int
ch_sr_buf_to_msg(ch_buf* buf, ch_message_t* msg);
//
//    Convert a buffer containing the packed data of the wire message in network
//    order to an ch_message_t.
//
//    :param ch_buf* buf: Pointer to the packed buffer of at least
//                        CH_SR_WIRE_MESSAGE_SIZE
//    :param ch_message_t* msg: Pointer to the message

// .. c:function::
int
ch_sr_msg_to_buf(ch_message_t* msg, ch_buf* buf);
//
//    Convert a ch_message_t to a buffer containing the packed data of
//    the wire message in network order.
//
//    :param ch_message_t* msg: Pointer to the message
//    :param ch_buf* buf: Pointer to the packed buffer of at least
//                        CH_SR_WIRE_MESSAGE_SIZE
//
// .. code-block:: cpp

#define CH_SR_HANDSHAKE_SIZE 18

#ifdef CH_ENABLE_ASSERTS
#define CH_SR_HANDSHAKE_CHECK                                                  \
    pos += CH_ID_SIZE;                                                         \
    A(pos == CH_SR_HANDSHAKE_SIZE, "Bad handshake serialization size");
#else
#define CH_SR_HANDSHAKE_CHECK
#endif

#define CH_SR_HANDSHAKE_LAYOUT                                                 \
    size_t    pos  = 0;                                                        \
    uint16_t* port = (void*) &buf[pos];                                        \
    pos += 2;                                                                  \
                                                                               \
    uint8_t* identity = (void*) &buf[pos];                                     \
    CH_SR_HANDSHAKE_CHECK

// .. c:function::
int
ch_sr_buf_to_hs(ch_buf* buf, ch_sr_handshake_t* hs);
//
//    Convert a buffer containing the packed data of a handshake in network
//    order to an ch_sr_handshake_t.
//
//    :param ch_buf* buf: Pointer to the packed buffer of at least
//                        CH_SR_HANDSHAKE_SIZE
//    :param ch_sr_handshake_t: Pointer to the handshake struct

// .. c:function::
int
ch_sr_hs_to_buf(ch_sr_handshake_t* hs, ch_buf* buf);
//
//    Convert a ch_sr_handshake_t to  a buffer containing the packed data of
//    the handshake in network order.
//
//    :param ch_sr_handshake_t: Pointer to the handshake struct
//    :param ch_buf* buf: Pointer to the packed buffer of at least
//                        CH_SR_HANDSHAKE_SIZE
//
// .. code-block:: cpp
//
#endif // ch_serializer_h
