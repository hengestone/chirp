// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ==================
// External functions
// ==================
//
// Wrapper around commonly used libuv functions: init, close, run
//
// .. code-block:: cpp

#ifndef ch_libchirp_wrappers_h
#define ch_libchirp_wrappers_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"

// Declarations
// ============

// .. c:function::
CH_EXPORT
int
ch_loop_init(uv_loop_t* loop);
//
//    An alias for uv_loop_init. Please refer to the libuv documentation.
//
//    :param uv_loop_t* loop: Loop struct allocated by user.
//
//    :return: the status of the initialization.
//    :rtype:  int
//

// .. c:function::
CH_EXPORT
int
ch_loop_close(uv_loop_t* loop);
//
//    An alias for uv_loop_close. Please refer to the libuv documentation.
//
//    :param uv_loop_t* loop: Loop struct allocated by the user.
//
//    :return: the status of the closing action.
//    :rtype:  int

// .. c:function::
CH_EXPORT
int
ch_run(uv_loop_t* loop);
//
//    A wrapper for uv_run.
//    Runs the loop once again, in case closing chirp's resources caused
//    additional requests/handlers.
//
//    Please refer to the libuv documentation.
//
//    :param uv_loop_t*  loop: Loop struct allocated by user.
//
//    :return: the status of the action.
//    :rtype:  int
//
// .. code-block:: cpp

#endif // ch_libchirp_wrappers_h
