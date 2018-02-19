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
//       The identity of the message. Never change the identity of message.
//
//    .. c:member:: uint32_t serial
//
//       The serial number of the message. Represents to order they are sent.
//       Be aware of overflows.
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
//       Header of the message defined as (char-) buffer. Used by upper-layer
//       protocols. Users should not use it, except you know what you are
//       doing.
//
//    .. c:member:: ch_buf* data
//
//       The data of the message as pointer to a buffer. ch_buf* is an alias
//       for char* and denotes to binary buffer: the length has to be supplied
//       (data_len). Please use :c:func:`ch_msg_set_data` to set this.
//
//    .. c:member:: uint8_t ip_protocol
//
//       The IP protocol which was / shall be used for this message. This may
//       either be IPv4 or IPv6. See :c:type:`ch_ip_protocol_t`.
//
//    .. c:member:: uint8_t[16] address
//
//       IPv4/6 address of the sender if the message was received. IPv4/6
//       address of the recipient if the message is going to be sent.
//
//    .. c:member:: int32_t port
//
//       The port that the will be used reading/writing a message over a
//       connection.
//
//    .. c:member:: uint8_t remote_identity[CH_ID_SIZE]
//
//       Used to detect the remote instance. By default the remote_identity
//       will change on each start of chirp. If multiple peers share state,
//       a change in the remote_identity should trigger a reset of the state.
//       Simply use the remote_identity as key in a dictionary of shared state.
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
    uint8_t       ip_protocol;
    uint8_t       address[CH_IP_ADDR_SIZE]; // 16
    int32_t       port;
    uint8_t       remote_identity[CH_ID_SIZE];
    void*         user_data;
    uint8_t       _flags;
    ch_send_cb_t  _send_cb;
    uint8_t       _handler;
    void*         _pool;
    ch_message_t* _next;
};

// Protocol receiver /Pseudo code/
//
// .. code-block:: cpp
//
//    ch_message_t msg;
//    recv_wait(buffer=&msg, size=39)
//    if(msg.header_len) {
//        msg.header = malloc(msg.header_len) *
//        recv_exactly(buffer=msg.header, msg.header_len)
//    }
//    if(msg.data_len) {
//        msg.data  = malloc(msg.data_len) *
//        recv_exactly(buffer=msg.data, msg.data_len)
//    }
//
// * Please use MAX_HANDLERS preallocated buffers of size 32 for header
// * Please use MAX_HANDLERS preallocated buffers of size 512 for data
//
// Either fields may exceed the limit, in which case you have to alloc and set
// the free_* field.

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
ch_msg_has_recv_handler(ch_message_t* message);
//
//    Returns 1 if the message has a recv handler and therefore you have to
//    call ch_chirp_release_message.
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
