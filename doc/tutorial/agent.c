// =========
// The agent
// =========
//
// Includes
// ========
//
// We need some basic includes to get going. Note the :file:`libchirp.h`, which
// of course is chirp itself, and :file:`common.h`, which is our own header
// where we have put some common data structures and utilities.
//
// .. code-block:: cpp
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "libchirp.h"

// A few global data structures
// ============================
//
// **Chirp**: This is our chirp instance. This is the central piece of our
// monitor.
//
// .. code-block:: cpp

static ch_chirp_t* _chirp_instance;

// **Shutdown flag**:
// Since we want to let upstream know if we're shutting down cleanly, we also
// need a flag to globally note that we're going to shut down. See
// ``notify_status`` to see why.
//
// .. code-block:: cpp

static int shutting_down;

// Declaring our functions
// =======================
//
// If you know C, you know we need to declare our functions, so we can
// implement them in an order that makes sense, instead of the utility
// functions first, which may be confusing for a casual reader.
//
// Don't worry about those, we're going to explain them in detail just
// a bit further down.
//
// .. code-block:: cpp

static void
notify_status(int svc_status, int agent_status);
static void
sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status);
static void
connection_timeout_cb(uv_timer_t* timer);
static void
poll_connected_cb(uv_connect_t* conn, int status);
static void
socket_close_cb(uv_handle_t* handle);

// Monitoring the service
// ======================
//
// Service host and port
// ---------------------
//
// This will store the hostname and port of the service we're monitoring.
//
// .. code-block:: cpp

static char* service_host;
static int   service_port;

// Polling information
// -------------------
//
// Timers, interval and sockets that we need to check upon the service.
//
// .. code-block:: cpp

static uv_timer_t    poll_timer;
static uv_timer_t    poll_timeout_timer;
static uv_tcp_t      poll_socket;
static uv_connect_t  poll_connect;
uv_shutdown_t        poll_closer;

// Poll the service, report it's status
// ------------------------------------
//
// In this function, we try to connect to our betrusted service, and setup a
// timer so we can react if it takes too long to connect. This function is
// called repeatedly via a ``libuv`` timer.
//
// .. code-block:: cpp

static
void
do_poll_cb(uv_timer_t* timer)
{
    (void)(timer);
    uv_tcp_init(ch_chirp_get_loop(_chirp_instance), &poll_socket);

    struct sockaddr_in dest;

    uv_ip4_addr(service_host, service_port, &dest);
    uv_tcp_connect(
        &poll_connect, &poll_socket,
        (const struct sockaddr*)&dest,
        poll_connected_cb);

    uv_timer_init(ch_chirp_get_loop(_chirp_instance), &poll_timeout_timer);

    // We wait for two seconds before we assume the service "down".
    uv_timer_start(&poll_timeout_timer, connection_timeout_cb, 2000, 0);

}


// Socket shutdown callback
// ------------------------
//
// This is how ``libuv`` shuts down a socket: You "request" a shutdown, which
// triggers a callback, then you can close the socket - which triggers another
// callback. This callback here responds to the completed shutdown request.
//
// .. code-block:: cpp

static
void
socket_shutdown_cb(uv_shutdown_t* req, int status)
{
    (void)(status);

    uv_close((uv_handle_t*)req->handle, socket_close_cb);
}

// Socket closed callback
// ----------------------
//
// This is the second stage after ``socket_shutdown_cb``. Required by
// ``libuv``, but we're not really doing anything here.
//
// .. code-block:: cpp

static
void
socket_close_cb(uv_handle_t* handle)
{
    (void)handle;
}


// Connection callback
// -------------------
//
// When we receive this callback, the connection request has completed.
// The ``status`` argument lets us know if the connection was successful.
//
// .. code-block:: cpp

