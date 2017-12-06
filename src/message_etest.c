// =============
// Message etest
// =============
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "chirp.h"
#include "common.h"
#include "libchirp.h"
#include "message_test.h"
#include "qs.h"
#include "quickcheck_test.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <assert.h>
#include <getopt.h>
#include <unistd.h>

// Declarations
// ============
//
// .. code-block:: cpp
//

typedef enum {
    CH_TST_ALWAYS_ENCRYPT = 1000,
    CH_TST_MESSAGE_COUNT  = 1001,
    CH_TST_BUFFER_SIZE    = 1002,
    CH_TST_TIMEOUT        = 1003,
    CH_TST_NO_ACK         = 1004,
    CH_TST_MIN_HANDLERS   = 1005,
    CH_TST_MAX_MSG_SIZE   = 1006,
    CH_TST_SLOW           = 1007,
    CH_TST_HELP           = 1008,
} ch_tst_args_t;

typedef ch_error_t (*ch_tst_chirp_send_t)(
        ch_chirp_t* chirp, ch_message_t* msg, ch_send_cb_t send_cb);

#define ch_tr_other_chirp(x)                                                   \
    ((ch_tst_chirp_thread_t*) (x)->user_data)->other->chirp

#define PORT_SENDER 59731
#define PORT_ECHO 59732

static void
_ch_tst_send_message(ch_chirp_t* chirp);

// Definitions
// ===========
//
// .. code-block:: cpp
//
MINMAX_FUNCS(int)

// Test functions
// ==============
//
// .. code-block:: cpp
//

struct ch_tst_chirp_thread_s;
typedef struct ch_tst_chirp_thread_s ch_tst_chirp_thread_t;
struct ch_tst_chirp_thread_s {
    int                    port;
    ch_start_cb_t          init;
    ch_tst_chirp_thread_t* other;
    ch_chirp_t*            chirp;
};

struct ch_tst_msg_stack_s;
typedef struct ch_tst_msg_stack_s ch_tst_msg_stack_t;
struct ch_tst_msg_stack_s {
    ch_message_t*       msg;
    ch_tst_msg_stack_t* next;
};

qs_stack_bind_m(ch_tst_msg, ch_tst_msg_stack_t) CH_ALLOW_NL;

static int                 _ch_tst_msg_send_count = 0;
static ch_message_t        _ch_tst_msg;
static int                 _ch_tst_msg_echo_count = 0;
static char                _ch_tst_data[]         = "hello";
static int                 _ch_tst_buffer_size    = 0;
static float               _ch_tst_timeout        = 5.0;
static int                 _ch_tst_message_count  = 20;
static uv_timer_t          _ch_tst_sleep_timer;
static int                 _ch_tst_ack          = 1;
static int                 _ch_tst_slow         = 0;
static int                 _ch_tst_min_handlers = 0;
static uint32_t            _ch_tst_max_msg_size = CH_MAX_MSG_SIZE;
static ch_tst_msg_stack_t* _ch_tst_msg_stack    = NULL;

static void
_ch_tst_simple_msg(ch_message_t* msg)
{
    ch_msg_init(msg);
    ch_msg_set_address(msg, AF_INET, "127.0.0.1", PORT_ECHO);
    msg->data     = _ch_tst_data;
    msg->data_len = strnlen(_ch_tst_data, sizeof(_ch_tst_data));
}

static void
_ch_tst_delay_release(uv_timer_t* handle)
{
    (void) (handle);
    ch_tst_msg_stack_t* cur;
    ch_tst_msg_pop(&_ch_tst_msg_stack, &cur);
    while (cur != NULL) {
        ch_chirp_release_message(cur->msg);
        ch_free(cur);
        ch_tst_msg_pop(&_ch_tst_msg_stack, &cur);
    }
}

