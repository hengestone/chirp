// =============
// Remote header
// =============
//
// ch_remote_t represents a remote node.
//
// .. code-block:: cpp
//
#ifndef ch_remote_h
#define ch_remote_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"
#include "connection.h"
#include "rbtree.h"

// Declarations
// ============

// .. c:type:: ch_remote_t
//
//    ch_remote_t represents remote node and is a dictionary used to lookup
//    remotes.
//
//    .. c:member:: uint8_t ip_protocol
//
//       What IP protocol (IPv4 or IPv6) shall be used for connections.
//
//    .. c:member:: uint8_t[16] address
//
//       IPv4/6 address of the sender if the message was received.  IPv4/6
//       address of the recipient if the message is going to be sent.
//
//    .. c:member:: int32_t port
//
//       The port that shall be used for connections.
//
//    .. c:member:: ch_connection_t* conn
//
//       The active connection to this remote. Can be NULL. Callbacks always
//       have to check if the connection is NULL. The code that sets the
//       connection to NULL has to notify the user. So callbacks can safely
//       abort if conn is NULL.
//
//    .. c:member:: ch_message_t* no_rack_msg_queue
//
//       Queue of messages that don't require an ACK.
//
//    .. c:member:: ch_message_t* rack_msg_queue
//
//       Queue of messages that require an ACK.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Pointer to the chirp object. See: :c:type:`ch_chirp_t`.
//
//    .. c:member:: uint32_t serial
//
//       The current serial number for this remote
//
//    .. c:member:: char color
//
//       rbtree member
//
//    .. c:member:: ch_remote_t* left
//
//       rbtree member
//
//    .. c:member:: ch_remote_t* right
//
//       rbtree member
//
//    .. c:member:: ch_remote_t* parent
//
//       rbtree member
//
// .. code-block:: cpp
//
struct ch_remote_s {
    uint8_t          ip_protocol;
    uint8_t          address[CH_IP_ADDR_SIZE];
    int32_t          port;
    ch_connection_t* conn;
    ch_message_t*    no_rack_msg_queue;
    ch_message_t*    rack_msg_queue;
    ch_message_t*    wait_ack_message;
    ch_chirp_t*      chirp;
    uint32_t         serial;
    char             color;
    ch_remote_t*     parent;
    ch_remote_t*     left;
    ch_remote_t*     right;
};

// rbtree Prototypes
// ------------------
//
// .. code-block:: cpp
//
#define ch_rm_cmp_m(x, y) ch_remote_cmp(x, y)

// .. code-block:: cpp
//
rb_bind_decl_m(ch_rm, ch_remote_t) CH_ALLOW_NL;

        // .. c:function::
        void ch_rm_init_from_msg(
                ch_chirp_t* chirp, ch_remote_t* remote, ch_message_t* msg);
//
//    Initialize the remote data-structure from a message.
//
//    :param ch_remote_t* remote: Remote to initialize
//    :param ch_message_t* msg:   Message to initialize from

// .. c:function::
void
ch_rm_init_from_conn(
        ch_chirp_t* chirp, ch_remote_t* remote, ch_connection_t* conn);
//
//    Initialize the remote data-structure from a connection.
//
//    :param ch_remote_t* remote: Remote to initialize
//    :param ch_connection_t*:   Connection to initialize from

#endif // ch_remote_h
