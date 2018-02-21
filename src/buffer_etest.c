// ===================
// Testing buffer pool
// ===================
//
// Test runner if the buffer pool handles errors and misuse. Driven by
// hypothesis.
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "buffer.h"
#include "libchirp.h"
#include "test_test.h"

typedef enum {
    func_init_e    = 1,
    func_acquire_e = 2,
    func_release_e = 3,
    func_cleanup_e = 4,
} ch_tst_func_t;

static ch_buffer_pool_t* pool;

// Runner
// ======
//
// .. code-block:: cpp
//
static void
ch_tst_test_slot(mpack_node_t data, mpack_writer_t* writer)
{
    int func = mpack_node_int(mpack_node_array_at(data, 0));
    int val  = mpack_node_int(mpack_node_array_at(data, 1));
    switch (func) {
        ch_bf_slot_t* slot;
    case func_init_e:
        pool = ch_alloc(sizeof(*pool));
        ch_bf_init(pool, NULL, val);
        ch_tst_return_int(writer, 0);
        break;
    case func_acquire_e:
        slot = ch_bf_acquire(pool);
        mpack_start_array(writer, 1);
        if (slot == NULL) {
            mpack_write_int(writer, -1);
        } else {
            mpack_write_int(writer, slot->id);
        }
        mpack_finish_array(writer);
        break;
    case func_release_e:
        ch_bf_release(pool, val);
        ch_tst_return_int(writer, 0);
        break;
    case func_cleanup_e:
        ch_bf_free(pool);
        ch_tst_return_int(writer, 0);
        break;
    default:
        assert(0 && "Not implemented");
    }
}

int
main(void)
{
    ch_libchirp_init();
    int ret = mpp_runner(ch_tst_test_slot);
    ch_libchirp_cleanup();
    return ret;
}
