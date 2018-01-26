// ==================
// Testing quickcheck
// ==================
//
// Test quickcheck with a few simple examples.
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp.h"
#include "quickcheck_test.h"
#include "util.h"

static int
ch_tst_is_ascii_string(ch_qc_mem_track_t* item, char* string)
{
    int i;
    for (i = 0; i < item->count; i++) {
        if (string[i] < 0) {
            return 0;
        }
    }
    return item->data[item->count - 1] == 0;
}

// Runner
// ======
//
// .. code-block:: cpp
//
int
main(void)
{
    ch_libchirp_init();
    int i;
    int ret = 0;
    ch_qc_init();
    for (i = 0; i < 100; i++) {
        char*              string;
        ch_qc_mem_track_t* item = ch_qc_tgen_string(&string);
        if (!ch_tst_is_ascii_string(item, string)) {
            ret |= 1;
            printf("%s is not ascii\n", string);
            break;
        }
    }
    if (ret == 0) {
        printf("Test sucessful\n");
    }
    ch_qc_free_mem();
    ch_libchirp_cleanup();
    return ret;
}
