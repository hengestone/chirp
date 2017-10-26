// ==============
// Message header
// ==============
//
// .. todo:: Document purpose
//
// .. code-block:: cpp
//
#ifndef ch_msg_message_h
#define ch_msg_message_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp/message.h"
#include "qs.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else // _WIN32
#include <arpa/inet.h>
#endif // _WIN32

// Queue declarations
// ==================
//
// .. code-block:: cpp
//
#define ch_msg_next_m(x) (x)->_next
qs_queue_bind_decl_cx_m(ch_msg, ch_message_t)


// .. c:type:: ch_msg_type_t
//
//    Represents message type flags.
//
//    .. c:member:: CH_MSG_REQ_ACK
//
//       Message requires ack.
//
//    .. c:member:: CH_MSG_ACK
//
//       Message is an ack.
//
// .. code-block:: cpp
//
typedef enum {
    CH_MSG_REQ_ACK = 1 << 0,
    CH_MSG_ACK     = 1 << 1,
} ch_msg_types_t;

// .. c:type:: ch_msg_flags_t
//
//    Represents message flags.
//
//    .. c:member:: CH_MSG_FREE_HEADER
//
//       Header data has to be freed before releasing the buffer
//
//    .. c:member:: CH_MSG_FREE_DATA
//
//       Data has to be freed before releasing the buffer
//
//    .. c:member:: CH_MSG_USED
//
//       The message is used by chirp
//
//    .. c:member:: CH_MSG_ACK_RECEIVED
//
//       Writer has received ACK.
//
//    .. c:member:: CH_MSG_WRITE_DONE
//
//       Write is done (last callback has been called).
//
//    .. c:member:: CH_MSG_FAILURE
//
//       On failure we still want to finish the message, therefore failure is
//       CH_MSG_ACK_RECEIVED || CH_MSG_WRITE_DONE.
//
//    .. c:member:: CH_MSG_IS_HANDLER
//
//       The message is handler. Used to check that the user only releases
//       handler messages.
//
// .. code-block:: cpp
//
typedef enum {
    CH_MSG_FREE_HEADER  = 1 << 0,
    CH_MSG_FREE_DATA    = 1 << 1,
    CH_MSG_USED         = 1 << 2,
    CH_MSG_ACK_RECEIVED = 1 << 3,
    CH_MSG_WRITE_DONE   = 1 << 4,
    CH_MSG_FAILURE      = CH_MSG_ACK_RECEIVED | CH_MSG_WRITE_DONE,
    CH_MSG_IS_HANDLER   = 1 << 5,
} ch_msg_flags_t;

#endif //ch_msg_message_h
