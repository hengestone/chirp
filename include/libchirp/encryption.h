// ==========
// Encryption
// ==========
//
// Interface for manual or fine-grained encryption setup: Only use if you know
// what you are doing. All functions become no-ops if CH_WITHOUT_TLS is
// defined.
//
// .. code-block:: cpp

#ifndef ch_libchirp_encryption_h
#define ch_libchirp_encryption_h

// Declarations
// ============

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_init(void);
//
//    Initialize LibreSSL or OpenSSL according to configuration. Is a no-op if
//    CH_WITHOUT_TLS is defined.
//
//   :return: A chirp error. see: :c:type:`ch_error_t`
//   :rtype:  ch_error_t

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_cleanup(void);
//
//    Cleanup LibreSSL or OpenSSL. Is a no-op if CH_WITHOUT_TLS is defined.
//
//   :return: A chirp error. see: :c:type:`ch_error_t`
//   :rtype:  ch_error_t

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_threading_cleanup(void);
//
//    DO NOT USE, unless you really really know what you are doing. Provided
//    for the rare case where your host application initializes libressl or
//    openssl without threading support, but you need threading. Chirp usually
//    doesn't need threading. Is a no-op if CH_WITHOUT_TLS is defined.
//
//    Cleanup libressl or openssl threading by setting destroying the locks and
//    freeing memory.
//

// .. c:function::
CH_EXPORT
ch_error_t
ch_en_tls_threading_setup(void);
//
//    DO NOT USE, unless you really really know what you are doing. Provided
//    for the rare case where your host application initializes openssl without
//    threading support, but you need threading. Chirp usually doesn't need
//    threading. Is a no-op if CH_WITHOUT_TLS is defined.
//
//    Setup openssl threading by initializing the required locks and setting
//    the lock and the thread_id callbacks.
//

// .. c:function::
CH_EXPORT
void
ch_en_set_manual_tls_init(void);
//
//    Manually initialize LibreSSL or OpenSSL. Is a no-op if CH_WITHOUT_TLS is
//    set.
//
//    By default chirp will initialize libressl or openssl on the first
//    instance of chirp and cleanup libressl or openssl on the last instance of
//    chirp. If for some reason the count of chirp instances can drop to zero
//    sometimes and you do not want libressl or openssl to get uninitialized
//    you can manually call :c:func:`ch_en_tls_init` before creating the first
//    chirp instance and :c:func:`ch_en_tls_cleanup` after closing the last
//    chirp instance.
//
//    You can also initialize libress or openssl yourself if you have to or not
//    initialize it at all if your host application has already initialized
//    libressl or openssl.
//

#endif // ch_libchirp_encryption_h
