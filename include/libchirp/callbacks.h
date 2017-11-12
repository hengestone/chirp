// ===============
// Chirp Callbacks
// ===============
//
// If you are on an embedded platform you have to set the memory functions of
// chirp, libuv and openssl.
//
// * ch_set_allocators
// * uv_replace_allocator_
// * CRYPTO_set_mem_functions
//
// .. _uv_replace_allocator: http://docs.libuv.org/en/v1.x/misc.html
// .. _CRYPTO_set_mem_functions: https://www.openssl.org/docs/man1.1.0/crypto/OPENSSL_malloc.html
//
// .. code-block:: cpp

#ifndef ch_libchirp_callbacks_h
#define ch_libchirp_callbacks_h

#include "common.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <stdlib.h>

// .. c:type:: ch_alloc_cb_t
//
//    Callback used by chirp to request memory.
//
//    .. c:member:: size_t size
//
//       The size to allocate
//
// .. code-block:: cpp
//
typedef void* (*ch_alloc_cb_t)(size_t size);

// .. c:type:: ch_done_cb_t
//
//    Callback called when chirp has closed.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Chirp object closed.
//
// .. code-block:: cpp
//
typedef void (*ch_done_cb_t)(ch_chirp_t* chirp);

// .. c:type:: ch_free_cb_t
//
//    Callback used by chirp to free memory.
//
// .. code-block:: cpp
//
typedef void (*ch_free_cb_t)(void* buf);

// .. c:type:: ch_log_cb_t
//
//    Logging callback
//
//    .. c:member:: char msg[]
//
//       The message to log
//
//    .. c:member:: char error
//
//       The message is a error
//
// .. code-block:: cpp

typedef void (*ch_log_cb_t)(char msg[], char error);

// .. c:type:: ch_send_cb_t
//
//    Called by chirp when message is sent and can be freed.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Chirp object sending
//
//    .. c:member:: int status
//
//       Error code: CH_SUCCESS, CH_TIMEOUT, CH_CANNOT_CONNECT TODO update
//
// .. code-block:: cpp
//
typedef void (*ch_send_cb_t)(
        ch_chirp_t* chirp,
        ch_message_t* msg,
        int status
);

// .. c:type:: ch_recv_cb_t
//
//    Called by chirp when message is received.
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Chirp object received
//
//    .. c:member:: ch_message_t* msg
//
//       Received message. The address is the remote address, so changing only
//       the user_data and send it, will send the message back to the sender.
//
// .. code-block:: cpp
//
typedef void (*ch_recv_cb_t)(ch_chirp_t* chirp, ch_message_t* msg);

// .. c:type:: ch_start_cb_t
//
//    Callback called when chirp is started
//
//    .. c:member:: ch_chirp_t* chirp
//
//       Chirp object started
//
// .. code-block:: cpp
//
typedef void (*ch_start_cb_t)(ch_chirp_t* chirp);

// .. c:type:: ch_realloc_cb_t
//
//    Callback used by chirp to request memory reallocation.
//
//    .. c:member:: void* buf
//
//       The Buffer to reallocate
//
//    .. c:member:: size_t new_size
//
//       The requested new size
//
// .. code-block:: cpp
//
typedef void* (*ch_realloc_cb_t)(void* buf, size_t new_size);

#endif //ch_libchirp_callbacks_h
