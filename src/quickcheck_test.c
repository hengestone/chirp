// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// ==========
// Quickcheck
// ==========
//
// Fork of https://github.com/mcandre/qc by Andrew Pennebaker removing gc.h
// and changing almost everything.
//
// Forked at commit fc8e7f76af339fdd56546ad46cb9d97cd42ec5cb
//
//
// .. code-block:: cpp
//
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "quickcheck_test.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Declarations
// ============
//
// .. c:var:: ch_qc_mem_track_t* _ch_qc_mem_track
//
//    List of memory allocations during current quickcheck test.
//
// .. code-block:: cpp
//
static ch_qc_mem_track_t* _ch_qc_mem_track = NULL;

// Definitions
// ===========
//
// qs Prototypes
// -------------
//
// .. code-block:: cpp
//
qs_stack_bind_impl_m(ch_qc, ch_qc_mem_track_t) CH_ALLOW_NL;

// Functions
// ---------
//

// .. c:function::
void
ch_qc_free_mem(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_free_mem`
//
// .. code-block:: cpp
//
{
    ch_qc_mem_track_t* item;
    while (_ch_qc_mem_track != NULL) {
        ch_qc_pop(&_ch_qc_mem_track, &item);
        ch_free(item->data);
        ch_free(item);
    }
}

// .. c:function::
void
ch_qc_init(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_init`
//
// .. code-block:: cpp
//
{
    srand((unsigned int) time(NULL));
}

// .. c:function::
int
ch_qc_tgen_bool(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_bool`
//
// .. code-block:: cpp
//
{
    return rand() % 2 == 0;
}

// .. c:function::
uint8_t
ch_qc_tgen_byte(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_byte`
//
// .. code-block:: cpp
//
{
    return rand(); /* No need to mod overflow of unsigned char is
                    * is well defined. */
}

// .. c:function::
char
ch_qc_tgen_char(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_char`
//
// .. code-block:: cpp
//
{
    return (char) ((rand() % 127) + 1);
}

// .. c:function::
double
ch_qc_tgen_double(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_double`
//
// .. code-block:: cpp
//
{
    return ((double) rand() / (double) RAND_MAX);
}

// .. c:function::
int
ch_qc_tgen_int(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_int`
//
// .. code-block:: cpp
//
{
    return rand();
}

// .. c:function::
uint8_t
ch_qc_pgen_uint8_t(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_pgen_uint8_t`
//
// .. code-block:: cpp
//
{
    double prop = ch_qc_tgen_double();
    if (prop < 0.1) {
        return 0;
    }
    if (prop < 0.2) {
        return 0xFF;
    }
    return rand();
}

// .. c:function::
uint16_t
ch_qc_pgen_uint16_t(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_pgen_uint16_t`
//
// .. code-block:: cpp
//
{
    double prop = ch_qc_tgen_double();
    if (prop < 0.1) {
        return 0;
    }
    if (prop < 0.2) {
        return 0xFFFF;
    }
    return rand();
}

// .. c:function::
uint32_t
ch_qc_pgen_uint32_t(void)
//    :noindex:
//
//    see: :c:func:`ch_qc_pgen_uint32_t`
//
// .. code-block:: cpp
//
{
    double prop = ch_qc_tgen_double();
    if (prop < 0.1) {
        return 0;
    }
    if (prop < 0.2) {
        return 0xFFFFFFFF;
    }
    return rand();
}

// .. c:function::
ch_qc_mem_track_t*
ch_qc_tgen_bytes(uint8_t** bytes)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_bytes`
//
// .. code-block:: cpp
//
{
    ch_qc_mem_track_t* item;
    ch_qc_tgen_array_m(item, *bytes, ch_qc_tgen_byte);

    return item;
}

// .. c:function::
ch_qc_mem_track_t*
ch_qc_tgen_string(char** string)
//    :noindex:
//
//    see: :c:func:`ch_qc_tgen_string`
//
// .. code-block:: cpp
//
{
    ch_qc_mem_track_t* item;
    ch_qc_tgen_array_m(item, *string, ch_qc_tgen_char);
    item->data[(item->count * item->size) - 1] = 0;

    return item;
}

// .. c:function::
ch_qc_mem_track_t*
ch_qc_track_alloc(size_t size)
//    :noindex:
//
//    see: :c:func:`ch_qc_track_alloc`
//
// .. code-block:: cpp
//
{
    ch_buf*            arr  = ch_alloc(size);
    ch_qc_mem_track_t* item = ch_alloc(sizeof(*item));
    item->data              = arr;
    item->count             = size;
    item->size              = 1;
    item->next              = NULL;
    ch_qc_push(&_ch_qc_mem_track, item);

    return item;
}
