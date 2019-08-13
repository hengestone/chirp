// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ===============================
// Common testing functions header
// ===============================
//
// Common testing helper functions
//
// .. code-block:: cpp

#ifndef ch_test_test_h
#define ch_test_test_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp/common.h"
#include "mpack_test.h"
#include "mpipe_test.h"

// .. c:function::
void
ch_tst_return_int(mpack_writer_t* writer, int val);
//
//    Write a integer back to the test driver
//
//    :param mpack_writer_t* writer: Mpack writer
//    :param int val: Value to return
//
// .. c:function::
void
ch_tst_return_float(mpack_writer_t* writer, float val);
//
//    Write a float back to the test driver
//
//    :param mpack_writer_t* writer: Mpack writer
//    :param float val: Value to return
//
#endif
