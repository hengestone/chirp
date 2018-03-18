// =====
// Chirp
// =====

// Project includes
// ================
//
// .. code-block:: cpp
//
#include "chirp.h"
#include "common.h"
#include "remote.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <openssl/err.h>
#include <signal.h>
#include <time.h>
#ifdef _WIN32
#define ch_access _access
#include <io.h>
#else
#define ch_access access
#include <unistd.h>
#endif

// Declarations
// ============
//
// .. c:var:: uv_mutex_t _ch_chirp_init_lock
//
//    It seems that initializing signals accesses a shared data-structure. We
//    lock during init, just to be sure.
//
// .. code-block:: cpp
//
static uv_mutex_t _ch_chirp_init_lock;

// .. c:var:: int _ch_libchirp_initialized
//
//    Variable to check if libchirp is already initialized.
//
// .. code-block:: cpp
//
static int _ch_libchirp_initialized = 0;

// .. c:var:: ch_config_t ch_config_defaults
//
//    Default config of chirp.
//
// .. code-block:: cpp
//
static ch_config_t _ch_config_defaults = {
        .REUSE_TIME         = 30,
        .TIMEOUT            = 5.0,
        .PORT               = 2998,
        .BACKLOG            = 100,
        .MAX_SLOTS          = 0,
        .SYNCHRONOUS        = 1,
        .DISABLE_SIGNALS    = 0,
        .BUFFER_SIZE        = 0,
        .MAX_MSG_SIZE       = CH_MAX_MSG_SIZE,
        .BIND_V6            = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .BIND_V4            = {0, 0, 0, 0},
        .IDENTITY           = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .CERT_CHAIN_PEM     = NULL,
        .DH_PARAMS_PEM      = NULL,
        .DISABLE_ENCRYPTION = 0,
};

#ifndef NDEBUG
int _ch_tst_fail_init_at_end = 0;
#endif

// .. c:function::
static void
_ch_chirp_ack_send_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status);
//
//    Called by chirp once ack_msg is sent.
//
//    :param ch_chirp_t* chirp: Chirp instance
//    :param ch_message_t* msg: Ack message sent

// .. c:function::
static void
_ch_chirp_check_closing_cb(uv_prepare_t* handle);
//
//    Close chirp when the closing semaphore reaches zero.
//
//    :param uv_prepare_t* handle: Prepare handle which will be stopped (and
//                                 thus its callback)
//

// .. c:function::
static void
_ch_chirp_close_async_cb(uv_async_t* handle);
//
//    Internal callback to close chirp. Makes ch_chirp_close_ts thread-safe
//
//    :param uv_async_t* handle: Async handle which is used to closed chirp
//

// .. c:function::
static void
_ch_chirp_closing_down_cb(uv_handle_t* handle);
//
//    Close chirp after the check callback has been closed, calls
//    done-callback in next uv-loop iteration. To make sure that any open
//    requests are handled, before informing the user.
//
//    :param uv_handle_t* handle: Handle just closed
//

// .. c:function::
static void
_ch_chirp_done_cb(uv_async_t* handle);
//
//    The done async-callback calls the user supplied done callback when chirp
//    is finished. Then closes itself, which calls _ch_chirp_stop_cb, which
//    finally frees chirp.
//
//    :param uv_async_t* handle: Async handle
//

// .. c:function::
static void
_ch_chirp_init_signals(ch_chirp_t* chirp);
//
//    Setup signal handlers for chirp. Internally called from ch_chirp_init()
//
//   :param   ch_chirp_t* chrip: Instance of a chirp object

// .. c:function::
static void
_ch_chirp_sig_handler(uv_signal_t*, int);
//
//    Closes all chirp instances on sig int.
//
//    :param uv_signal_t* handle : The libuv signal handler structure
//
//    :param int signo: The signal number, that tells which signal should be
//                      handled.
//

// .. c:function::
static void
_ch_chirp_start_cb(uv_async_t* handle);
//
//    Start callback calls the user supplied done callback.
//
//    :param uv_async_t* handle: Async handler.
//

// .. c:function::
static void
_ch_chirp_stop_cb(uv_handle_t* handle);
//
//    Last close callback when stopping chirp. Frees chirp.
//
//    :param uv_handle_t* handle: handle just closed
//
//
// .. c:function::
static void
_ch_chirp_uninit(ch_chirp_t* chirp, uint16_t uninit);
//
//    Uninitializes resources on failed ch_chirp_init.
//
//    :param ch_chirp_t* chirp: Instance of a chirp object
//    :param uint16_t   uninit: Initialization state
//

// .. c:function::
static ch_error_t
_ch_chirp_verify_cfg(ch_chirp_t* chirp);
//
//    Verifies the configuration.
//
//    :param   ch_chirp_t* chirp: Instance of a chirp object
//
//    :return: A chirp error. see: :c:type:`ch_error_t`
//    :rtype:  ch_error_t

// .. c:var:: extern char* ch_version
//    :noindex:
//
//    Version of chirp.
//
// .. code-block:: cpp
//
CH_EXPORT
char* ch_version = CH_VERSION;

// Definitions
// ===========

