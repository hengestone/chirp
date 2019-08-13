// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ===================
// Message test Header
// ===================
//
// Generate messages for tests
//
// .. code-block:: cpp
//
#ifndef ch_message_test_h
#define ch_message_test_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "buffer.h"
#include "common.h"
#include "message.h"

// Declarations
// ============

// .. c:function::
int
ch_tst_check_pattern(ch_buf* data, size_t count);
//
//    Check if the pattern is correct.
//
//    :param ch_buf*: Buffer containing the pattern
//    :param ch_buf*: The size of data
//    :rtype: bool
//

// .. c:function::
ch_message_t*
ch_tst_gen_message(void);
//
//    Generate a message (ch_message_t*). The memoy allocated, tracked and
//    freed by quickcheck.
//
//    :param ch_chirp_t* chirp: Pointer to chirp instance
//
//    :rtype: ch_message_t*

// Definitions
// ===========

#endif // ch_message_test_h
