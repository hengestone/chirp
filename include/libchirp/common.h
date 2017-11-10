// =============
// Common header
// =============
//
// .. code-block:: cpp
//
#ifndef ch_libchirp_common_h
#define ch_libchirp_common_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp/const.h"
#include "libchirp/error.h"

// Library export
// ==============
//
// .. code-block:: cpp
//
#ifdef CH_BUILD
#   if defined __GNUC__ || __clang__
#       define CH_EXPORT __attribute__ ((visibility ("default")))
#   endif
#else // CH_BUILD
#   define CH_EXPORT
#endif

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <uv.h>
#include <stdio.h>
#include <stdint.h>


// Typedefs
// ========
//
// .. code-block:: cpp

typedef char ch_buf; /* Used to show it is not a c-string but a buffer. */

// .. c:type:: ch_text_address_t
//
//    Type to be used with :c:func:`ch_msg_get_address`. Used for the textual
//    representation of the IP-address.
//
// .. code-block:: cpp
//
typedef struct ch_text_address_s {
    char data[INET6_ADDRSTRLEN];
} ch_text_address_t;


// Forward declarations
// =====================
//
// .. code-block:: cpp

struct ch_chirp_s;
typedef struct ch_chirp_s ch_chirp_t;
struct ch_config_s;
typedef struct ch_config_s ch_config_t;
struct ch_message_s;
typedef struct ch_message_s ch_message_t;

// Logging
// =======

// .. c:macro:: CH_WRITE_LOG
//
//    Logs the given message including arbitrary arguments to a custom callback
//    in debug-/development-mode.
//
//    The logging macro CH_WRITE_LOG(chirp, message, ...) behaves like printf
//    and allows to log to a custom callback. Usually used to log into pythons
//    logging facility.
//
//    The logging macro CH_WRITE_LOGC(chirp, message, clear, ...) behaves like
//    CH_WRITE_LOG except you can add part that isn't highlighted.
//
//    :param chirp:   Pointer to a chirp object.
//    :param message: The highlighted message to report.
//    :param clear:   The clear message (not highlighted to report.
//    :param ...:     Variadic arguments for xprintf
//
// .. code-block:: cpp
//
#define CH_WRITE_LOGC(chirp, message, clear, ...) \
    ch_write_log(chirp, __FILE__, __LINE__, message, clear, 0, __VA_ARGS__);
#define CH_WRITE_LOG(chirp, message, ...) \
    CH_WRITE_LOGC(chirp, message, "",  __VA_ARGS__)

#endif //ch_libchirp_common_h