static
void
poll_connected_cb(uv_connect_t* conn, int status)
{
    /* Let upstream know if the service is OK: If status == 0, everything is
     * fine. Otherwise, the connection was refused. */
    notify_status(status == 0, 1);

    /* Also stop the timer - No need for waiting to see if the connection
    * times out anymore. */
    uv_timer_stop(&poll_timeout_timer);

    /* We don't really want to talk to the service, so immediately request a
     * shutdown of the socket..  */
    uv_shutdown(&poll_closer, (uv_stream_t*)conn->handle, socket_shutdown_cb);
}

// Callback when we could not connect to our service after some time
// -----------------------------------------------------------------
//
// When we receive this callback, some time elapsed since we tried to
// connect to the service. This usually means the service is around, but
// very busy. Let's report it as "down" in that case (maybe we'll add a
// timeout status later...)
//
// .. code-block:: cpp

static
void
connection_timeout_cb(uv_timer_t* timer)
{
    (void)(timer);

    notify_status(0, 1);

    /* Shutdown the connection, we had a timeout */
    uv_shutdown(
        &poll_closer,
        (uv_stream_t*)poll_connect.handle,
        socket_shutdown_cb
    );
}

// Upstream communication
// ======================
//
// Upstream host and port
// ----------------------
//
// We need to know where to forward our messages.
//
// .. code-block:: cpp

static char* upstream_host;
static int   upstream_port;


// Message and service info
// ------------------------
//
// For simplicity in this tutorial, we allocate a message along with the
// service info statically. In a production environment, you'd want to
// allocate/free this on an as-needed basis.

ch_message_t  msg;
mon_service_t svc;

// Notify the upstream
// -------------------
//
// This helper is called to notify our upstream about our service's status.
//
// .. code-block:: cpp

static void
notify_status(int svc_status, int agent_status)
{
    snprintf(svc.name, 32, "%s:%d", service_host, service_port);

    svc.status = 0;
    if (svc_status) {
        svc.status |= SERVICE_STATUS_ALIVE;
    }
    if (agent_status) {
        svc.status |= SERVICE_STATUS_AGENT_ALIVE;
    }

    svc.last_check       = time(NULL);

    ch_msg_init(&msg);
    ch_msg_set_address(&msg, AF_INET, upstream_host, upstream_port);

    /* Note: This is very naive, and any self-respecting code should use a
     * proper serializer for this.
     */
    ch_msg_set_data(&msg, (ch_buf*)&svc, sizeof(svc));
    ch_chirp_send(_chirp_instance, &msg, sent_cb);

}

// Received callback
// -----------------
//
// In the agent, we don't really do anything. But Chirp doesn't know this and
// still requires a callback for incoming messages.
//
// Maybe later we'll respond to health checks requests from peers or something
//
// .. code-block:: cpp

static
void
new_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    (void)(chirp);
    ch_chirp_release_message(msg);
}


// Sent callback
// -------------
//
// This is needed to clean up after a message has been sent upstream.
//
// Since this is the last callback when we're shutting down, we
// need to know that and finish the job.
//
// .. code-block:: cpp

static
void
sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status)
{
    (void)(status);
    (void)(msg);

    if (shutting_down) {
        ch_chirp_close_ts(chirp);
    }
}

// Shutdown via :kbd:`Ctrl-C`
// ==========================
//
// We use this to be notified when the user hits :kbd:`Ctrl-C`, so we can
// notify upstream before exiting. Otherwise, the monitor wouldn't know that we
// exited, and still list the service, even if it's legitimately not monitored
// anymore.
//
// .. code-block:: cpp

static uv_signal_t   sighandler;

// Signal handler for exiting
// --------------------------
//
// We want to properly clean up when the user hits :kbd:`Ctrl-C`. This signal
// handler needs to clean up our polling timer, free the services data
// structure, and let Chirp know we're done.
//
// .. code-block:: cpp

static
void
sig_handler_cb(uv_signal_t* handle, int signum)
{
    (void)(handle);
    (void)(signum);

    shutting_down = 1;
    // First, we stop the timers. We don't need them anymore.
    uv_timer_stop(&poll_timeout_timer);
    uv_timer_stop(&poll_timer);

    /* TODO: we should shutdown the socket as well if we're currently
     * trying to connect... */

    /* Second, we need to tell upstream that we're gone and won't be reporting
     * on the service anymore */
    notify_status(0, 0);
}


