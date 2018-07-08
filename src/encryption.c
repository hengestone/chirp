// ==========
// Encryption
// ==========
//

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "encryption.h"
#include "chirp.h"
#include "common.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#ifndef CH_WITHOUT_TLS
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/engine.h>
#include <openssl/err.h>

// Declarations
// ============

// .. c:var:: _ch_en_manual_tls
//
//    The user will call ch_en_tls_init() and ch_en_tls_cleanup().
//    Defaults to 0.
//
// .. code-block:: cpp
//
static char _ch_en_manual_tls = 0;

#ifdef CH_OPENSSL_10_API
// .. c:var:: _ch_en_lock_count
//
//    The count of locks created for openssl.
//
// .. code-block:: cpp
//
static int _ch_en_lock_count = 0;

// .. c:var:: _ch_en_lock_list
//
//    List of locks provided for openssl. Openssl will tell us how many locks
//    it needs.
//
// .. code-block:: cpp
//
static uv_rwlock_t* _ch_en_lock_list = NULL;

// .. c:function::
static void
_ch_en_locking_function(int mode, int n, const char* file, int line);
//
//    Called by openssl to lock a mutex.
//
//    :param int mode: Can be CRYPTO_LOCK or CRYPTO_UNLOCK
//    :param int n: The lock to lock/unlock
//    :param const char* file: File the function was called from (deubbing)
//    :param const char* line: Line the function was called from (deubbing)
//

// .. c:function::
static unsigned long
_ch_en_thread_id_function(void);
//
//    Called by openssl to get the current thread it.
//
#endif // CH_OPENSSL_10_API

// Definitions
// ===========
//
// .. c:function::
void
ch_en_init(ch_chirp_t* chirp, ch_encryption_t* enc)
//    :noindex:
//
//    see: :c:func:`ch_en_init`
//
// .. code-block:: cpp
//
{
    memset(enc, 0, sizeof(*enc));
    enc->chirp = chirp;
}

