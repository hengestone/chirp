// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ============
// Message test
// ============
//
// .. code-block:: cpp

// Project includes
// ================
//
// .. code-block:: cpp

#include "message.h"
#include "quickcheck_test.h"
#include "util.h"

// Definitions
// ===========
//
//
// .. c:function::
int
ch_tst_check_pattern(ch_buf* data, size_t count)
//    :noindex:
//
//    see: :c:func:`ch_ms_gen_message`
//
// .. code-block:: cpp
//
{
    if (count == 0) {
        return 1;
    }
    size_t pos = 7;
    if (memcmp(data, "pattern", pos) != 0) {
        return 0;
    }
    for (;;) {
        uint8_t pat_len = data[pos];
        uint8_t i       = 0;
        pos += 1;
        while (i < pat_len && pos < count) {
            if (((uint8_t) data[pos]) != i) {
                return 0;
            }
            pos += 1;
            i += 1;
        }
        if (pos >= count) {
            return 1;
        }
    }
    return 1;
}

// .. c:function::
static size_t
_ch_tst_gen_data_field(
        float    zero_probability,
        float    max_probability,
        int      max_count,
        ch_buf** data)
//
//   Allocates memory for one of the data field in a chirp message: header,
//   data and writes a pattern into it.
//
//    :param zero_probability: Probability of the len being zero
//    :param max_probability: Probability of the len being max
//    :param int max_count: Max count of that field
//    :param ch_buf**: Buffer for the data
//
// .. code-block:: cpp
//
{
    size_t pos   = 7;
    double draw  = ch_qc_tgen_double();
    size_t count = 0;
    if (draw > zero_probability) {
        if (draw < max_probability) {
            count = max_count;
        } else {
            count = ch_qc_tgen_double() * max_count;
        }
    }
    if (count == 0) {
        return count;
    } else {
        count += pos;
    }
    ch_qc_mem_track_t* track = ch_qc_track_alloc(count);
    *data                    = track->data;
    memcpy(track->data, "pattern", 7);
    while (pos < count) {
        uint8_t i = 0;
        /* The pattern length must always be at least 1, so we scale pat_len to
         * UCHAR_MAX - 1 (254) and add 1. This ensures that there is no string
         * of zeros which might not detect a bug.*/
        uint8_t pat_len  = (uint8_t)(ch_qc_tgen_double() * (UCHAR_MAX - 1)) + 1;
        track->data[pos] = pat_len;
        pos += 1;
        while (pos < count && i < pat_len) {
            track->data[pos] = i;
            pos += 1;
            i += 1;
        }
    }
    return count;
}

// .. c:function::
ch_message_t*
ch_tst_gen_message(void)
//    :noindex:
//
//    see: :c:func:`ch_ms_gen_message`
//
// .. code-block:: cpp
//
{
    int data_count = 1024;
    int big        = ch_qc_tgen_bool();
    if (big) {
        int very = ch_qc_tgen_bool();
        if (very) {
            data_count = 1024 * 1024;
        } else {
            data_count = 1024 * 256;
        }
    }
    ch_qc_mem_track_t* track;
    ch_message_t*      message;
    track   = ch_qc_track_alloc(sizeof(*message));
    message = (ch_message_t*) track->data;
    ch_msg_init(message);
    message->header_len =
            _ch_tst_gen_data_field(0.1, 0.1, 1024, &message->header);
    message->data_len =
            _ch_tst_gen_data_field(0.1, 0.05, data_count, &message->data);
    int ipv6 = ch_qc_tgen_bool();
    if (ipv6) {
        ch_msg_set_address(message, AF_INET6, "::1", 59732);
    } else {
        ch_msg_set_address(message, AF_INET, "127.0.0.1", 59732);
    }
    return message;
}
