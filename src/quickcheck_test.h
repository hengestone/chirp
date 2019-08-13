// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =================
// Quickcheck header
// =================
//
// Random generator for basic data-types (Template based generation ala
// hypotheis might be added in the future.)
//
// .. code-block:: cpp

#ifndef ch_quickcheck_test_h
#define ch_quickcheck_test_h

// Project includes
// ================
//
// .. code-block:: cpp

#include "common.h"
#include "qs.h"
#include "rbtree.h"

// System includes
// ===============
//
// .. code-block:: cpp

#include <stdbool.h>
#include <stdlib.h>

// Declarations
// ============
//
// Types
// -----

// .. c:type:: ch_qc_mem_track_t
//
//    List to track memory allocations and free them after a test.
//
//    .. c:member:: ch_buf* data
//
//       Pointer to the data.
//
//    .. c:member:: size_t size
//
//       Size of one item of the memory
//
//    .. c:member:: unsigned char count
//
//       Count of items of the memory. size * count equals the total size in
//       bytes.
//
//    .. c:member:: struct ch_qc_mem_track_s *next
//
//       Pointer to the next list item.
//
// .. code-block:: cpp
//
typedef struct ch_qc_mem_track_s {
    ch_buf*                   data;
    size_t                    size;
    unsigned char             count;
    struct ch_qc_mem_track_s* next;
} ch_qc_mem_track_t;

// rbtree prototypes
// -----------------
//
// .. code-block:: cpp

qs_stack_bind_decl_m(ch_qc, ch_qc_mem_track_t) CH_ALLOW_NL;

// Macros
// ------
//
// .. code-block:: cpp

#define ch_qc_tgen_array_m(item, typed, gen_function)                          \
    {                                                                          \
        size_t __ch_qc_tgen_len_ = (rand() % 99) + 1;                          \
                                                                               \
        typed         = ch_alloc((size_t) __ch_qc_tgen_len_ * sizeof(*typed)); \
        item          = ch_alloc(sizeof(ch_qc_mem_track_t));                   \
        (item)->data  = (ch_buf*) typed;                                       \
        (item)->count = __ch_qc_tgen_len_;                                     \
        (item)->size  = sizeof(*typed);                                        \
        (item)->next  = NULL;                                                  \
        ch_qc_push(&_ch_qc_mem_track, item);                                   \
                                                                               \
        size_t __ch_qc_tgen_i_;                                                \
                                                                               \
        for (__ch_qc_tgen_i_ = 0;                                              \
             __ch_qc_tgen_i_ < (size_t) __ch_qc_tgen_len_;                     \
             __ch_qc_tgen_i_++) {                                              \
            (typed)[__ch_qc_tgen_i_] = gen_function();                         \
        }                                                                      \
    }

// Functions
// ---------

// .. c:function::
void
ch_qc_free_mem(void);
//
//    Free memory allocations made during quickcheck test. Call this if you
//    don't intend to call ch_qc_for_all().

// .. c:function::
void
ch_qc_init(void);
//
//    Initializes srand. Can be omitted to initialize srand in different way.
//

// .. c:function::
ch_qc_mem_track_t*
ch_qc_tgen_bytes(uint8_t** bytes);
//
//    Generate a string of bytes of type uint8_t*. The memoy allocated, tracked
//    and freed by quickcheck. Typed version used in intermediate generation
//    functions.
//
//    :param uint8_t** bytes: (out) The bytes generated
//
//    :rtype: ch_qc_mem_track_t*
//

// .. c:function::
ch_qc_mem_track_t*
ch_qc_tgen_string(char** string);
//
//    Generate a string of ascii characters of type char*. The memoy allocated,
//    tracked and freed by quickcheck. Typed version used in intermediate
//    generation functions.
//
//    :param char** string: (out) The string generated
//
//    :rtype: ch_qc_mem_track_t*
//

// .. c:function::
ch_qc_mem_track_t*
ch_qc_track_alloc(size_t size);
//
//    Allocate memory and track it. The memory is freed by quickcheck.
//
//    :param size_t size: The size of memory to allocate

// .. c:function::
int
ch_qc_tgen_bool(void);
//
//    Generate data of type bool.
//
//    :rtype: int

// .. c:function::
uint8_t
ch_qc_tgen_byte(void);
//
//    Generate byte of type uint8_t.
//
//    :rtype: uint8_t

// .. c:function::
char
ch_qc_tgen_char(void);
//
//    Generate ascii character of type char.
//
//    :rtype: char
//

// .. c:function::
double
ch_qc_tgen_double(void);
//
//    Generate positive float of type double.
//
//    :rtype: dobule

// .. c:function::
int
ch_qc_tgen_int(void);
//
//    Generate int.
//
//    :rtype: int

// .. c:function::
uint8_t
ch_qc_pgen_uint8_t(void);
//
//    Generate uint8_t using a property template.
//
//    1/10: 0
//    1/10: MAX
//    8/10: Random
//
//    :rtype: uint8_t

// .. c:function::
uint16_t
ch_qc_pgen_uint16_t(void);
//
//    Generate uint16_t using a property template.
//
//    1/10: 0
//    1/10: MAX
//    8/10: Random
//
//    :rtype: uint16_t

// .. c:function::
uint32_t
ch_qc_pgen_uint32_t(void);
//
//    Generate uint32_t using a property template.
//
//    1/10: 0
//    1/10: MAX
//    8/10: Random
//
//    :rtype: uint32_t

// .. code-block:: cpp

#endif // ch_quickcheck_test_h
