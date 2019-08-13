// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =============
// Common header
// =============
//
// Defines global forward declarations and utility macros.
//
// .. code-block:: cpp
//
#ifndef ch_common_h
#define ch_common_h

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp-config.h"
#include "libchirp/common.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
// =====================
//
// .. code-block:: cpp

struct ch_remote_s;
typedef struct ch_remote_s ch_remote_t;
struct ch_connection_s;
typedef struct ch_connection_s ch_connection_t;
struct ch_protocol_s;
typedef struct ch_protocol_s ch_protocol_t;

//
// .. _double-eval:
//
// Double evaluation
// =================
//
// In order to get proper file and line number we have to implement validation
// and assertion using macros. But macros are prone to double evaluation, if you
// pass function having side-effects. Passing a pure function or data is valid.
//
// So ONLY pass variables to the V and A macros!
//
// But what is double evaluation?
//     The macro will just copy the function call, passed to it. So it will
//     be evaluated inside the macro, which people will often forget, the
//     code will behave in unexpected ways. You might think that this is
//     obvious, but forgetting it so easy. Humans have no problem writing
//     something like this and be amazed that it doesn't work.
//
// .. code-block:: cpp
//
//    int i = generate_int();
//    /* A few lines in between, so it isn't that obvious */
//    V(chirp, generate_int() == 0, "The first int generated isn't zero");
//
// Which should look like this.
//
// .. code-block:: cpp
//
//    int i = generate_int();
//    V(chirp, i == 0, "The first int generated isn't zero");
//
// Definitions
// ===========

// .. c:type:: ch_chirp_uninit_t
//
//    Used to uninitialize on ch_chirp_init failure.
//
// .. code-block:: cpp
//
typedef enum {
    CH_UNINIT_INIT_LOCK     = 1 << 0,
    CH_UNINIT_ICHIRP        = 1 << 1,
    CH_UNINIT_ASYNC_CLOSE   = 1 << 2,
    CH_UNINIT_ASYNC_DONE    = 1 << 3,
    CH_UNINIT_ASYNC_START   = 1 << 4,
    CH_UNINIT_ASYNC_SEND_TS = 1 << 5,
    CH_UNINIT_SEND_TS_LOCK  = 1 << 6,
    CH_UNINIT_ASYNC_RELE_TS = 1 << 7,
    CH_UNINIT_RELE_TS_LOCK  = 1 << 8,
    CH_UNINIT_SERVERV4      = 1 << 9,
    CH_UNINIT_SERVERV6      = 1 << 10,
    CH_UNINIT_TIMER_GC      = 1 << 11,
    CH_UNINIT_TIMER_RECON   = 1 << 12,
    CH_UNINIT_SIGNAL        = 1 << 13,
} ch_chirp_uninit_t;

#ifndef NDEBUG
#ifndef CH_ENABLE_LOGGING
#define CH_ENABLE_LOGGING
#endif
#ifndef CH_ENABLE_ASSERTS
#define CH_ENABLE_ASSERTS
#endif
#endif

// .. c:macro:: EC
//
//    Reports an error.
//
//    The error macro E(chirp, message, ...) behaves like printf and allows to
//    log to a custom callback. Usually used to log into pythons logging
//    facility. On the callback it sets the argument error to true and it will
//    log to stderr if no callback is set.
//
//    :param chirp: Pointer to a chirp object.
//    :param message: The highlighted message to report.
//    :param clear:   The clear message to report.
//    :param ...: Variadic arguments for xprintf
//
// .. code-block:: cpp
//
#define EC(chirp, message, clear, ...)                                         \
    ch_write_log(chirp, __FILE__, __LINE__, message, clear, 1, __VA_ARGS__);
#define E(chirp, message, ...) EC(chirp, message, "", __VA_ARGS__)

// .. c:macro:: V
//
//    Validates the given condition and reports a message including arbitrary
//    given arguments when the condition is not met in debug-/development-mode.
//
//    Be aware of :ref:`double-eval`
//
//    The validate macro V(chirp, condition, message, ...) behaves like printf
//    and allows to print a message given an assertion. If that assertion is
//    not fullfilled, it will print the given message and return
//    :c:member:`ch_error_t.CH_VALUE_ERROR`, even in release mode.
//
//    :param chirp: Pointer to a chirp object.
//    :param condition: A boolean condition to check.
//    :param message: Message to print when the condition is not met.
//    :param ...: Variadic arguments for xprintf
//
// .. code-block:: cpp
//
#ifndef NDEBUG
#define V(chirp, condition, message, ...)                                      \
    if (!(condition)) {                                                        \
        ch_write_log(chirp, __FILE__, __LINE__, message, "", 1, __VA_ARGS__);  \
        exit(1);                                                               \
    }
