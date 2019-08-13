// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ========
// Libchirp
// ========
//
// .. code-block:: cpp

#ifndef ch_libchirp_h
#define ch_libchirp_h

#include "libchirp/chirp.h"
#include "libchirp/encryption.h"
#include "libchirp/message.h"

// .. c:var:: extern char* ch_version
//
//    Version of chirp.
//
// .. code-block:: cpp
//
CH_EXPORT
extern char* ch_version;

// .. c:function::
CH_EXPORT
ch_error_t
ch_libchirp_cleanup(void);
//
//    Cleanup the global libchirp structures, including encryption/libssl
//
//    In release-mode we do not cleanup the TLS library. If you want to check
//    for memory leaks in your application define :c:macro:`CH_TLS_CLEANUP` or
//    call :c:func:`ch_en_tls_cleanup` manually.

// .. c:function::
CH_EXPORT
ch_error_t
ch_libchirp_init(void);
//
//    Initialize the global libchirp structures, including encryption/libssl
//

// .. c:function::
CH_EXPORT
void
ch_set_alloc_funcs(
        ch_alloc_cb_t alloc, ch_realloc_cb_t realloc, ch_free_cb_t free);
//
//    Set allocation functions.
//
//    .. note::
//
//       The user can change the functions multiple times. The user has to
//       ensure consistency of allocation/free pairs.
//
//    :param ch_alloc_cb_t alloc:     Memory allocation callback. Has the same
//                                    signature as malloc
//    :param ch_realloc_cb_t realloc: Memory reallocation callback. Has the same
//                                    signature as realloc
//    :param ch_free_cb_t free:       Callback to free memory. Has the same
//                                    signature as free
//
// .. code-block:: cpp
//

#endif // ch_libchirp_h
