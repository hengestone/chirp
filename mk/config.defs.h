// ======
// Config
// ======
//
// .. code-block:: cpp

#ifndef ch_global_config_h
#define ch_global_config_h

// .. c:macro:: CH_VERSION
//
//    Version of chirp.
//
// .. code-block:: cpp
//
#define CH_VERSION "XVERSIONX"

// .. c:macro:: CH_TLS_CLEANUP
//
//    In release-mode we do not cleanup the TLS library. If you want to check
//    for memory leaks in your application define CH_TLS_CLEANUP or call
//    :c:func:`ch_en_tls_cleanup` manually.
//
// .. code-block:: cpp

/* #define CH_TLS_CLEANUP */

// .. c:macro:: CH_BUFFER_SIZE
//
//    Buffer-size when allocating communication buffers, can be overridden in
//    :c:type:`ch_config_t`.
//
// .. code-block:: cpp

/* 64k */
#define CH_BUFFER_SIZE 65536

// .. c:macro:: CH_MIN_BUFFER_SIZE
//
//    Minimal buffersize we require when allocating communication buffers.
//
// .. code-block:: cpp

#define CH_MIN_BUFFER_SIZE 1024

// .. c:macro:: CH_ENC_BUFFER_SIZE
//
//    Encryption buffer size. Only change if it doesn't match with your TLS
//    library.
//
// .. code-block:: cpp

/* 16k */
#define CH_ENC_BUFFER_SIZE 16384

// .. c:macro:: CH_BF_PREALLOC_HEADER
//
//    Preallocated buffer size for header. If the header is bigger memory will
//    be allocated.
//
// .. code-block:: cpp

#define CH_BF_PREALLOC_HEADER 32

// .. c:macro:: CH_BF_PREALLOC_DATA
//
//    Preallocated buffer size for data. If the data is bigger memory will be
//    allocated.
//
// .. code-block:: cpp

#define CH_BF_PREALLOC_DATA 1024

// .. c:macro:: CH_TCP_KEEPALIVE
//
//    TCP keep-alive time.
//
// .. code-block:: cpp

#define CH_TCP_KEEPALIVE 60

// .. c:macro:: CH_MAX_MSG_SIZE
//
//    Hard limit for message size.  Can be overridden in :c:type:`ch_config_t`.
//
// .. code-block:: cpp

#define CH_MAX_MSG_SIZE 1024 * 1024 * 100 // 100M

// .. c:macro:: CH_WITHOUT_TLS
//
//    Build chirp without SSL code. The functions from encryption.h become
//    no-ops.
//
// .. code-block:: cpp

/* #define CH_WITHOUT_TLS */

// .. c:macro:: CH_ENABLE_LOGGING
//
//    Logging is done via a macro which is disabled by default. You can enable
//    it here for debug purposes.
//
// .. code-block:: cpp

/* #define CH_ENABLE_LOGGING */

// .. c:macro:: CH_ENABLE_ASSERTS
//
//    Chirp asserts a lot of conditions. Asserting is done via a macro which is
//    disabled by default. You can enable it here for debug purposes.
//
// .. code-block:: cpp

/* #define CH_ENABLE_ASSERTS */

// .. c:macro:: CH_CN_PRINT_CIPHERS
//
//    Log available ciphers. Used to debug connections failures.
//
// .. code-block:: cpp

/* #define CH_CN_PRINT_CIPHERS */

// .. c:macro:: CH_DISABLE_SIGNALS
//
//    Disable SIGINT and SIGTERM to close chirp. Used for compatibility with
//    existing applications: When you use chirp in an existing application that
//    uses SIGINT and SIGTERM signals and the handlers added by chirp somehow
//    conflict. Can be overridden in :c:type:`ch_config_t`.
//
// .. code-block:: cpp

/* #define CH_DISABLE_SIGNALS */

#ifndef CH_REMOVE_DEBUGDEFS

// Only used for testing, ignore if you only build the library
//
// .. code-block:: cpp

/* SDS allocator selection.
 *
 * This file is used in order to change the SDS allocator at compile time.
 * Just define the following defines to what you want to use. Also add
 * the include of your alternate allocator if needed (not needed in order
 * to use the default libc allocator).
 */

#define s_malloc malloc
#define s_realloc realloc
#define s_free free

#endif // CH_REMOVE_DEBUGDEFS

#endif // ch_global_config_h