// Startup and Initialisation
// ==========================

// Startup callback
// ----------------
//
// Chirp triggers a callback when it has finished initializing itself.  We
// setup our polling timer and signal handler here.
//
// .. code-block:: cpp

static
void
chirp_started_cb(ch_chirp_t* chirp)
{
    uv_timer_init(ch_chirp_get_loop(chirp), &poll_timer);
    poll_timer.data = chirp;

    // we're not shutting down just yet
    shutting_down = 0;

    // Setup a signal handler, so we can shut down gracefully on Ctrl+C
    uv_signal_init(ch_chirp_get_loop(chirp), &sighandler);
    uv_signal_start_oneshot(&sighandler, sig_handler_cb, SIGINT);

    uv_timer_start(&poll_timer, do_poll_cb, 1, svc.polling_interval * 1000);

}

// The main program
// ----------------
//
// For the agent, we parse a whopping four parameters:
//
// * First, we need the port where we will listen (to configure our own
//   chirp instance)
// * Second, we need to know our upstream. This should be a string with
//   a hostname, followed by a colon and the port number.
// * Third, we of course need to know the service we're monitoring
// * Last, an interval, in seconds where we're polling the service's
//   availability
//
// We'll verify the parameters, then setup chirp and get going :)
//
// .. code-block:: cpp

int
main(int argc, char *argv[])
{
    if(argc < 5) {
        fprintf(
            stderr,
            "Usage: %s listen_port "
            "upstream_host:upstream_port "
            "service_host:service_port "
            "interval\n",
            argv[0]
        );
        exit(1);
    }
    int port = strtol(argv[1], NULL, 10);
    if(errno) {
        fprintf(stderr, "port must be integer.\n");
        exit(1);
    }
    if(port <= 1024) {
        fprintf(stderr, "port must be greater than 1024.\n");
        exit(1);
    }
    if(port > 0xFFFF) {
        fprintf(stderr, "port must be less than %d.\n", 0xFFFF);
        exit(1);
    }

    upstream_host = argv[2];
    service_host  = argv[3];

    parse_hostport_into_port(upstream_host, &upstream_port);
    parse_hostport_into_port(service_host, &service_port);

    printf("Agent, listening on port %d\n", port);

    printf(
        "Upstream: host=%s port=%d\n",
        upstream_host, upstream_port
    );

    printf(
        "Service: host=%s port=%d\n",
        service_host, service_port
    );

    svc.polling_interval = strtol(argv[4], NULL, 10);
    if(svc.polling_interval <= 2) {
        /* The interval must be over two seconds, as this is the time
         * we allow the service to connect. We're using a repeating timer,
         * so if this would be two seconds or shorter, libuv would topple
         * itself by triggering another service poll before the previous one
         * has completed.
         *
         * Sane monitoring would only poll like every minute or so anyway. */
        fprintf(stderr, "Interval must be more than 2 seconds\n");
        exit(1);
    }


    /* Initialize chirp. This initializes just the global data structures, and
     * needs to be done once per program run (not for each chirp instance!) */
    ch_libchirp_init();

    /* Setup a config structure. The defaults are quite sane, so no need to
     * change everything. Just note that for this tutorial, we disable
     * encryption, so we don't have to setup all the TLS keys anc certs.  */
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.PORT               = port;
    config.DISABLE_ENCRYPTION = 1;
    config.DISABLE_SIGNALS    = 1;

    /* Start chirp. This will not return until the event loop has ended. This
     * will also set the output parameter ``_chirp_instance``, so we can later
     * access it while the program runs. */
    ch_chirp_run(
        &config,
        &_chirp_instance,
        new_message_cb,
        chirp_started_cb,
        NULL,
        NULL
    );

    // Before we exit, let's do some cleanup of the global data structures.
    ch_libchirp_cleanup();

    return 0;
}

// .. vim: set sw=4 ts=4 et:
