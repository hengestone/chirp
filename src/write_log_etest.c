// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =================
// Testing write log
// =================
//
// This is partly a manual test, it tests for crashes, but the user has to
// manualy check if the log format is nice.
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "chirp.h"
#include "libchirp.h"
#include "util.h"

/* clang-format off */
static char* const ch_tst_too_long =
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc "
"abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc abc %d";
/* clang-format on */

// Runner
// ======
//
// .. code-block:: cpp
//

void
ch_tst_log_callback(char msg[], char error)
{
    if (error) {
        fprintf(stderr, "is error");
    }
    fprintf(stderr, "%s", msg);
    fprintf(stderr, "\n");
}

void
ch_tst_write_logs(ch_chirp_t* chirp)
{
    ch_write_log(
            chirp,
            "bla/blala/write_log_etest.c",
            48,
            "message without args",
            "clear part",
            0);
    ch_write_log(
            chirp,
            "bla/blala/error.c",
            48,
            "message without args",
            "clear part",
            1);
    ch_write_log(
            chirp,
            "bla/blala/write_log_etest.c",
            48,
            "args %d",
            "clear part",
            0,
            4);
    ch_write_log(
            chirp, "bla/blala/error.c", 48, "message", "clear arg: %d", 0, 43);
    ch_write_log(
            chirp,
            "bla/blala/error.c",
            48,
            "arg: %d",
            "clear arg: %d",
            0,
            42,
            43);
    ch_write_log(chirp, "bla/blala/error.c", 48, ch_tst_too_long, "", 0, 42);
}

int
main(void)
{
    ch_libchirp_init();
    ch_chirp_t chirp;
    memset(&chirp, 0, sizeof(chirp));
    ch_chirp_int_t ichirp;
    memset(&ichirp, 0, sizeof(ichirp));
    chirp._ = &ichirp;
    ch_random_ints_as_bytes(ichirp.identity, CH_ID_SIZE);
    ch_tst_write_logs(&chirp);
    chirp._log = ch_tst_log_callback;
    fprintf(stderr, "\nlog via callback\n\n");
    ch_tst_write_logs(&chirp);
    ch_libchirp_cleanup();
    return 0;
}
