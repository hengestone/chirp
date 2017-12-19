// ===========
// Util Header
// ===========
//
// Common utility functions.
//
// .. code-block:: cpp
//
#ifndef ch_util_h
#define ch_util_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"
#include "libchirp/callbacks.h"

// Declarations
// ============

// .. c:function::
void*
ch_alloc(size_t size);
//
//    Allocate fixed amount of memory.
//
//    :param size_t size: The amount of memory in bytes to allocate
//
// Debug alloc tracking
// --------------------
//
// .. code-block:: cpp

#ifndef CH_ENABLE_ASSERTS
// .. c:function::
int
ch_at_allocated(void* buf);
//
//    Check if a buffer currently is allocated. Return 1 if buffer is
//    allocated.
//
//    :param void* buf: Pointer to the buffer to track.
//
// .. c:function::
void
ch_at_init(void);
//
//    Initialize alloc tracking for memory leak debugging.
//
// .. c:function::
void
ch_at_cleanup(void);
//
//    Cleanup and print memory leak summary.
//
//
#endif

// .. c:function::
void
ch_bytes_to_hex(uint8_t* bytes, size_t bytes_size, char* str, size_t str_size);
//
//    Convert a bytes array to a hex string.
//
//    :param uint8_t* bytes:     Bytes to convert.
//    :param size_t bytes_size:  Length of the bytes to convert.
//    :param char* str:          Destination string.
//    :param size_t str_size:    Length of the buffer to write the string to.

// .. c:function::
void
ch_free(void* buf);
//
//    Free a memory handle.
//
//    :param void* buf: The handle to free.

// .. c:function::
int
ch_is_local_addr(ch_text_address_t* addr);
//
//    Check if an address is either 127.0.0.1 or ::1
//
//    :param ch_text_address_t* addr: Address to check

// .. c:function::
void
ch_random_ints_as_bytes(uint8_t* bytes, size_t len);
//
//    Fill in random ints efficiently.
//    The parameter ``len`` MUST be multiple of four.
//
//    Thank you windows, for making this really complicated.
//
//    :param uint8_t* bytes:  The buffer to fill the bytes into.
//    :param size_t  len:     The length of the buffer.

// .. c:function::
void*
ch_realloc(void* buf, size_t size);
//
//    Resize allocated memory.
//
//    :param void* buf:    The handle to resize.
//    :param size_t size:  The new size of the memory in bytes.

// .. c:function::
ch_error_t
ch_textaddr_to_sockaddr(
        int                      af,
        ch_text_address_t*       text,
        uint16_t                 port,
        struct sockaddr_storage* addr);
//
//    Convert a text address to a struct sockaddr. As an input we want struct
//    sockaddr_storage, to have enough space for an IPv4 and IPv6 address, but
//    you can cast it to struct sockaddr afterwards.
//
//    :param int af:                  Either AF_INT or AF_INET6
//    :param ch_text_address_t* text: A text representation of the address
//    :param uint16_t port:           The port
//    :param sockaddr_storage* addr:  The socket to set

#endif // ch_util_h
