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
#include "sds_test.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <string.h>
#include <stdio.h>

// Variables
// ===============
//
// .. code-block:: cpp
//
int _msg_count = 0;
int _sent = 0;
int _msgs_len = 0;
char _data[] = "hello";

// Testcode
// ========
//
// .. code-block:: cpp
//
static
void
ch_tst_sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status, float load)
{
    (void)(status);
    (void)(load);
    (void)(msg);
    _sent += 1;
    if(_sent < _msg_count)
        ch_chirp_send(
                chirp,
                msg,
                ch_tst_sent_cb
        );
    else
        ch_chirp_close_ts(chirp);
}

static
void
ch_tst_start(ch_chirp_t* chirp)
{
    ch_message_t* msgs = chirp->user_data;
    for(int i = 0; i < _msgs_len; i++) {
        ch_chirp_send(
                chirp,
                &msgs[i],
                ch_tst_sent_cb
        );
    }
}

static
int
ch_tst_send(
        int argc,
        char* argv[]
)
{
    int tmp_err;
    int cspl;
    ch_chirp_t chirp;
    uv_loop_t loop;
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.CERT_CHAIN_PEM = "./cert.pem";
    config.DH_PARAMS_PEM = "./dh.pem";
    _msg_count = strtol(argv[1], NULL, 10);
    int count = argc - 2;
    _msgs_len = count;
    ch_message_t msgs[count];
    if(errno) {
        perror(NULL);
        return 1;
    }
    ch_libchirp_init();
    ch_loop_init(&loop);
    if(ch_chirp_init(
            &chirp,
            &config,
            &loop,
            NULL,
            ch_tst_start,
            NULL,
            NULL
    ) != CH_SUCCESS) {
        printf("ch_chirp_init error\n");
        return 1;
    }
    chirp.user_data = msgs;
    for(int i = 0; i < count; i++) {
        int port;
        ch_message_t* msg = &msgs[i];
        ch_msg_init(msg);
        sds s = sdsnew(argv[i + 2]);
        sds* spl = sdssplitlen(s, sdslen(s), ":", 1, &cspl);
        if(cspl != 2) {
            printf("Invalid argument\n");
            sdsfreesplitres(spl, cspl);
            sdsfree(s);
            return 1;
        }
        port = strtol(spl[1], NULL, 10);
        if(errno) {
            perror(NULL);
            sdsfreesplitres(spl, cspl);
            sdsfree(s);
            return 1;
        }
        tmp_err = ch_msg_set_address(msg, AF_INET, spl[0], port);
        if(tmp_err != CH_SUCCESS) {
            printf("Invalid argument\n");
            sdsfreesplitres(spl, cspl);
            sdsfree(s);
            return 1;
        }
        sdsfreesplitres(spl, cspl);
        sdsfree(s);
        msg->data = _data;
        msg->data_len = strlen(_data);
    }
    ch_chirp_set_auto_stop_loop(&chirp);
    ch_run(&loop);
    tmp_err = ch_loop_close(&loop);
    ch_libchirp_cleanup();
    return tmp_err;
}

static
int
ch_tst_listen(
        char* port
)
{
    ch_chirp_t chirp;
    uv_loop_t loop;
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.CERT_CHAIN_PEM = "./cert.pem";
    config.DH_PARAMS_PEM = "./dh.pem";
    config.PORT = strtol(port, NULL, 10);
    if(errno) {
        perror(NULL);
        return 1;
    }
    ch_libchirp_init();
    ch_loop_init(&loop);
    if(ch_chirp_init(
            &chirp,
            &config,
            &loop,
            NULL,
            NULL,
            NULL,
            NULL
    ) != CH_SUCCESS) {
        printf("ch_chirp_init error\n");
        return 1;
    }
    ch_chirp_set_auto_stop_loop(&chirp);
    ch_run(&loop);
    int tmp_err = ch_loop_close(&loop);
    ch_libchirp_cleanup();
    return tmp_err;
}

int
main(
        int argc,
        char* argv[]
)
{
    if(argc < 2) {
        printf(
            "Arguments:\nport : listen mode\n"
            "[nmsgs] [ipv4:port]+ : send mode\n"
        );
        return 1;
    } else if(argc == 2)
        ch_tst_listen(argv[1]);
    else
        ch_tst_send(argc, argv);
}
