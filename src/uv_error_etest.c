// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =======================
// Converting libuv errors
// =======================
//
// First argument is a libuv error number like -14
//
// System includes
// ===============
//
// .. code-block:: cpp
//
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

int
main(int argc, char* argv[])
{
    int err;
    if (argc < 2) {
        printf("The error number should be the first argument\n");
        return 1;
    }
    err = strtol(argv[1], NULL, 10);
    if (errno != 0) {
        printf("The argument is not a number\n");
        return 1;
    }
    printf("%s\n", uv_strerror(err));
    return 0;
}
