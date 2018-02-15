// =======
// Message
// =======
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "message.h"
#include "util.h"

// Definitions
// ===========

// Queue definition
// ----------------
//

qs_queue_bind_impl_cx_m(ch_msg, ch_message_t) CH_ALLOW_NL;

// Interface definitions
// ---------------------

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_get_address(const ch_message_t* message, ch_text_address_t* address)
//    :noindex:
//
//    see: :c:func:`ch_msg_get_address`
//
// .. code-block:: cpp
//
{
    int ip_protocol = message->ip_protocol;
    if (!(ip_protocol == AF_INET || ip_protocol == AF_INET6)) {
        return CH_VALUE_ERROR;
    }
    int tmp_err = uv_inet_ntop(
            ip_protocol,
            message->address,
            address->data,
            sizeof(address->data));
    /* This error is not dynamic, it means ch_text_address_t is too small, so
     * we do not return it to the user */
    A(tmp_err == 0, "Cannot convert address to text (not enough space)");
    (void) (tmp_err);
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
ch_identity_t
ch_msg_get_identity(ch_message_t* message)
//    :noindex:
//
//    see: :c:func:`ch_msg_get_identity`
//
// .. code-block:: cpp
//
{
    ch_identity_t id;
    memcpy(id.data, message->identity, sizeof(id.data));
    return id;
}

// .. c:function::
CH_EXPORT
ch_identity_t
ch_msg_get_remote_identity(ch_message_t* message)
//    :noindex:
//
//    see: :c:func:`ch_msg_get_remote_identity`
//
// .. code-block:: cpp
//
{
    ch_identity_t id;
    memcpy(id.data, message->remote_identity, sizeof(id.data));
    return id;
}

// .. c:function::
CH_EXPORT
int
ch_msg_has_recv_handler(ch_message_t* message)
//    :noindex:
//
//    see: :c:func:`ch_msg_has_recv_handler`
//
// .. code-block:: cpp
//
{
    return message->_flags & CH_MSG_IS_HANDLER;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_init(ch_message_t* message)
//    :noindex:
//
//    see: :c:func:`ch_msg_init`
//
// .. code-block:: cpp
//
{
    memset(message, 0, sizeof(*message));
    ch_random_ints_as_bytes(message->identity, sizeof(message->identity));
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_set_address(
        ch_message_t*    message,
        ch_ip_protocol_t ip_protocol,
        const char*      address,
        int32_t          port)
//    :noindex:
//
//    see: :c:func:`ch_msg_set_address`
//
// .. code-block:: cpp
//
{
    message->ip_protocol = ip_protocol;
    if (!(ip_protocol == AF_INET || ip_protocol == AF_INET6)) {
        return CH_VALUE_ERROR;
    }
    if (uv_inet_pton(ip_protocol, address, message->address)) {
        return CH_VALUE_ERROR;
    }
    message->port = port;
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
void
ch_msg_set_data(ch_message_t* message, ch_buf* data, uint32_t len)
//    :noindex:
//
//    see: :c:func:`ch_msg_set_data`
//
// .. code-block:: cpp
//
{
    message->data     = data;
    message->data_len = len;
}
