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
#include "libchirp.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <assert.h>
#include <signal.h>

// Declarations
// ============
//
// .. code-block:: cpp

static ch_chirp_t* _ch_tst_chirp;
static uv_tty_t    _ch_tst_tty;
static int         _ch_tst_tty_init       = 0;
static int         _ch_tst_always_encrypt = 0;
static char        _ch_tst_buf[1024];

// Internal for checks
// ===================
//
// .. code-block:: cpp

#define CH_MSG_ACK 1 << 1
#define CH_MSG_USED 1 << 2

// Definitions
// ============
//
// .. code-block:: cpp
//
//
static void
_ch_tst_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void) (handle);
    (void) (suggested_size);
    buf->base = _ch_tst_buf;
    buf->len  = 1024;
}

static void
_ch_tst_done_cb(ch_chirp_t* chirp)
{
    (void) (chirp);
    if (_ch_tst_tty_init) {
        uv_read_stop((uv_stream_t*) &_ch_tst_tty);
        uv_close((uv_handle_t*) &_ch_tst_tty, NULL);
    }
}

static void
_ch_tst_read_stdin_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    (void) (nread);
    (void) (buf);
    ch_chirp_t* chirp = stream->data;
    ch_chirp_close_ts(chirp);
}

static void
_ch_tst_start(ch_chirp_t* chirp)
{
    CH_WRITE_LOG(chirp, "Echo server started", CH_NO_ARG);
    if (_ch_tst_always_encrypt) {
        ch_chirp_set_always_encrypt();
    }
    if (uv_tty_init(ch_chirp_get_loop(chirp), &_ch_tst_tty, 0, 1) != 0) {
        return;
    }
    _ch_tst_tty_init = 1;
    uv_read_start(
            (uv_stream_t*) &_ch_tst_tty,
            _ch_tst_alloc_cb,
            _ch_tst_read_stdin_cb);
    _ch_tst_tty.data = chirp;
}

static void
_ch_tst_sent_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
{
    (void) (chirp);
    (void) (status);
    CH_WRITE_LOGC(chirp, "Release message.", "ch_message_t:%p", msg);
    ch_chirp_release_msg_slot(msg);
}

static void
_ch_tst_recv_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    assert(msg != NULL && "Not a ch_message_t*");
    assert(!(msg->type & CH_MSG_ACK) && "ACK should not call callback");
    assert(!(msg->_flags & CH_MSG_USED) && "The message should not be used");
    CH_WRITE_LOGC(chirp, "Echo message", "ch_message_t:%p", msg);
    ch_chirp_send(chirp, msg, _ch_tst_sent_cb);
}

int
main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);
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
            _ch_tst_done_cb,
            NULL);
    ch_libchirp_cleanup();
    fprintf(stderr, "Closing echo_etest\n");
    return 0;
}