static void
_ch_tst_echo_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
{
    (void) (chirp);
    (void) (status);
    _ch_tst_msg_echo_count += 1;
    A(status == CH_SUCCESS, "Echoing failed");
    int           stack_was_null = _ch_tst_msg_stack == NULL;
    ch_chirp_t*   other          = ch_tr_other_chirp(chirp);
    ch_identity_t other_id       = ch_chirp_get_identity(other);
    ch_identity_t id             = ch_msg_get_remote_identity(msg);
    A(memcmp(&other_id, &id, sizeof(id)) == 0, "Wrong remote identity set");
    if (_ch_tst_slow) {
        ch_tst_msg_stack_t* item = ch_alloc(sizeof(*item));
        item->next               = NULL;
        item->msg                = msg;
        ch_tst_msg_push(&_ch_tst_msg_stack, item);
        if (stack_was_null) {
            uv_timer_start(&_ch_tst_sleep_timer, _ch_tst_delay_release, 300, 0);
        }
    } else {
        ch_chirp_release_message(msg);
    }
    if (_ch_tst_msg_echo_count == _ch_tst_message_count) {
        uv_timer_stop(&_ch_tst_sleep_timer);
        _ch_tst_delay_release(&_ch_tst_sleep_timer);
        uv_close((uv_handle_t*) &_ch_tst_sleep_timer, NULL);
        ch_chirp_close_ts(ch_tr_other_chirp(chirp));
        ch_chirp_close_ts(chirp);
    }
}

static void
_ch_tst_sent_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
{
    (void) (status);
    (void) (msg);
    ch_qc_free_mem();
    A(status == CH_SUCCESS, "Sending failed");
    _ch_tst_msg_send_count += 1;
    if (_ch_tst_msg_send_count < _ch_tst_message_count) {
        L(chirp,
          "Message to send: %d",
          _ch_tst_message_count - _ch_tst_msg_send_count);
        _ch_tst_send_message(chirp);
    }
}

static void
_ch_tst_recv_echo_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    ch_chirp_check_m(chirp);
    A(msg != NULL, "Not a ch_message_t*");
    A(!(msg->type & CH_MSG_ACK), "ACK should not call callback");
    A(!(msg->_flags & CH_MSG_USED), "The message should not be used");
    if (memcmp(msg->data, "hello", ch_min_int(5, msg->data_len)) == 0) {
        L(chirp, "Echo received hello", CH_NO_ARG);
    } else {
        assert(ch_tst_check_pattern(msg->header, msg->header_len));
        assert(ch_tst_check_pattern(msg->data, msg->data_len));
        L(chirp, "Echo received a message", CH_NO_ARG);
    }
    ch_chirp_send(chirp, msg, _ch_tst_echo_cb);
}

static void
_ch_tst_send_message(ch_chirp_t* chirp)
{
    ch_chirp_check_m(chirp);

    int simple = ch_qc_tgen_bool();
    int use_ts = ch_qc_tgen_bool();

    ch_tst_chirp_send_t send_func;
    if (use_ts) {
        send_func = ch_chirp_send_ts;
    } else {
        send_func = ch_chirp_send;
    }

    int ret;
    if (simple) {
        _ch_tst_simple_msg(&_ch_tst_msg);
        _ch_tst_msg.port = PORT_ECHO;
        ret              = send_func(chirp, &_ch_tst_msg, _ch_tst_sent_cb);
    } else {
        ch_message_t* msg = ch_tst_gen_message();
        assert(ch_tst_check_pattern(msg->header, msg->header_len));
        assert(ch_tst_check_pattern(msg->data, msg->data_len));
        msg->port = PORT_ECHO;
        ret       = send_func(chirp, msg, _ch_tst_sent_cb);
    }
    (void) (ret);
    assert(ret == CH_SUCCESS);
}

static void
_ch_tst_sender_init_handler(ch_chirp_t* chirp)
{
    ch_chirp_check_m(chirp);
    _ch_tst_send_message(chirp);
}

static void
_ch_tst_echo_init_handler(ch_chirp_t* chirp)
{
    ch_chirp_check_m(chirp);
    uv_timer_init(ch_chirp_get_loop(chirp), &_ch_tst_sleep_timer);
    ch_chirp_set_recv_callback(chirp, _ch_tst_recv_echo_message_cb);
}

static void
_ch_tst_run_chirp(void* arg)
{
    ch_tst_chirp_thread_t* args = (ch_tst_chirp_thread_t*) arg;
    ch_chirp_t             chirp;
    uv_loop_t              loop;
    ch_config_t            config;
    ch_chirp_config_init(&config);
    config.BUFFER_SIZE    = _ch_tst_buffer_size;
    config.TIMEOUT        = _ch_tst_timeout;
    config.REUSE_TIME     = _ch_tst_timeout * 15;
    config.PORT           = args->port;
    config.CERT_CHAIN_PEM = "./cert.pem";
    config.DH_PARAMS_PEM  = "./dh.pem";
    config.ACKNOWLEDGE    = _ch_tst_ack;
    config.MAX_MSG_SIZE   = _ch_tst_max_msg_size;
    if (_ch_tst_min_handlers) {
        config.MAX_HANDLERS = 1;
    }
    ch_loop_init(&loop);
    if (ch_chirp_init(&chirp, &config, &loop, NULL, args->init, NULL, NULL) !=
        CH_SUCCESS) {
        printf("ch_chirp_init error\n");
        return;
    }
    args->chirp     = &chirp;
    chirp.user_data = args;
    ch_chirp_set_auto_stop_loop(&chirp);
    ch_run(&loop); /* Will block till loop stops */
    ch_loop_close(&loop);
}

// Runner
// ======
//
// .. code-block:: cpp
//
static void
print_help(struct option long_options[])
{
    printf("Usage:\n\n");
    while (long_options->name != NULL) {
        printf("    --%s ", long_options->name);
        if (long_options->has_arg == required_argument) {
            printf("arg");
        } else if (long_options->has_arg == optional_argument) {
            printf("[arg]");
        }
        printf("\n");
        long_options++;
    }
}

int
main(int argc, char* argv[])
{
    int                  option_index;
    static struct option long_options[] = {
            {"always-encrypt", no_argument, 0, CH_TST_ALWAYS_ENCRYPT},
            {"message-count", required_argument, 0, CH_TST_MESSAGE_COUNT},
            {"timeout", required_argument, 0, CH_TST_TIMEOUT},
            {"buffer-size", required_argument, 0, CH_TST_BUFFER_SIZE},
            {"max-msg-size", required_argument, 0, CH_TST_MAX_MSG_SIZE},
            {"no-ack", no_argument, 0, CH_TST_NO_ACK},
            {"min-handlers", no_argument, 0, CH_TST_MIN_HANDLERS},
            {"slow", no_argument, 0, CH_TST_SLOW},
            {"help", no_argument, 0, CH_TST_HELP},
            {NULL, 0, 0, 0}};
    for (;;) {
        int c = getopt_long(
                argc, argv, "abc:d:012", long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case CH_TST_HELP:
            print_help(long_options);
            exit(1);
            break;
        case CH_TST_ALWAYS_ENCRYPT:
            printf("Set always encrypt\n");
            ch_chirp_set_always_encrypt();
            break;
        case CH_TST_MESSAGE_COUNT:
            _ch_tst_message_count = strtol(optarg, NULL, 10);
            if (errno) {
                fprintf(stderr, "message-count must be integer.\n");
                exit(1);
            }
            break;
        case CH_TST_BUFFER_SIZE:
            _ch_tst_buffer_size = strtol(optarg, NULL, 10);
            if (errno) {
                fprintf(stderr, "buffer-size must be integer.\n");
                exit(1);
            }
            break;
        case CH_TST_MAX_MSG_SIZE:
            _ch_tst_max_msg_size = strtol(optarg, NULL, 10);
            if (errno) {
                fprintf(stderr, "max-msg-size must be integer.\n");
                exit(1);
            }
            break;
        case CH_TST_TIMEOUT:
            _ch_tst_timeout = strtof(optarg, NULL);
            if (errno) {
                fprintf(stderr, "timeout must be float.\n");
                exit(1);
            }
            break;
        case CH_TST_NO_ACK:
            printf("Set no-ack\n");
            _ch_tst_ack = 0;
            break;
        case CH_TST_MIN_HANDLERS:
            printf("Set min-handlers\n");
            _ch_tst_min_handlers = 1;
            break;
        case CH_TST_SLOW:
            printf("Set slow\n");
            _ch_tst_slow = 1;
            break;
        default:
            fprintf(stderr, "unknown option\n");
            exit(1);
        }
    }
    ch_tst_chirp_thread_t sender_thread;
    ch_tst_chirp_thread_t echo_thread;
    sender_thread.other = &echo_thread;
    echo_thread.other   = &sender_thread;
    sender_thread.port  = PORT_SENDER;
    sender_thread.init  = _ch_tst_sender_init_handler;
    echo_thread.port    = PORT_ECHO;
    echo_thread.init    = _ch_tst_echo_init_handler;
    uv_thread_t echo;
    uv_thread_t sender;
    ch_libchirp_init();
    uv_thread_create(&echo, _ch_tst_run_chirp, &echo_thread);
    sleep(1);
    uv_thread_create(&sender, _ch_tst_run_chirp, &sender_thread);
    uv_thread_join(&echo);
    uv_thread_join(&sender);
    ch_qc_free_mem();
    ch_libchirp_cleanup();
}