#else
#define V(chirp, condition, message, ...)                                      \
    if (!(condition)) {                                                        \
        ch_write_log(chirp, __FILE__, __LINE__, message, "", 1, __VA_ARGS__);  \
        return CH_VALUE_ERROR;                                                 \
    }
#endif

#ifdef CH_ENABLE_LOGGING

// .. c:macro:: LC
//
//    Logs the given message including arbitrary arguments to a custom callback
//    in debug-/development-mode.
//
//    The logging macro L(chirp, message, ...) behaves like printf
//    and allows to log to a custom callback. Usually used to log into pythons
//    logging facility.
//
//    The logging macro LC(chirp, message, clear, ...) behaves like
//    L except you can add part that isn't highlighted.
//
//    :param chirp:   Pointer to a chirp object.
//    :param message: The highlighted message to report.
//    :param clear:   The clear message (not highlighted) to report.
//    :param ...:     Variadic arguments for xprintf
//
// .. code-block:: cpp
//
#define LC(chirp, message, clear, ...)                                         \
    CH_WRITE_LOGC(chirp, message, clear, __VA_ARGS__)
#define L(chirp, message, ...) CH_WRITE_LOG(chirp, message, __VA_ARGS__)

#else // CH_ENABLE_LOGGING

// .. c:macro:: L
//
//    See :c:macro:`L`. Does nothing in release-mode.
// .. code-block:: cpp
//
#define L(chirp, message, ...)                                                 \
    (void) (chirp);                                                            \
    (void) (message)
#define LC(chirp, message, clear, ...)                                         \
    (void) (chirp);                                                            \
    (void) (message);                                                          \
    (void) (clear)
#endif

#ifdef CH_ENABLE_ASSERTS

// .. c:macro:: A
//
//    Validates the given condition and reports arbitrary arguments when the
//    condition is not met in debug-/development-mode.
//
//    Be aware of :ref:`double-eval`
//
//    :param condition: A boolean condition to check.
//    :param message:   Message to display
//
// .. code-block:: cpp
//
#define A(condition, message)                                                  \
    if (!(condition)) {                                                        \
        fprintf(stderr,                                                        \
                "%s:%d: Assert failed: %s\n",                                  \
                __FILE__,                                                      \
                __LINE__,                                                      \
                message);                                                      \
        exit(1);                                                               \
    }

// .. c:macro:: AP
//
//    Validates the given condition and reports arbitrary arguments when the
//    condition is not met in debug-/development-mode.
//
//    Be aware of :ref:`double-eval`
//
//    The assert macro AP(condition, ...) behaves like printf and allows to
//    print a arbitrary arguments when the given assertion fails.
//
//    :param condition: A boolean condition to check.
//    :param message:   Message to display
//
// .. code-block:: cpp
//
#define AP(condition, message, ...)                                            \
    if (!(condition)) {                                                        \
        fprintf(stderr, "%s:%d: Assert failed: ", __FILE__, __LINE__);         \
        fprintf(stderr, message "\n", __VA_ARGS__);                            \
        exit(1);                                                               \
    }

// .. c:macro:: ch_chirp_check_m()
//
//    Validates that we have a valid chirp object and we are on the right
//    thread.
//
// .. code-block:: cpp

#define ch_chirp_check_m(chirp)                                                \
    {                                                                          \
        A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");                \
        uv_thread_t __ch_chirp_check_self_ = uv_thread_self();                 \
        A(uv_thread_equal(&__ch_chirp_check_self_, &chirp->_thread) != 0,      \
          "Call on the wrong thread");                                         \
    }

#else // CH_ENABLE_ASSERTS

//.. c:macro:: A
//
//    See :c:macro:`A`. Does nothing in release-mode.
//
//    Be aware of :ref:`double-eval`
//
// .. code-block:: cpp
//
#define A(condition, message) (void) 0

//.. c:macro:: AP
//
//    See :c:macro:`AP`. Does nothing in release-mode.
//
//    Be aware of :ref:`double-eval`
//
// .. code-block:: cpp
//
#define AP(condition, message, ...) (void) 0

#define ch_chirp_check_m(chirp) (void) (chirp)

#endif

#ifndef A
#error Assert macro not defined
#endif
#ifndef AP
#error Assert print macro not defined
#endif
#ifndef ch_chirp_check_m
#error ch_chirp_check_m macro not defined
#endif
#ifndef L
#error Log macro L not defined
#endif
#ifndef LC
#error Log macro LC not defined
#endif
#ifndef V
#error Validate macro not defined
#endif

#define CH_CHIRP_MAGIC 42429
/* Used cheat clang format */
#define CH_ALLOW_NL static void _ch_noop_func__(void)

// Generic functions
// =================
//
// .. code-block:: text
//
#define MINMAX_FUNCS(type)                                                     \
    static type ch_max_##type(type a, type b) { return (a > b) ? a : b; }      \
    static type ch_min_##type(type a, type b) { return (a < b) ? a : b; }

#endif // ch_common_h
