// ============
// Uninit etest
// ============
//
// Test partial ch_chirp_init() recovery.
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "chirp.h"
#include "libchirp.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <stdio.h>

static ch_chirp_t*  _ch_tst_chirp1;
static ch_config_t* _ch_tst_config1;
static ch_chirp_t*  _ch_tst_chirp2;
static ch_config_t* _ch_tst_config2;

static void
_ch_tst_done_cb(ch_chirp_t* chirp)
{
    (void) (chirp);
    uv_loop_t* loop = ch_chirp_get_loop(chirp);
    if (_ch_tst_chirp1 == chirp) {
        _ch_tst_chirp1 = NULL;
    }
    if (_ch_tst_chirp2 == chirp) {
        ch_chirp_close_ts(_ch_tst_chirp1);
        _ch_tst_chirp2 = NULL;
    }
    ch_free(chirp);
    if (_ch_tst_chirp1 == NULL && _ch_tst_chirp2 == NULL) {
        uv_stop(loop);
    }
}

#ifndef NDEBUG
static int
_ch_tst_fail_at_end(uv_loop_t* loop)
{
    _ch_tst_chirp1  = ch_alloc(sizeof(*_ch_tst_chirp1));
    _ch_tst_config1 = ch_alloc(sizeof(*_ch_tst_config1));
    ch_chirp_config_init(_ch_tst_config1);
    _ch_tst_config1->CERT_CHAIN_PEM = "./cert.pem";
    _ch_tst_config1->DH_PARAMS_PEM  = "./dh.pem";

    _ch_tst_fail_init_at_end = 1;
    if (ch_chirp_init(
                _ch_tst_chirp1,
                _ch_tst_config1,
                loop,
                NULL,
                NULL,
                _ch_tst_done_cb,
                NULL) != CH_SUCCESS) {
        printf("ch_chirp_init (1) error\n");
    }
    _ch_tst_fail_init_at_end = 0;
    ch_free(_ch_tst_config1);
    int ret = 0;
    ret |= ch_run(loop); /* Will block till loop stops */
    return ret;
}
#endif

static int
_ch_tst_listen_fail(uv_loop_t* loop)
{
    _ch_tst_chirp1  = ch_alloc(sizeof(*_ch_tst_chirp1));
    _ch_tst_config1 = ch_alloc(sizeof(*_ch_tst_config1));
    ch_chirp_config_init(_ch_tst_config1);
    _ch_tst_config1->CERT_CHAIN_PEM = "./cert.pem";
    _ch_tst_config1->DH_PARAMS_PEM  = "./dh.pem";

    _ch_tst_chirp2  = ch_alloc(sizeof(*_ch_tst_chirp2));
    _ch_tst_config2 = ch_alloc(sizeof(*_ch_tst_config2));
    ch_chirp_config_init(_ch_tst_config2);
    _ch_tst_config2->CERT_CHAIN_PEM = "./cert.pem";
    _ch_tst_config2->DH_PARAMS_PEM  = "./dh.pem";

    if (ch_chirp_init(
                _ch_tst_chirp1,
                _ch_tst_config1,
                loop,
                NULL,
                NULL,
                _ch_tst_done_cb,
                NULL) != CH_SUCCESS) {
        printf("ch_chirp_init (1) error\n");
    }
    ch_free(_ch_tst_config1);
    if (ch_chirp_init(
                _ch_tst_chirp2,
                _ch_tst_config2,
                loop,
                NULL,
                NULL,
                _ch_tst_done_cb,
                NULL) != CH_SUCCESS) {
        printf("ch_chirp_init (2) error\n");
    }
    ch_free(_ch_tst_config2);
    return ch_run(loop); /* Will block till loop stops */
}

int
main(int argc, char* argv[])
{
    (void) (argc);
    (void) (argv);
    ch_libchirp_init();
    uv_loop_t loop;
    ch_loop_init(&loop);
    int ret = 0;
#ifndef NDEBUG
    ret |= _ch_tst_fail_at_end(&loop);
#endif
    ret |= _ch_tst_listen_fail(&loop);
    ret |= ch_loop_close(&loop);
    ret |= ch_libchirp_cleanup();
    return ret;
}
