// ==========
// Echo etest
// ==========
//
// Very simple echo server for hypothesis tests.
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "common.h"
#include "connection.h"
#include "libchirp.h"
#include "message.h"

// System includes
// ===============
//
// .. code-block:: cpp
//

// Declarations
// ============
//
// .. code-block:: cpp

static ch_chirp_t* _ch_tst_chirp;
static int         _ch_tst_always_encrypt = 0;

// Definitions
// ============
//
// .. code-block:: cpp
//
//
static void
_ch_tst_start(ch_chirp_t* chirp)
{
    ch_chirp_check_m(chirp);
    L(chirp, "Echo server started", CH_NO_ARG);
    if (_ch_tst_always_encrypt) {
        ch_chirp_set_always_encrypt(chirp);
    }
}

static void
_ch_tst_sent_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
{
    (void) (chirp);
    (void) (status);
    ch_chirp_check_m(chirp);
    L(chirp, "Release message ch_message_t:%p", msg);
    ch_chirp_release_message(msg);
}
static void
_ch_tst_recv_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    ch_chirp_check_m(chirp);
    A(msg != NULL, "Not a ch_message_t*");
    A(!(msg->type & CH_MSG_ACK), "ACK should not call callback");
    A(!(msg->_flags & CH_MSG_USED), "The message should not be used");
    L(chirp, "Echo message ch_message_t:%p", msg);
    ch_chirp_send(chirp, msg, _ch_tst_sent_cb);
}

int
main(int argc, char* argv[])
{
    fprintf(stderr, "Starting echo_etest\n");
    if (argc < 3) {
        fprintf(stderr, "%s listen_port always_encrypt\n", argv[0]);
        exit(1);
    }
    int port = strtol(argv[1], NULL, 10);
    if (errno) {
        fprintf(stderr, "port must be integer.\n");
        exit(1);
    }
    if (port <= 1024) {
        fprintf(stderr, "port must be greater than 1024.\n");
        exit(1);
    }
    if (port > 0xFFFF) {
        fprintf(stderr, "port must be lesser than %d.\n", 0xFFFF);
        exit(1);
    }
    _ch_tst_always_encrypt = strtol(argv[2], NULL, 10);
    if (errno) {
        fprintf(stderr, "always_encrypt must be integer.\n");
        exit(1);
    }
    if (!(_ch_tst_always_encrypt == 0 || _ch_tst_always_encrypt == 1)) {
        fprintf(stderr, "always_encrypt must be boolean (0/1).\n");
        exit(1);
    }
    ch_libchirp_init();
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.PORT           = port;
    config.CERT_CHAIN_PEM = "./cert.pem";
    config.DH_PARAMS_PEM  = "./dh.pem";
    ch_chirp_run(
            &config,
            &_ch_tst_chirp,
            _ch_tst_recv_message_cb,
            _ch_tst_start,
            NULL,
            NULL);
    ch_libchirp_cleanup();
    fprintf(stderr, "Closing echo_etest\n");
    return 0;
}