#ifdef CH_OPENSSL_10_API
// .. c:function::
static void
_ch_en_locking_function(int mode, int n, const char* file, int line)
//    :noindex:
//
//    see: :c:func:`_ch_en_locking_function`
//
// .. code-block:: cpp
//
{
    (void) (file);
    (void) (line);
    if (mode & CRYPTO_LOCK) {
        if (!(mode & CRYPTO_READ)) { /* The user requested write */
            uv_rwlock_wrlock(&_ch_en_lock_list[n]);
        } else if (!(mode & CRYPTO_WRITE)) { /* The user requested read */
            uv_rwlock_rdlock(&_ch_en_lock_list[n]);
        } else { /* The user request is bad, do a wrlock for safety */
            uv_rwlock_wrlock(&_ch_en_lock_list[n]);
        }
    } else {
        if (!(mode & CRYPTO_READ)) {
            uv_rwlock_wrunlock(&_ch_en_lock_list[n]);
        } else if (!(mode & CRYPTO_WRITE)) {
            uv_rwlock_rdunlock(&_ch_en_lock_list[n]);
        } else {
            uv_rwlock_wrunlock(&_ch_en_lock_list[n]);
        }
    }
}
#endif // CH_OPENSSL_10_API
#endif // CH_WITHOUT_TLS

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_init(void)
//    :noindex:
//
//    see: :c:func:`ch_en_tls_init`
//
// .. code-block:: cpp
//
{
#ifndef CH_WITHOUT_TLS
    if (_ch_en_manual_tls) {
        return CH_SUCCESS;
    }
#ifdef CH_OPENSSL_10_API
    OPENSSL_add_all_algorithms_noconf();
    SSL_load_error_strings();
    SSL_library_init();

    /* Check if the host program already initialized threading. The
     * initialization above only adds things and doesn't change anything, but
     * threading setup would change the lock-callback and bad things would
     * happen. */
    if (CRYPTO_get_locking_callback() != NULL) {
        return CH_SUCCESS;
    }
    return ch_en_tls_threading_setup();
#else
    if (OPENSSL_init_ssl(0, NULL) == 0) {
        return CH_TLS_ERROR;
    }
    return CH_SUCCESS;
#endif
#else  // CH_WITHOUT_TLS
    return CH_SUCCESS;
#endif // CH_WITHOUT_TLS
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_cleanup(void)
//    :noindex:
//
//    see: :c:func:`ch_en_tls_cleanup`
//
// .. code-block:: cpp
//
{
#ifndef CH_WITHOUT_TLS
    if (_ch_en_manual_tls) {
        return CH_SUCCESS;
    }

#ifdef CH_OPENSSL
    FIPS_mode_set(0);
#endif
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    CONF_modules_free();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
#ifdef CH_OPENSSL_10_API
    CRYPTO_THREADID id;
    CRYPTO_THREADID_current(&id);
    ERR_remove_thread_state(&id);
    ERR_remove_thread_state(NULL);
#endif
    ASN1_STRING_TABLE_cleanup();

    return ch_en_tls_threading_cleanup();
#else // CH_WITHOUT_TLS
    return CH_SUCCESS;
#endif
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_threading_cleanup(void)
//    :noindex:
//
//    see: :c:func:`ch_en_tls_threading_cleanup`
//
// .. code-block:: cpp
//
{
#ifndef CH_WITHOUT_TLS
#ifdef CH_OPENSSL_10_API
    A(_ch_en_lock_list, "Threading not setup");
    if (!_ch_en_lock_list) {
        fprintf(stderr,
                "%s:%d Fatal: Threading not setup.\n",
                __FILE__,
                __LINE__);
        return CH_VALUE_ERROR;
    }
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for (int i = 0; i < _ch_en_lock_count; i++) {
        uv_rwlock_destroy(&_ch_en_lock_list[i]);
    }
    ch_free(_ch_en_lock_list);
    _ch_en_lock_list = NULL;
#else
    OPENSSL_thread_stop();
#endif // CH_OPENSSL_10_API
#endif // CH_WITHOUT_TLS
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_threading_setup(void)
//    :noindex:
//
//    see: :c:func:`ch_en_tls_threading_setup`
//
// .. code-block:: cpp
//
{
#ifndef CH_WITHOUT_TLS
#ifdef CH_OPENSSL_10_API
    A(!_ch_en_lock_list, "Threading already setup");
    if (_ch_en_lock_list) {
        fprintf(stderr,
                "%s:%d Fatal: Threading already setup.\n",
                __FILE__,
                __LINE__);
        return CH_VALUE_ERROR;
    }
    int lock_count   = CRYPTO_num_locks();
    _ch_en_lock_list = ch_alloc(lock_count * sizeof(uv_rwlock_t));
    if (!_ch_en_lock_list) {
        fprintf(stderr,
                "%s:%d Fatal: Could not allocate memory for locking.\n",
                __FILE__,
                __LINE__);
        return CH_ENOMEM;
    }
    _ch_en_lock_count = lock_count;
    for (int i = 0; i < lock_count; i++) {
        if (uv_rwlock_init(&_ch_en_lock_list[i]) < 0) {
            ch_free(_ch_en_lock_list);
            return CH_INIT_FAIL;
        }
    }
    CRYPTO_set_id_callback(_ch_en_thread_id_function);
    CRYPTO_set_locking_callback(_ch_en_locking_function);
#endif // CH_OPENSSL_10_API
#endif // CH_WITHOUT_TLS
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
void
ch_en_set_manual_tls_init(void)
//    :noindex:
//
//    see: :c:func:`ch_en_set_manual_tls_init`
//
// .. code-block:: cpp
//
{
#ifndef CH_WITHOUT_TLS
    _ch_en_manual_tls = 1;
#endif
}

#ifndef CH_WITHOUT_TLS
// .. c:function::
ch_error_t
ch_en_start(ch_encryption_t* enc)
//    :noindex:
//
//    see: :c:func:`ch_en_start`
//
// .. code-block:: cpp
//
{
    ch_chirp_t*     chirp  = enc->chirp;
    ch_chirp_int_t* ichirp = chirp->_;
#ifdef CH_OPENSSL_10_API
    const SSL_METHOD* method = TLSv1_2_method();
#else
    const SSL_METHOD* method = TLS_method();
#endif
    if (method == NULL) {
        E(chirp, "Could not get the TLSv1_2_method", CH_NO_ARG);
        return CH_TLS_ERROR;
    }
    enc->ssl_ctx = SSL_CTX_new(method);
    if (enc->ssl_ctx == NULL) {
        E(chirp, "Could create the SSL_CTX", CH_NO_ARG);
        return CH_TLS_ERROR;
    }
    SSL_CTX_set_mode(
            enc->ssl_ctx, SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_CTX_set_options(enc->ssl_ctx, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_verify(
            enc->ssl_ctx,
            SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
            NULL);
#ifndef CH_OPENSSL_10_API // NOT DEF!
    SSL_CTX_set_min_proto_version(enc->ssl_ctx, TLS1_2_VERSION);
#endif
    SSL_CTX_set_verify_depth(enc->ssl_ctx, 5);
    if (SSL_CTX_load_verify_locations(
                enc->ssl_ctx, ichirp->config.CERT_CHAIN_PEM, NULL) != 1) {
        E(chirp,
          "Could not set the verification certificate %s",
          ichirp->config.CERT_CHAIN_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    if (SSL_CTX_use_certificate_chain_file(
                enc->ssl_ctx, ichirp->config.CERT_CHAIN_PEM) != 1) {
        E(chirp,
          "Could not set the certificate %s",
          ichirp->config.CERT_CHAIN_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    if (SSL_CTX_use_PrivateKey_file(
                enc->ssl_ctx,
                ichirp->config.CERT_CHAIN_PEM,
                SSL_FILETYPE_PEM) != 1) {
        E(chirp,
          "Could not set the private key %s",
          ichirp->config.CERT_CHAIN_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    if (SSL_CTX_check_private_key(enc->ssl_ctx) != 1) {
        E(chirp, "Private key is not valid %s", ichirp->config.CERT_CHAIN_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    DH*   dh        = NULL;
    FILE* paramfile = fopen(ichirp->config.DH_PARAMS_PEM, "r");
    if (paramfile == NULL) {
        E(chirp,
          "Could not open the dh-params %s",
          ichirp->config.DH_PARAMS_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    dh = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
    fclose(paramfile);
    if (dh == NULL) {
        E(chirp,
          "Could not load the dh-params %s",
          ichirp->config.DH_PARAMS_PEM);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    if (SSL_CTX_set_tmp_dh(enc->ssl_ctx, dh) != 1) {
        E(chirp,
          "Could not set the dh-params %s",
          ichirp->config.DH_PARAMS_PEM);
        DH_free(dh);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    if (SSL_CTX_set_cipher_list(
                enc->ssl_ctx,
                "-ALL:"
                "DHE-DSS-AES256-GCM-SHA384:"
                "DHE-RSA-AES256-GCM-SHA384:"
                "DHE-RSA-AES256-SHA256:"
                "DHE-DSS-AES256-SHA256:") != 1) {
        E(chirp, "Could not set the cipher list. ch_chirp_t: %p", chirp);
        DH_free(dh);
        SSL_CTX_free(enc->ssl_ctx);
        return CH_TLS_ERROR;
    }
    L(chirp, "Created SSL context for chirp", CH_NO_ARG);
    DH_free(dh);
    return CH_SUCCESS;
}

// .. c:function::
ch_error_t
ch_en_stop(ch_encryption_t* enc)
//    :noindex:
//
//    see: :c:func:`ch_en_stop`
//
// .. code-block:: cpp
//
{
    if (enc->ssl_ctx) {
        SSL_CTX_free(enc->ssl_ctx);
    }
    ERR_clear_error();
#ifdef CH_OPENSSL_10_API
    CRYPTO_THREADID id;
    CRYPTO_THREADID_current(&id);
    ERR_remove_thread_state(&id);
#endif // CH_OPENSSL_10_API
    return CH_SUCCESS;
}

#ifdef CH_OPENSSL_10_API
// .. c:function::
static unsigned long
_ch_en_thread_id_function(void)
//    :noindex:
//
//    see: :c:func:`_ch_en_thread_id_function`
//
// .. code-block:: cpp
//
{
    uv_thread_t self = uv_thread_self();
    return (unsigned long) self;
}
#endif // CH_OPENSSL_10_API
#endif // CH_WITHOUT_TLS
