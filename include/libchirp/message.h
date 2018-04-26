// =======
// Message
// =======
//
// The message structure, initialization and setting/getting address.
//
// .. code-block:: cpp
//
#ifndef ch_libchirp_message_h
#define ch_libchirp_message_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "callbacks.h"
#include "chirp.h"
#include "common.h"

// Declarations
// ============

// .. c:type:: ch_message_t
//
//    Represents a message. Although many members are not marked as private.
//    Use the APIs to access the message. Never change the identity of a
//    message.
//
//    .. c:member:: uint8_t[16] identity
//
//       Identify the message and answers to it. Never change the identity of
//       message. The identity can be used to find answers to a message, since
//       replying to message won't change the identity.
//
//       If you need to uniquely identify the message, use the identity/serial
//       pair, since the serial will change when replying to messages.
//
//    .. c:member:: uint32_t serial
//
//       The serial number of the message. Increases monotonic. Be aware of
//       overflows, if want to use it for ordering use the delta: serialA -
//       serialB. Only received messages have a serial. But also received
//       messages can have the serial 0.
//
//    .. c:member:: uint8_t type
//
//       The type of the message.
//
//    .. c:member:: uint16_t header_len
//
//       Length of the message header.
//
//    .. c:member:: uint32_t data_len
//
//       Length of the data the message contains.
//
//    .. c:member:: ch_buf* header
//
//       Header used by upper-layer protocols. Users should not use it, except
//       if you know what you are doing. ch_buf* is an alias for char* and
//       denotes to binary buffer: the length has to be supplied (header_len)
//
//    .. c:member:: ch_buf* data
//
//       The data of the message as pointer to a buffer. ch_buf* is an alias
//       for char* and denotes to binary buffer: the length has to be supplied
//       (data_len). Please use :c:func:`ch_msg_set_data` to set this.
//
//    .. c:member:: uint8_t ip_protocol
//
//       The IP protocol which was / shall be used for this message.
//
//       If the message was received: The protocol of the remote the message
//       was received from.
//
//       If the message will be sent: The protocol to send the message to.
//
//       This allows to reply to messages just by replacing the data
//       :c:func:`ch_msg_set_data`.
//
//       This may either be IPv4 or IPv6. See :c:type:`ch_ip_protocol_t`.
//       Please use :c:func:`ch_msg_set_address` to set this.
//
//    .. c:member:: uint8_t[16] address
//
//       IPv4/6 address.
//
//       If the message was received: The address of the remote the message was
//       received from.
//
//       If the message will be sent: The address to send the message to.
//
//       This allows to reply to messages just by replacing the data
//       :c:func:`ch_msg_set_data`.
//
//       Please use :c:func:`ch_msg_set_address` to set and
//       :c:func:`ch_msg_get_address` to get the address.
//
//    .. c:member:: int32_t port
//
//       The port.
//
//       If the message was received: The port of the remote the message was
//       received from.
//
//       If the message will be sent: The port to send the message to.
//
//       This allows to reply to messages just by replacing the data
//       :c:func:`ch_msg_set_data`.
//
//       Please use :c:func:`ch_msg_set_address` to set the port.
//
//    .. c:member:: uint8_t remote_identity[CH_ID_SIZE]
//
//       Detect the remote instance. By default a node's identity will change
//       on each start of chirp. If multiple peers share state, a change in the
//       remote_identity should trigger a reset of the state. Simply use the
//       remote_identity as key in a dictionary of shared state.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Pointer to chirp instance of the message pool.
//
//    .. c:member:: void* user_data
//
//       Pointer to user data, can be used to access user-data in the
//       send-callback.
//
// .. code-block:: cpp
//
struct ch_message_s {
    // Network data, has to be sent in network order
    uint8_t  identity[CH_ID_SIZE];
    uint32_t serial;
    uint8_t  type;
    uint16_t header_len;
    uint32_t data_len;
    // These fields follow the message in this order (see *_len above)
    ch_buf* header;
    ch_buf* data;
    // Local       only data
    uint8_t         ip_protocol;
    uint8_t         address[CH_IP_ADDR_SIZE]; // 16
    int32_t         port;
    uint8_t         remote_identity[CH_ID_SIZE];
    void*           user_data;
    uint8_t         _flags;
    ch_send_cb_t    _send_cb;
    ch_release_cb_t _release_cb;
    uint8_t         _slot;
    void*           _pool;
    void*           _ssl_context;
    ch_message_t*   _next;
};

// IMPORTANT: The wire-message layout is different from the message layout.
// Because we don't want to break the ABI and the message's format (if packed)
// would be unaligned. See serializer.h

// .. c:function::
CH_EXPORT
void
ch_msg_free_data(ch_message_t* message);
//
//    Frees data attached to the message. This function is mostly intended to
//    implement bindings, where you can to copy the data into memory provided
//    by the host language, so it can freely garbage-collect the data.
//
//    After calling this the data/header fields will be NULL.
//
//    :param ch_message_t* message: Pointer to the message

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_get_address(const ch_message_t* message, ch_text_address_t* address);
//
//    Get the messages' address which is an IP-address. The port and
//    ip_protocol can be read directly from the message. Address must be of the
//    size INET(6)_ADDRSTRLEN.
//
//    :param ch_message_t* message: Pointer to the message
//    :param ch_text_address_t* address: Textual representation of IP-address
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:function::
CH_EXPORT
ch_identity_t
ch_msg_get_identity(ch_message_t* message);
//
//    Get the identity of the message.
//
//    :param ch_message_t* message: Pointer to the message
//
//    :rtype:  ch_identity_t

// .. c:function::
CH_EXPORT
ch_identity_t
ch_msg_get_remote_identity(ch_message_t* message);
//
//    Get the identity of the remote chirp instance.
//
//    By default the remote_identity will change on each start of chirp. If
//    multiple peers share state, a change in the remote_identity should
//    trigger a reset of the state. Simply use the remote_identity as key in a
//    dictionary of shared state.
//
//    :param ch_message_t* message: Pointer to the message
//
//    :rtype:  ch_identity_t

// .. c:function::
CH_EXPORT
int
ch_msg_has_slot(ch_message_t* message);
//
//    Returns 1 if the message has a slot and therefore you have to call
//    ch_chirp_release_msg_slot.
//
//    :param ch_message_t* message: Pointer to the message
//
//    :rtype:  bool

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_init(ch_message_t* message);
//
//    Initialize a message. Memory provided by caller (for performance).
//
//    :param ch_message_t* message: Pointer to the message
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:function::
CH_EXPORT
ch_error_t
ch_msg_set_address(
        ch_message_t*    message,
        ch_ip_protocol_t ip_protocol,
        const char*      address,
        int32_t          port);
//
//    Set the messages' address in terms of IP-address and port.
//
//    :param ch_message_t* message: Pointer to the message
//    :param ch_ip_protocol_t ip_protocol: IP-protocol of the address
//    :param char* address: Textual representation of IP
//    :param int32_t port: Port of the remote
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:function::
CH_EXPORT
void
ch_msg_set_data(ch_message_t* message, ch_buf* data, uint32_t len);
//
//    Set the messages' data. ch_buf* is an alias for char* and denotes to a
//    binary buffer: the length has to be supplied. The data pointer has to be
//    valid until the :c:type:`ch_send_cb_t` supplied in
//    :c:func:`ch_chirp_send` has been called.
//
//    :param ch_message_t* message: Pointer to the message
//    :param ch_buf* data: Pointer to the data
//    :param uint32_t len: The length of the data
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. code-block:: cpp
//
#endif // ch_libchirp_message_h