// .. c:function::
static void
_ch_chirp_ack_send_cb(ch_chirp_t* chirp, ch_message_t* msg, ch_error_t status)
//    :noindex:
//
//    see: :c:func:`_ch_rd_handshake`
//
// .. code-block:: cpp
//
{
    (void) (msg);
    (void) (status);
    ch_chirp_check_m(chirp);
    if (msg->_release_cb != NULL) {
        ch_chirp_t* rchirp = msg->user_data;
        ch_chirp_check_m(rchirp);
        ch_connection_t* conn = msg->_pool;
        ch_release_cb_t  cb   = msg->_release_cb;
        msg->_release_cb      = NULL;
        cb(rchirp, msg->identity, conn->release_serial);
    }
}

// .. c:function::
static void
_ch_chirp_check_closing_cb(uv_prepare_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_check_closing_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    A(ichirp->closing_tasks > -1, "Closing semaphore dropped below zero");
    L(chirp, "Check closing semaphore (%d)", ichirp->closing_tasks);
    /* In production we allow the semaphore to drop below zero but log it as
     * an error. */
    if (ichirp->closing_tasks < 1) {
        int tmp_err;
        tmp_err = uv_prepare_stop(handle);
        A(tmp_err == CH_SUCCESS, "Could not stop prepare callback");
        (void) (tmp_err);
#ifndef CH_WITHOUT_TLS
        if (!ichirp->config.DISABLE_ENCRYPTION) {
            tmp_err = ch_en_stop(&ichirp->encryption);
            A(tmp_err == CH_SUCCESS, "Could not stop encryption");
        }
#endif
        uv_close((uv_handle_t*) handle, _ch_chirp_closing_down_cb);
    }
    if (ichirp->closing_tasks < 0) {
        E(chirp, "Check closing semaphore dropped blow 0", CH_NO_ARG);
    }
}

// .. c:function::
static void
_ch_chirp_close_async_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_close_async_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    if (chirp->_ == NULL) {
        E(chirp, "Chirp closing callback called on closed", CH_NO_ARG);
        return;
    }
    ch_chirp_int_t* ichirp = chirp->_;
    if (ichirp->flags & CH_CHIRP_CLOSED) {
        E(chirp, "Chirp closing callback called on closed", CH_NO_ARG);
        return;
    }
    L(chirp, "Chirp closing callback called", CH_NO_ARG);
    int tmp_err;
    tmp_err = ch_pr_stop(&ichirp->protocol);
    A(tmp_err == CH_SUCCESS, "Could not stop protocol");
    (void) (tmp_err);
    if (!ichirp->config.DISABLE_SIGNALS) {
        uv_signal_stop(&ichirp->signals[0]);
        uv_signal_stop(&ichirp->signals[1]);
        uv_close((uv_handle_t*) &ichirp->signals[0], ch_chirp_close_cb);
        uv_close((uv_handle_t*) &ichirp->signals[1], ch_chirp_close_cb);
        ichirp->closing_tasks += 2;
    }
    uv_close((uv_handle_t*) &ichirp->send_ts, ch_chirp_close_cb);
    uv_close((uv_handle_t*) &ichirp->release_ts, ch_chirp_close_cb);
    uv_close((uv_handle_t*) &ichirp->close, ch_chirp_close_cb);
    ichirp->closing_tasks += 3;
    uv_mutex_destroy(&ichirp->send_ts_queue_lock);
    uv_mutex_destroy(&ichirp->release_ts_queue_lock);
    tmp_err = uv_prepare_init(ichirp->loop, &ichirp->close_check);
    A(tmp_err == CH_SUCCESS, "Could not init prepare callback");
    ichirp->close_check.data = chirp;
    /* We use a semaphore to wait until all callbacks are done:
     * 1. Every time a new callback is scheduled we do
     *    ichirp->closing_tasks += 1
     * 2. Every time a callback is called we do ichirp->closing_tasks -= 1
     * 3. Every uv_loop iteration before it blocks we check
     *    ichirp->closing_tasks == 0
     * -> if we reach 0 all callbacks are done and we continue freeing memory
     * etc. */
    tmp_err =
            uv_prepare_start(&ichirp->close_check, _ch_chirp_check_closing_cb);
    A(tmp_err == CH_SUCCESS, "Could not start prepare callback");
}

// .. c:function::
void
ch_chirp_close_cb(uv_handle_t* handle)
//    :noindex:
//
//    see: :c:func:`ch_chirp_close_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    chirp->_->closing_tasks -= 1;
    LC(chirp,
       "Closing semaphore (%d). ",
       "uv_handle_t:%p",
       chirp->_->closing_tasks,
       (void*) handle);
}

// .. c:function::
static void
_ch_chirp_closing_down_cb(uv_handle_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_closing_down_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    if (uv_async_send(&chirp->_->done) < 0) {
        E(chirp, "Could not call done callback", CH_NO_ARG);
    }
}
// .. c:function::
static void
_ch_chirp_stop_cb(uv_handle_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_stop_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_int_t* ichirp = handle->data;
    if (ichirp->flags & CH_CHIRP_AUTO_STOP) {
        uv_stop(ichirp->loop);
    }
    ch_free(ichirp);
}

