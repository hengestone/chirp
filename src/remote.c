// ======
// Remote
// ======
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "remote.h"
#include "chirp.h"
#include "util.h"

// rbtree prototypes
// =================
//
// .. c:function::
static int
ch_remote_cmp(ch_remote_t* x, ch_remote_t* y)
//
//    Compare operator for connections.
//
//    :param ch_remote_t* x: First remote instance to compare
//    :param ch_remote_t* y: Second remote instance to compare
//
//    :return: the comparision between
//                 - the IP protocols, if they are not the same, or
//                 - the addresses, if they are not the same, or
//                 - the ports
//    :rtype: int
//
// .. code-block:: cpp
//
{
    if (x->ip_protocol != y->ip_protocol) {
        return x->ip_protocol - y->ip_protocol;
    } else {
        int tmp_cmp =
                memcmp(x->address,
                       y->address,
                       x->ip_protocol == AF_INET6 ? CH_IP_ADDR_SIZE
                                                  : CH_IP4_ADDR_SIZE);
        if (tmp_cmp != 0) {
            return tmp_cmp;
        } else {
            return x->port - y->port;
        }
    }
}

rb_bind_impl_m(ch_rm, ch_remote_t) CH_ALLOW_NL;

// stack prototypes
// ================
//
// .. code-block:: cpp

qs_stack_bind_impl_m(ch_rm_st, ch_remote_t) CH_ALLOW_NL;

// Definitions
// ===========
//
// .. code-block:: cpp

static void
_ch_rm_init(ch_chirp_t* chirp, ch_remote_t* remote)
//
//    Initialize remote
//
// .. code-block:: cpp
//
{
    memset(remote, 0, sizeof(*remote));
    ch_rm_node_init(remote);
    remote->chirp = chirp;
    ch_random_ints_as_bytes((uint8_t*) &remote->serial, sizeof(remote->serial));
}

// .. c:function::
void
ch_rm_init_from_msg(ch_chirp_t* chirp, ch_remote_t* remote, ch_message_t* msg)
//    :noindex:
//
//    see: :c:func:`ch_rm_init_from_msg`
//
// .. code-block:: cpp
//
{
    _ch_rm_init(chirp, remote);
    remote->ip_protocol = msg->ip_protocol;
    remote->port        = msg->port;
    memcpy(&remote->address, &msg->address, CH_IP_ADDR_SIZE);
    remote->conn = NULL;
}

// .. c:function::
void
ch_rm_init_from_conn(
        ch_chirp_t* chirp, ch_remote_t* remote, ch_connection_t* conn)
//    :noindex:
//
//    see: :c:func:`ch_rm_init_from_conn`
//
// .. code-block:: cpp
//
{
    _ch_rm_init(chirp, remote);
    remote->ip_protocol = conn->ip_protocol;
    remote->port        = conn->port;
    memcpy(&remote->address, &conn->address, CH_IP_ADDR_SIZE);
    remote->conn = NULL;
}
