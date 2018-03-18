//
// .. code-block:: cpp

#include "libchirp-config.h"
#ifndef CH_WITHOUT_TLS

// =================
// Encryption header
// =================
//
// Setting up and cleaning up TLS.
//
// .. code-block:: cpp

#ifndef ch_encryption_h
#define ch_encryption_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp/chirp.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <openssl/ssl.h>
#include <string.h>

// Determine OpenSSL API
// =====================
//
// .. code-block:: cpp

#ifdef LIBRESSL_VERSION_NUMBER
#define CH_OPENSSL_10_API
#define CH_LIBRESSL
#else
#define CH_OPENSSL
#if (OPENSSL_VERSION_NUMBER <= 0x10100000L)
#define CH_OPENSSL_10_API
#endif
#endif

// Declarations
// ============

// .. c:type:: ch_en_tls_ops_t
//
//    Represents TLS operations.
//
//    .. c:member:: CH_EN_OP_HANDSHAKE
//
//       Continue with handshake
//
//    .. c:member:: CH_EN_OP_READ
//
//       Read data from remote
//
//    .. c:member:: CH_EN_OP_WRITE
//
//       Write data to remote
//
//    .. c:member:: CH_EN_OP_SHUTDOWN
//
//       Continue with shutdown
//
// .. code-block:: cpp
//
typedef enum {
    CH_EN_OP_HANDSHAKE = 0,
    CH_EN_OP_READ      = 1,
    CH_EN_OP_WRITE     = 2,
    CH_EN_OP_SHUTDOWN  = 3,
} ch_en_tls_ops_t;

// .. c:type:: ch_encryption_t
//
//    Encryption object.
//
//    .. c:member:: ch_chirp_t*
//
//       reference back to chirp
//
// .. code-block:: cpp
//
typedef struct ch_encryption_s {
    ch_chirp_t* chirp;
    SSL_CTX*    ssl_ctx;
} ch_encryption_t;

// .. c:function::
ch_error_t
ch_en_start(ch_encryption_t* enc);
//
//    Start the encryption.
//
//    :param ch_encryption_t* enc: Pointer to a encryption object (holding chirp
//                                 and OpenSSL context)
//
//   :return: A chirp error. see: :c:type:`ch_error_t`
//   :rtype:  ch_error_t

// .. c:function::
ch_error_t
ch_en_stop(ch_encryption_t* enc);
//
//    Stop the encryption.
//
//    :param ch_encryption_t* enc: pointer to a encryption object (holding chirp
//                                 and openssl context)
//
//   :return: A chirp error. see: :c:type:`ch_error_t`
//   :rtype:  ch_error_t

// Definitions
// ===========

// .. c:function::
void
ch_en_init(ch_chirp_t* chirp, ch_encryption_t* enc);
//
//    Initialize the encryption struct.
//
//    :param ch_chirp_t* chirp: Chirp instance
//    :param ch_encryption_t* enc: Encryption to initialize

#endif // ch_encryption_h
#endif // CH_WITHOUT_TLS