// .. c:function::
static void
_ch_chirp_uninit(ch_chirp_t* chirp, uint16_t uninit)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_uninit`
//
// .. code-block:: cpp
//
{
    /* The existence of this function shows a little technical dept. Classes
     * like chirp/protocol/encryption only know how to close themselves when
     * they are fully initialized. This should be streamlined one day. */
    if (uninit & CH_UNINIT_ASYNC_DONE) {
        ch_chirp_int_t* ichirp   = chirp->_;
        ch_protocol_t*  protocol = &ichirp->protocol;

        if (uninit & CH_UNINIT_ASYNC_SEND_TS) {
            uv_close((uv_handle_t*) &ichirp->send_ts, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_ASYNC_RELE_TS) {
            uv_close((uv_handle_t*) &ichirp->release_ts, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_ASYNC_CLOSE) {
            uv_close((uv_handle_t*) &ichirp->close, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_ASYNC_START) {
            uv_close((uv_handle_t*) &ichirp->start, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_SEND_TS_LOCK) {
            uv_mutex_destroy(&ichirp->send_ts_queue_lock);
        }
        if (uninit & CH_UNINIT_RELE_TS_LOCK) {
            uv_mutex_destroy(&ichirp->release_ts_queue_lock);
        }
        if (uninit & CH_UNINIT_SERVERV4) {
            uv_close((uv_handle_t*) &protocol->serverv4, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_SERVERV6) {
            uv_close((uv_handle_t*) &protocol->serverv6, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_TIMER_GC) {
            uv_timer_stop(&protocol->gc_timeout);
            uv_close((uv_handle_t*) &protocol->gc_timeout, ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_TIMER_RECON) {
            uv_timer_stop(&protocol->reconnect_timeout);
            uv_close(
                    (uv_handle_t*) &protocol->reconnect_timeout,
                    ch_chirp_close_cb);
            ichirp->closing_tasks += 1;
        }
        if (uninit & CH_UNINIT_SIGNAL) {
            uv_signal_stop(&ichirp->signals[0]);
            uv_signal_stop(&ichirp->signals[1]);
            uv_close((uv_handle_t*) &ichirp->signals[0], ch_chirp_close_cb);
            uv_close((uv_handle_t*) &ichirp->signals[1], ch_chirp_close_cb);
            ichirp->closing_tasks += 2;
        }

        uv_prepare_init(ichirp->loop, &ichirp->close_check);
        ichirp->close_check.data = chirp;
        uv_prepare_start(&ichirp->close_check, _ch_chirp_check_closing_cb);
    } else {
        chirp->_init = 0;
        if (uninit & CH_UNINIT_ICHIRP) {
            ch_free(chirp->_);
        }
    }
    if (uninit & CH_UNINIT_INIT_LOCK) {
        uv_mutex_unlock(&_ch_chirp_init_lock);
    }
}

// .. c:function::
static void
_ch_chirp_done_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_done_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    handle->data           = ichirp;
    uv_close((uv_handle_t*) handle, _ch_chirp_stop_cb);
    L(chirp, "Closed.", CH_NO_ARG);
    if (ichirp->flags & CH_CHIRP_AUTO_STOP) {
        LC(chirp,
           "UV-Loop stopped by chirp. ",
           "uv_loop_t:%p",
           (void*) ichirp->loop);
    }
    if (ichirp->done_cb != NULL) {
        ichirp->done_cb(chirp);
    }
}

// .. c:function::
static void
_ch_chirp_init_signals(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_init_signals`
//
// .. code-block:: cpp
//
{
#ifndef CH_DISABLE_SIGNALS
    ch_chirp_int_t* ichirp = chirp->_;
    if (ichirp->config.DISABLE_SIGNALS) {
        return;
    }
    uv_signal_init(ichirp->loop, &ichirp->signals[0]);
    uv_signal_init(ichirp->loop, &ichirp->signals[1]);

    ichirp->signals[0].data = chirp;
    ichirp->signals[1].data = chirp;

    if (uv_signal_start(&ichirp->signals[0], &_ch_chirp_sig_handler, SIGINT)) {
        E(chirp, "Unable to set SIGINT handler", CH_NO_ARG);
        return;
    }

    if (uv_signal_start(&ichirp->signals[1], &_ch_chirp_sig_handler, SIGTERM)) {
        uv_signal_stop(&ichirp->signals[0]);
        uv_close((uv_handle_t*) &ichirp->signals[0], NULL);
        E(chirp, "Unable to set SIGTERM handler", CH_NO_ARG);
    }
#else
    (void) (chirp);
#endif
}

