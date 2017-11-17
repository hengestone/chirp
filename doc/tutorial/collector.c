// =============
// The collector
// =============
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

#include "common.h"
#include "libchirp.h"

// Global data structure - Chirp instance
// ======================================
//
// This is our chirp instance. This is the central piece of our monitor.
//
// .. code-block:: cpp

static ch_chirp_t* _chirp_instance;

// Upstream: Notifying monitor
// ===========================
//

// Upstream host and port
// ----------------------
//
// We need to know where to forward our messages.
//
// .. code-block:: cpp

static char* upstream_host;
static int   upstream_port;

// Sent callback
// -------------
//
// This is needed to clean up after a message has been sent upstream.
// We use it to release the recv handler from the message that we received.
//
// .. code-block:: cpp

static void
sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status)
{
    (void) (chirp);
    (void) (status);
    ch_chirp_release_message(msg);
}

// Listening for incoming messages
// ===============================

// Received callback
// -----------------
//
// We just forward the message to our upstream and declare our work done.
//
// But take note: Chrip expects us to release the recv handler when done, so
// don't forget that! We're doing this only after forwarding the message, so
// check ``sent_cb`` to see it.
//
// .. code-block:: cpp

static void
new_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    // Set upstream address, send out message
    ch_msg_set_address(msg, AF_INET, upstream_host, upstream_port);
    ch_chirp_send(chirp, msg, sent_cb);
}

// Startup and initialisation
// ==========================
//
// Startup callback
// ----------------
//
// Chirp needs a function to let us know when it has finished initializing.
//
// TODO: Can we leave this out if it's not doing anything? ie. pass it as NULL
// to ``ch_chirp_run``?
//
// .. code-block:: cpp

static void
chirp_started_cb(ch_chirp_t* chirp)
{
    (void) (chirp);
}

// The main program
// ----------------
//
// For the collector, we parse two parameters:
//
// * First, we need the port where we will listen (to configure our own
//   chirp instance)
// * Second, we need to know our upstream. This should be a string with
//   a hostname, followed by a colon and the port number.
//
// We'll verify the parameters, then setup chirp and get going :)
//
// .. code-block:: cpp

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s listen_port upstream_host:upstream_port\n",
                argv[0]);
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
        fprintf(stderr, "port must be less than %d.\n", 0xFFFF);
        exit(1);
    }

    upstream_host = argv[2];

    parse_hostport_into_port(upstream_host, &upstream_port);

    printf("Collector, listening on port %d\n", port);
    printf("Upstream: host=%s port=%d\n", upstream_host, upstream_port);

    /* Initialize chirp. This initializes just the global data structures, and
     * needs to be done once per program run (not for each chirp instance!) */
    ch_libchirp_init();

    /* Setup a config structure. The defaults are quite sane, so no need to
     * change everything. Just note that for this tutorial, we disable
     * encryption, so we don't have to setup all the TLS keys anc certs. */
    ch_config_t config;
    ch_chirp_config_init(&config);
    config.PORT               = port;
    config.DISABLE_ENCRYPTION = 1;

    /* Start chirp. This will not return until the event loop has ended. This
     * will also set the output parameter ``_chirp_instance``, so we can later
     * access it while the program runs. */
    ch_chirp_run(
            &config,
            &_chirp_instance,
            new_message_cb,
            chirp_started_cb,
            NULL,
            NULL);

    // Before we exit, let's do some cleanup of the global data structures.
    ch_libchirp_cleanup();

    return 0;
}

// .. vim: set sw=4 ts=4 et:
