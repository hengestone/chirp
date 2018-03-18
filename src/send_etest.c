// ==============================
// Testing chirp in star topology
// ==============================
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Variables
// ===============
//
// .. code-block:: cpp
//
int               _ch_tst_always_encrypt = 0;
int               _ch_tst_msg_count      = 0;
int               _ch_tst_sent           = 0;
int               _ch_tst_msg_len        = 0;
char              _ch_tst_data[]         = "hello";
static uv_timer_t _ch_tst_sleep_timer;

// Testcode
// ========
//
// .. code-block:: cpp
//

static void
_ch_tst_close_cb(uv_timer_t* handle)
{
    ch_chirp_t* chirp = handle->data;
    uv_timer_stop(&_ch_tst_sleep_timer);
    uv_close((uv_handle_t*) &_ch_tst_sleep_timer, NULL);
    ch_chirp_close_ts(chirp);
}

static void
ch_tst_sent_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
{
    (void) (msg);
    if (status != CH_SUCCESS) {
        CH_WRITE_LOGC(chirp, "Send failed", "ch_message_t:%p", msg);
        exit(status);
    }
    _ch_tst_sent += 1;
    if (_ch_tst_sent < _ch_tst_msg_count) {
        ch_chirp_send(chirp, msg, ch_tst_sent_cb);
    } else {
        uv_timer_start(&_ch_tst_sleep_timer, _ch_tst_close_cb, 1000, 0);
    }
}

static void
ch_tst_start(ch_chirp_t* chirp)
{
    ch_message_t* msgs = chirp->user_data;
    for (int i = 0; i < _ch_tst_msg_len; i++) {
        ch_chirp_send(chirp, &msgs[i], ch_tst_sent_cb);
    }
}

static void
ch_tst_recv(ch_chirp_t* chirp, ch_message_t* msg)
{
    ch_chirp_release_msg_slot(chirp, msg, NULL);
}

static void
_ch_tst_parse_hostport_into_port(char* hostport, int* port)
{
    char* port_str = strstr(hostport, ":");
    if (port_str == NULL) {
        fprintf(stderr, "Upstream format must be host:port\n");
        exit(1);
    }
    port_str++; // port start AFTER the colon..
    *port = strtol(port_str, NULL, 10);

    *strstr(hostport, ":") = '\0';
}

static void
_ch_tst_recv_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    assert(msg != NULL && "Not a ch_message_t*");
    CH_WRITE_LOGC(chirp, "Received message", "ch_message_t:%p", msg);
    ch_chirp_release_msg_slot(chirp, msg, NULL);
}

static int
ch_tst_send(int argc, char* argv[])
{
    int         tmp_err;
    ch_chirp_t  chirp;
    uv_loop_t   loop;
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.CERT_CHAIN_PEM  = "./cert.pem";
    config.DH_PARAMS_PEM   = "./dh.pem";
    _ch_tst_always_encrypt = strtol(argv[1], NULL, 10);
    if (errno) {
        fprintf(stderr, "always_encrypt must be integer.\n");
        exit(1);
    }
    if (!(_ch_tst_always_encrypt == 0 || _ch_tst_always_encrypt == 1)) {
        fprintf(stderr, "always_encrypt must be boolean (0/1).\n");
        exit(1);
    }
    _ch_tst_msg_count = strtol(argv[2], NULL, 10);
    if (errno) {
        fprintf(stderr, "nmesg must be integer.\n");
        exit(1);
    }
    int count       = argc - 3;
    _ch_tst_msg_len = count;
    ch_message_t msgs[count];
    if (errno) {
        perror(NULL);
        return 1;
    }
    ch_libchirp_init();
    ch_loop_init(&loop);
    if (ch_chirp_init(
                &chirp,
                &config,
                &loop,
                _ch_tst_recv_message_cb,
                ch_tst_start,
                NULL,
                NULL) != CH_SUCCESS) {
        printf("ch_chirp_init error\n");
        return 1;
    }
    if (_ch_tst_always_encrypt) {
        ch_chirp_set_always_encrypt();
    }
    chirp.user_data = msgs;
    for (int i = 0; i < count; i++) {
        int           arg = 3 + i;
        int           port;
        ch_message_t* msg = &msgs[i];
        ch_msg_init(msg);
        _ch_tst_parse_hostport_into_port(argv[arg], &port);
        tmp_err       = ch_msg_set_address(msg, AF_INET, argv[arg], port);
        msg->data     = _ch_tst_data;
        msg->data_len = strlen(_ch_tst_data);
    }
    ch_chirp_set_auto_stop_loop(&chirp);
    uv_timer_init(&loop, &_ch_tst_sleep_timer);
    _ch_tst_sleep_timer.data = &chirp;
    ch_run(&loop);
    tmp_err = ch_loop_close(&loop);
    ch_libchirp_cleanup();
    return tmp_err;
}

int
main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);
    if (argc < 4) {
        printf("Arguments:\nalways_encrypt nmsgs [ipv4:port]+\n");
        return 1;
    }
    ch_tst_send(argc, argv);
}