// .. c:function::
static void
_ch_chirp_sig_handler(uv_signal_t* handle, int signo)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_sig_handler`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);

    if (signo != SIGINT && signo != SIGTERM) {
        return;
    }

    ch_chirp_close_ts(chirp);
}

// .. c:function::
static void
_ch_chirp_start_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_start_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    uv_close((uv_handle_t*) handle, NULL);
    if (ichirp->start_cb != NULL) {
        ichirp->start_cb(chirp);
    }
}

// .. c:function::
static ch_error_t
_ch_chirp_verify_cfg(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`_ch_chirp_verify_cfg`
//
// .. code-block:: cpp
//
{
    ch_config_t* conf = &chirp->_->config;
#ifndef CH_WITHOUT_TLS
    if (!conf->DISABLE_ENCRYPTION) {
        V(chirp,
          conf->DH_PARAMS_PEM != NULL,
          "Config: DH_PARAMS_PEM must be set.",
          CH_NO_ARG);
        V(chirp,
          conf->CERT_CHAIN_PEM != NULL,
          "Config: CERT_CHAIN_PEM must be set.",
          CH_NO_ARG);
        V(chirp,
          ch_access(conf->CERT_CHAIN_PEM, F_OK) != -1,
          "Config: cert %s does not exist.",
          conf->CERT_CHAIN_PEM);
        V(chirp,
          ch_access(conf->DH_PARAMS_PEM, F_OK) != -1,
          "Config: cert %s does not exist.",
          conf->CERT_CHAIN_PEM);
    }
#endif
    V(chirp,
      conf->PORT > 1024,
      "Config: port must be > 1024. (%d)",
      conf->PORT);
    V(chirp,
      conf->BACKLOG < 128,
      "Config: backlog must be < 128. (%d)",
      conf->BACKLOG);
    V(chirp,
      conf->TIMEOUT <= 60,
      "Config: timeout must be <= 60. (%f)",
      conf->TIMEOUT);
    V(chirp,
      conf->TIMEOUT >= 0.1,
      "Config: timeout must be >= 0.1. (%f)",
      conf->TIMEOUT);
    V(chirp,
      conf->REUSE_TIME >= 0.5,
      "Config: resuse time must be => 0.5. (%f)",
      conf->REUSE_TIME);
    V(chirp,
      conf->REUSE_TIME <= 3600,
      "Config: resuse time must be <= 3600. (%f)",
      conf->REUSE_TIME);
    V(chirp,
      conf->TIMEOUT <= conf->REUSE_TIME,
      "Config: timeout must be <= reuse time. (%f, %f)",
      conf->TIMEOUT,
      conf->REUSE_TIME);
    if (conf->SYNCHRONOUS == 1) {
        V(chirp,
          conf->MAX_SLOTS == 1,
          "Config: if synchronous is enabled max slots must be 1.",
          CH_NO_ARG);
    }
    V(chirp,
      conf->MAX_SLOTS <= 32,
      "Config: max slots must be <= 1.",
      CH_NO_ARG);
    V(chirp,
      conf->BUFFER_SIZE >= CH_MIN_BUFFER_SIZE || conf->BUFFER_SIZE == 0,
      "Config: buffer size must be > %d (%u)",
      CH_MIN_BUFFER_SIZE,
      conf->BUFFER_SIZE);
    V(chirp,
      conf->BUFFER_SIZE >= sizeof(ch_message_t) || conf->BUFFER_SIZE == 0,
      "Config: buffer size must be > %lu (%u)",
      (unsigned long) sizeof(ch_message_t),
      conf->BUFFER_SIZE);
    V(chirp,
      conf->BUFFER_SIZE >= CH_SR_HANDSHAKE_SIZE || conf->BUFFER_SIZE == 0,
      "Config: buffer size must be > %lu (%u)",
      (unsigned long) CH_SR_HANDSHAKE_SIZE,
      conf->BUFFER_SIZE);
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_close_ts(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`ch_chirp_close_ts`
//
//    This function is thread-safe.
//
// .. code-block:: cpp
//
{
    char            chirp_closed = 0;
    ch_chirp_int_t* ichirp;
    if (chirp == NULL || chirp->_init != CH_CHIRP_MAGIC) {
        fprintf(stderr,
                "%s:%d Fatal: chirp is not initialzed. ch_chirp_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) chirp);
        return CH_NOT_INITIALIZED;
    }
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    if (chirp->_ != NULL) {
        ichirp = chirp->_;
        if (ichirp->flags & CH_CHIRP_CLOSED) {
            chirp_closed = 1;
        }
    } else {
        chirp_closed = 1;
    }
    if (chirp_closed) {
        fprintf(stderr,
                "%s:%d Fatal: chirp is already closed. ch_chirp_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) chirp);
        return CH_FATAL;
    }
    if (ichirp->flags & CH_CHIRP_CLOSING) {
        E(chirp, "Close already in progress", CH_NO_ARG);
        return CH_IN_PRORESS;
    }
    ichirp->flags |= CH_CHIRP_CLOSING;
    ichirp->close.data = chirp;
    L(chirp, "Closing chirp via callback", CH_NO_ARG);
    if (uv_async_send(&ichirp->close) < 0) {
        E(chirp, "Could not call close callback", CH_NO_ARG);
        return CH_UV_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
void
ch_chirp_config_init(ch_config_t* config)
//    :noindex:
//
//    see: :c:func:`ch_chirp_config_init`
//
// .. code-block:: cpp
//
{
    *config = _ch_config_defaults;
}

// .. c:function::
CH_EXPORT
ch_identity_t
ch_chirp_get_identity(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`ch_chirp_get_identity`
//
// .. code-block:: cpp
//
{
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    ch_identity_t id;
    memcpy(id.data, chirp->_->identity, sizeof(id.data));
    return id;
}

// .. c:function::
CH_EXPORT
uv_loop_t*
ch_chirp_get_loop(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`ch_chirp_get_loop`
//
// .. code-block:: cpp
//
{
    A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    return chirp->_->loop;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_init(
        ch_chirp_t*        chirp,
        const ch_config_t* config,
        uv_loop_t*         loop,
        ch_recv_cb_t       recv_cb,
        ch_start_cb_t      start_cb,
        ch_done_cb_t       done_cb,
        ch_log_cb_t        log_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_init`
//
// .. code-block:: cpp
//
{
    int      tmp_err;
    uint16_t uninit = 0;
    uv_mutex_lock(&_ch_chirp_init_lock);
    uninit |= CH_UNINIT_INIT_LOCK;
    memset(chirp, 0, sizeof(*chirp));
    chirp->_init           = CH_CHIRP_MAGIC;
    chirp->_thread         = uv_thread_self();
    ch_chirp_int_t* ichirp = ch_alloc(sizeof(*ichirp));
    if (!ichirp) {
        fprintf(stderr,
                "%s:%d Fatal: Could not allocate memory for chirp. "
                "ch_chirp_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) chirp);
        uv_mutex_unlock(&_ch_chirp_init_lock);
        return CH_ENOMEM;
    }
    uninit |= CH_UNINIT_ICHIRP;
    memset(ichirp, 0, sizeof(*ichirp));
    ichirp->done_cb         = done_cb;
    ichirp->config          = *config;
    ichirp->public_port     = config->PORT;
    ichirp->loop            = loop;
    ichirp->start_cb        = start_cb;
    ichirp->recv_cb         = recv_cb;
    ch_config_t*   tmp_conf = &ichirp->config;
    ch_protocol_t* protocol = &ichirp->protocol;
    chirp->_                = ichirp;
    if (log_cb != NULL) {
        ch_chirp_set_log_callback(chirp, log_cb);
    }

    unsigned int i = 0;
    while (i < (sizeof(tmp_conf->IDENTITY) - 1) && tmp_conf->IDENTITY[i] == 0)
        i += 1;
    if (tmp_conf->IDENTITY[i] == 0) {
        ch_random_ints_as_bytes(ichirp->identity, sizeof(ichirp->identity));
    } else {
        *ichirp->identity = *tmp_conf->IDENTITY;
    }

    if (tmp_conf->SYNCHRONOUS) {
        tmp_conf->MAX_SLOTS = 1;
    } else {
        if (tmp_conf->MAX_SLOTS == 0) {
            tmp_conf->MAX_SLOTS = 16;
        }
    }

    if (uv_async_init(loop, &ichirp->done, _ch_chirp_done_cb) < 0) {
        E(chirp, "Could not initialize done handler", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        /* Small inaccuracy for ease of use. The user can await the done_cb,
         * except if we return CH_ENOMEM */
        return CH_ENOMEM;
    }
    ichirp->done.data = chirp;
    uninit |= CH_UNINIT_ASYNC_DONE;

    tmp_err = _ch_chirp_verify_cfg(chirp);
    if (tmp_err != CH_SUCCESS) {
        _ch_chirp_uninit(chirp, uninit);
        return tmp_err;
    }

    if (uv_async_init(loop, &ichirp->close, _ch_chirp_close_async_cb) < 0) {
        E(chirp, "Could not initialize close callback", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    ichirp->close.data = chirp;
    uninit |= CH_UNINIT_ASYNC_CLOSE;
    if (uv_async_init(loop, &ichirp->start, _ch_chirp_start_cb) < 0) {
        E(chirp, "Could not initialize done handler", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    ichirp->start.data = chirp;
    uninit |= CH_UNINIT_ASYNC_START;
    if (uv_async_init(loop, &ichirp->send_ts, ch_wr_send_ts_cb) < 0) {
        E(chirp, "Could not initialize send_ts handler", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    ichirp->send_ts.data = chirp;
    uninit |= CH_UNINIT_ASYNC_SEND_TS;
    if (uv_mutex_init(&ichirp->send_ts_queue_lock) < 0) {
        E(chirp, "Could not initialize send_ts_lock", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    uninit |= CH_UNINIT_SEND_TS_LOCK;
    if (uv_async_init(loop, &ichirp->release_ts, ch_chirp_release_ts_cb) < 0) {
        E(chirp, "Could not initialize release_ts handler", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    ichirp->release_ts.data = chirp;
    uninit |= CH_UNINIT_ASYNC_RELE_TS;
    if (uv_mutex_init(&ichirp->release_ts_queue_lock) < 0) {
        E(chirp, "Could not initialize release_ts_lock", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    }
    uninit |= CH_UNINIT_RELE_TS_LOCK;

    ch_pr_init(chirp, protocol);
    tmp_err = ch_pr_start(protocol, &uninit);
    if (tmp_err != CH_SUCCESS) {
        E(chirp, "Could not start protocol: %d", tmp_err);
        _ch_chirp_uninit(chirp, uninit);
        return tmp_err;
    }
#ifndef CH_WITHOUT_TLS
    ch_encryption_t* enc = &ichirp->encryption;
    if (!tmp_conf->DISABLE_ENCRYPTION) {
        ch_en_init(chirp, enc);
        tmp_err = ch_en_start(enc);
        if (tmp_err != CH_SUCCESS) {
#ifdef CH_ENABLE_LOGGING
            ERR_print_errors_fp(stderr);
#endif
            E(chirp, "Could not start encryption: %d", tmp_err);
            _ch_chirp_uninit(chirp, uninit);
            return tmp_err;
        }
    }
#endif
#ifdef CH_ENABLE_LOGGING
    char id_str[CH_ID_SIZE * 2 + 1];
    ch_bytes_to_hex(
            ichirp->identity, sizeof(ichirp->identity), id_str, sizeof(id_str));
    LC(chirp,
       "Chirp initialized id: %s. ",
       "uv_loop_t:%p",
       id_str,
       (void*) loop);
#endif
    _ch_chirp_init_signals(chirp);
    uninit |= CH_UNINIT_SIGNAL;
    if (uv_async_send(&ichirp->start) < 0) {
        E(chirp, "Could not call start callback", CH_NO_ARG);
        _ch_chirp_uninit(chirp, uninit);
        return CH_UV_ERROR;
    }
#ifndef NDEBUG
    if (_ch_tst_fail_init_at_end) {
        _ch_chirp_uninit(chirp, uninit);
        return CH_INIT_FAIL;
    } else {
        uv_mutex_unlock(&_ch_chirp_init_lock);
        return CH_SUCCESS;
    }
#else
    uv_mutex_unlock(&_ch_chirp_init_lock);
    return CH_SUCCESS;
#endif
}

// .. c:function::
void
ch_chirp_finish_message(
        ch_chirp_t* chirp, ch_connection_t* conn, ch_message_t* msg, int status)
//    :noindex:
//
//    see: :c:func:`ch_chirp_finish_message`
//
// .. code-block:: cpp
//
{
    char flags = msg->_flags;
    if (flags & CH_MSG_ACK_RECEIVED && flags & CH_MSG_WRITE_DONE) {
        msg->_flags &= ~(CH_MSG_ACK_RECEIVED | CH_MSG_WRITE_DONE);
#ifdef CH_ENABLE_LOGGING
        {
            char  id[CH_ID_SIZE * 2 + 1];
            char* action = "Success";
            if (status != CH_SUCCESS) {
                action = "Failure:";
            }
            ch_bytes_to_hex(
                    msg->identity, sizeof(msg->identity), id, sizeof(id));
            if (msg->type & CH_MSG_ACK) {
                LC(chirp,
                   "%s: sending ACK message id: %s\n"
                   "                            ",
                   "ch_message_t:%p",
                   action,
                   id,
                   (void*) msg);
            } else if (msg->type & CH_MSG_NOOP) {
                LC(chirp,
                   "%s: sending NOOP\n",
                   "ch_message_t:%p",
                   action,
                   (void*) msg);
            } else {
                LC(chirp,
                   "%s: finishing message id: %s\n"
                   "                            ",
                   "ch_message_t:%p",
                   action,
                   id,
                   (void*) msg);
            }
        }
#else
        (void) (chirp);
#endif
        uv_timer_stop(&conn->writer.send_timeout);
        msg->_flags &= ~CH_MSG_USED;
        if (msg->_send_cb != NULL) {
            /* The user may free the message in the cb */
            ch_send_cb_t cb = msg->_send_cb;
            msg->_send_cb   = NULL;
            cb(chirp, msg, status);
        }
    }
    if (conn->remote != NULL) {
        ch_wr_process_queues(conn->remote);
    } else {
        A(conn->flags & CH_CN_SHUTTING_DOWN, "Expected shutdown");
        /* Late write callback after shutdown. These are perfectly valid, since
         * we clear the remote early to improve consistency. We have to lookup
         * the remote. */
        ch_remote_t  key;
        ch_remote_t* remote = NULL;
        ch_rm_init_from_conn(chirp, &key, conn, 1);
        if (ch_rm_find(chirp->_->protocol.remotes, &key, &remote) ==
            CH_SUCCESS) {
            ch_wr_process_queues(remote);
        }
    }
}

CH_EXPORT
void
ch_chirp_release_msg_slot(
        ch_chirp_t* rchirp, ch_message_t* msg, ch_release_cb_t release_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_release_msg_slot`
//
// .. code-block:: cpp
//
{
    ch_buffer_pool_t* pool = msg->_pool;
    ch_connection_t*  conn = pool->conn;
    if (!(msg->_flags & CH_MSG_HAS_SLOT)) {
        fprintf(stderr,
                "%s:%d Fatal: Message does not have a slot. "
                "ch_buffer_pool_t:%p\n",
                __FILE__,
                __LINE__,
                (void*) pool);
        return;
    }
    int call_cb = 1;
    /* If the connection does not exist, it is already shutdown. The user may
     * release a message after a connection has been shutdown. We use reference
     * counting in the buffer pool to delay ch_free of the pool. */
    if (conn && !(conn->flags & CH_CN_SHUTTING_DOWN)) {
        ch_chirp_t* chirp = conn->chirp;
        A(chirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
        if (msg->_flags & CH_MSG_SEND_ACK) {
            msg->_flags &= ~CH_MSG_SEND_ACK;
            /* Send the ack to the connection, in case the user changed the
             * message for his need, which is absolutely ok, and valid use
             * case. */
            ch_message_t* ack_msg = &conn->ack_msg;
            memcpy(ack_msg->identity, msg->identity, CH_ID_SIZE);
            ack_msg->user_data = rchirp;
            A(ack_msg->_release_cb == NULL, "ack_msg in use");
            ack_msg->_release_cb = release_cb;
            conn->release_serial = msg->serial;
            call_cb              = 0;
            ch_wr_send(chirp, ack_msg, _ch_chirp_ack_send_cb);
        }
    }
    if (msg->_flags & CH_MSG_FREE_DATA) {
        ch_free(msg->data);
    }
    if (msg->_flags & CH_MSG_FREE_HEADER) {
        ch_free(msg->header);
    }
    if (call_cb) {
        if (release_cb != NULL) {
            release_cb(rchirp, msg->identity, msg->serial);
        }
    }
    int pool_is_empty = ch_bf_is_exhausted(pool);
    ch_bf_release(pool, msg->_slot);
    /* Decrement refcnt and free if zero */
    ch_bf_free(pool);
    if (pool_is_empty && conn) {
        ch_pr_restart_stream(conn);
    }
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_release_msg_slot_ts(
        ch_chirp_t* rchirp, ch_message_t* msg, ch_release_cb_t release_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_release_msg_slot_ts`
//
// .. code-block:: cpp
//
{
    A(rchirp->_init == CH_CHIRP_MAGIC, "Not a ch_chirp_t*");
    A(msg->_release_cb == NULL, "Message already released");
    msg->_release_cb       = release_cb;
    ch_chirp_int_t* ichirp = rchirp->_;
    uv_mutex_lock(&ichirp->release_ts_queue_lock);
    ch_msg_enqueue(&ichirp->release_ts_queue, msg);
    uv_mutex_unlock(&ichirp->release_ts_queue_lock);
    if (uv_async_send(&ichirp->release_ts) < 0) {
        E(rchirp, "Could not call release_ts callback", CH_NO_ARG);
        return CH_UV_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
void
ch_chirp_release_ts_cb(uv_async_t* handle)
//    :noindex:
//
//    see: :c:func:`ch_chirp_release_ts_cb`
//
// .. code-block:: cpp
//
{
    ch_chirp_t* chirp = handle->data;
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    uv_mutex_lock(&ichirp->release_ts_queue_lock);
    ch_message_t* cur;
    ch_msg_dequeue(&ichirp->release_ts_queue, &cur);
    while (cur != NULL) {
        uv_mutex_unlock(&ichirp->release_ts_queue_lock);
        ch_chirp_release_msg_slot(chirp, cur, cur->_release_cb);
        uv_mutex_lock(&ichirp->release_ts_queue_lock);
        ch_msg_dequeue(&ichirp->release_ts_queue, &cur);
    }
    uv_mutex_unlock(&ichirp->release_ts_queue_lock);
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_chirp_run(
        const ch_config_t* config,
        ch_chirp_t**       chirp_out,
        ch_recv_cb_t       recv_cb,
        ch_start_cb_t      start_cb,
        ch_done_cb_t       done_cb,
        ch_log_cb_t        log_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_run`
//
// .. code-block:: cpp
//
{
    ch_chirp_t chirp;
    uv_loop_t  loop;
    ch_error_t tmp_err;
    if (chirp_out == NULL) {
        return CH_NOT_INITIALIZED;
    }
    *chirp_out = NULL;

    tmp_err    = ch_loop_init(&loop);
    chirp._log = NULL; /* Bootstrap order problem. E checks _log but
                        * ch_chirp_init() will initialize it. */
    if (tmp_err != CH_SUCCESS) {
        EC((&chirp),
           "Could not init loop: %d. ",
           "uv_loop_t:%p",
           tmp_err,
           (void*) &loop);
        return CH_INIT_FAIL;
    }
    tmp_err = ch_chirp_init(
            &chirp, config, &loop, recv_cb, start_cb, done_cb, log_cb);
    if (tmp_err != CH_SUCCESS) {
        EC((&chirp),
           "Could not init chirp: %d. ",
           "ch_chirp_t:%p",
           tmp_err,
           (void*) &chirp);
        return tmp_err;
    }
    chirp._->flags |= CH_CHIRP_AUTO_STOP;
    LC((&chirp), "UV-Loop run by chirp. ", "uv_loop_t:%p", (void*) &loop);
    /* This works and is not TOO bad because the function blocks. */
    // cppcheck-suppress autoVariables
    *chirp_out = &chirp;
    tmp_err    = ch_run(&loop);
    *chirp_out = NULL;
    if (tmp_err != 0) {
        fprintf(stderr,
                "uv_run returned with error: %d. uv_loop_t:%p",
                tmp_err,
                (void*) &loop);
        return tmp_err;
    }
    if (ch_loop_close(&loop)) {
        return CH_UV_ERROR;
    }
    return CH_SUCCESS;
}

// .. c:function::
CH_EXPORT
void
ch_chirp_set_auto_stop_loop(ch_chirp_t* chirp)
//    :noindex:
//
//    see: :c:func:`ch_chirp_set_auto_stop_loop`
//
//    This function is thread-safe
//
// .. code-block:: cpp
//
{
    ch_chirp_check_m(chirp);
    chirp->_->flags |= CH_CHIRP_AUTO_STOP;
}

// .. c:function::
CH_EXPORT
void
ch_chirp_set_log_callback(ch_chirp_t* chirp, ch_log_cb_t log_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_set_log_callback`
//
// .. code-block:: cpp
//
{
    ch_chirp_check_m(chirp);
    chirp->_log = log_cb;
}

// .. c:function::
CH_EXPORT
void
ch_chirp_set_public_port(ch_chirp_t* chirp, uint16_t port)
//    :noindex:
//
//    see: :c:func:`ch_chirp_set_public_port`
//
// .. code-block:: cpp
//
{
    ch_chirp_check_m(chirp);
    chirp->_->public_port = port;
}

// .. c:function::
CH_EXPORT
void
ch_chirp_set_recv_callback(ch_chirp_t* chirp, ch_recv_cb_t recv_cb)
//    :noindex:
//
//    see: :c:func:`ch_chirp_set_recv_callback`
//
// .. code-block:: cpp
//
{
    ch_chirp_check_m(chirp);
    ch_chirp_int_t* ichirp = chirp->_;
    ichirp->recv_cb        = recv_cb;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_libchirp_cleanup(void)
//    :noindex:
//
//    see: :c:func:`ch_libchirp_cleanup`
//
// .. code-block:: cpp
//
{
    A(_ch_libchirp_initialized, "Libchirp is not initialized");
    if (!_ch_libchirp_initialized) {
        fprintf(stderr,
                "%s:%d Fatal: Libchirp is not initialized.\n",
                __FILE__,
                __LINE__);
        return CH_VALUE_ERROR;
    }
    _ch_libchirp_initialized = 0;
    uv_mutex_destroy(&_ch_chirp_init_lock);
#ifndef CH_WITHOUT_TLS
    ch_error_t ret = ch_en_tls_cleanup();
#else
    ch_error_t ret = CH_SUCCESS;
#endif
#ifdef CH_ENABLE_ASSERTS
    ch_at_cleanup();
#endif
    return ret;
}

// .. c:function::
CH_EXPORT
ch_error_t
ch_libchirp_init(void)
//    :noindex:
//
//    see: :c:func:`ch_libchirp_init`
//
// .. code-block:: cpp
//
{
    A(!_ch_libchirp_initialized, "Libchirp is already initialized");
    if (_ch_libchirp_initialized) {
        fprintf(stderr,
                "%s:%d Fatal: Libchirp is already initialized.\n",
                __FILE__,
                __LINE__);
        return CH_VALUE_ERROR;
    }
    srand(((unsigned int) time(NULL)) | ((unsigned int) uv_hrtime()));
    _ch_libchirp_initialized = 1;
    if (uv_mutex_init(&_ch_chirp_init_lock) < 0) {
        return CH_INIT_FAIL;
    }
#ifdef CH_ENABLE_ASSERTS
    ch_at_init();
#endif
#ifndef CH_WITHOUT_TLS
    return ch_en_tls_init();
#else
    return CH_SUCCESS;
#endif
}

// .. c:function::
CH_EXPORT
int
ch_loop_close(uv_loop_t* loop)
//    :noindex:
//
//    see: :c:func:`ch_loop_close`
//
// .. code-block:: cpp
//
{
    int tmp_err;
    tmp_err = uv_loop_close(loop);
#ifdef CH_ENABLE_LOGGING
    if (tmp_err != CH_SUCCESS) {
        fprintf(stderr,
                "%s:%d WARNING: Closing loop exitcode:%d. uv_loop_t:%p\n",
                __FILE__,
                __LINE__,
                tmp_err,
                (void*) loop);
    }
#endif
    return tmp_err;
}

// .. c:function::
CH_EXPORT
int
ch_loop_init(uv_loop_t* loop)
//    :noindex:
//
//    see: :c:func:`ch_loop_init`
//
// .. code-block:: cpp
//
{
    int tmp_err;
    uv_mutex_lock(&_ch_chirp_init_lock);
    tmp_err = uv_loop_init(loop);
    uv_mutex_unlock(&_ch_chirp_init_lock);
    return tmp_err;
}

// .. c:function::
CH_EXPORT
int
ch_run(uv_loop_t* loop)
//    :noindex:
//
//    see: :c:func:`ch_run`
//
// .. code-block:: cpp
//
{
    int tmp_err = uv_run(loop, UV_RUN_DEFAULT);
    if (tmp_err != 0) {
        /* uv_stop() was called and there are still active handles or requests.
         * This is clearly a bug in chirp or user code, we try to recover with
         * a warning. */
        fprintf(stderr, "WARNING: Cannot close all uv-handles/requests.\n");
        tmp_err = uv_run(loop, UV_RUN_NOWAIT);
        /* Now we have serious a problem */
        if (tmp_err != 0) {
            fprintf(stderr, "FATAL: Cannot close all uv-handles/requests.\n");
        }
    }
    return tmp_err;
}
