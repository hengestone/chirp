// ===========
// The monitor
// ===========
//
// The first program that we're going to build is the monitoring part itself.
// It consists of a bunch of output logic, a chirp instance for ingesting
// events, and some data structure to keep track of the services.
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

// Some global variables
// =====================
//
// Chirp instance
// --------------
//
// This is our chirp instance. This is the central piece of our monitor.
//
// .. code-block:: cpp

static ch_chirp_t* _chirp_instance;

// Our own port
// ------------
//
// We need to know our own port. This is only needed since we want to print
// the port on top of the monitoring table. If you don't want it, you can remove
// it along with the corresponding line in ``show_services()`` below.
//
// .. code-block:: cpp

static int port;

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

static mon_service_t*
service_get(mon_service_t* msg_svc);
static char*
service_status_str(int status);
static void
clear_screen();
static void
sig_handler_cb(uv_signal_t* handle, int signum);
int
main(int argc, char* argv[]);

// State management
// ================
//
// We need to manage our internal state. For simplicity's sake, we use a plain
// C array of structs, which we don't even sort. This is a Chirp tutorial after
// all, and not a C one :)
//
// List of services
// ----------------
//
// We need a list of services that we're currently monitoring. As this is a
// simple tutorial, we will not use an optimized data structure such as an RB
// tree or similar.
//
// Instead, we use a simple, unsorted array.
//
// .. code-block:: cpp

static mon_service_t* _services;
static size_t         _services_size = 0;

// Create service
// --------------
//
// This is the counterpart to ``service_remove_if_exists``: If we have a new
// service to monitor, it needs to be added to the list. We resize our array
// of services to accompany one more element, then return it.
//
// .. code-block:: cpp

static mon_service_t*
service_create()
{
    /* Make room for one more, then return the new (uninitialized) service.
     * Note that this, too, is very naive, but short. */
    _services_size++;
    _services = realloc(_services, sizeof(*_services) * _services_size);

    return &_services[_services_size - 1];
}

// Find a service
// ---------------
//
// When we receive a service update, we need to store it's updated state in our
// "database". To do this, we must be able to find the corresponding entry, if
// it exists.
//
// We return NULL if it doesn't, so the calling code can create an entry
// instead.
//
// .. code-block:: cpp

static mon_service_t*
service_get(mon_service_t* msg_svc)
{
    // Try to find the service in our list of services

    if (_services == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < _services_size; i++) {
        if (strncmp(_services[i].name, msg_svc->name, 32) == 0) {
            // Found! Return the service

            return &_services[i];
        }
    }
    return NULL;
}

// Remove a service
// ----------------
//
// This is the counterpart to ``service_create``: If a service should not
// be monitored anymore, it needs to be removed from the list. We do this by
// finding the service, then copying the last one over it, before resizing the
// list of services.
//
// .. code-block:: cpp

static void
service_remove_if_exists(mon_service_t* msg_svc)
{

    mon_service_t* internal_svc = service_get(msg_svc);
    if (internal_svc == NULL) {
        // Nothing to do, service does not exist anymore
        return;
    }

    /* Special care needs to be taken: If removing the last element,
     * we don't want to copy the last element over itself. */
    if (_services_size <= 1) {
        /* Copy last service over the current one, before freeing the
         * last one. */
        memcpy(internal_svc,
               &_services[_services_size - 1],
               sizeof(*internal_svc));
    }

    // Resize storage
    _services_size--;
    _services = realloc(_services, sizeof(*_services) * _services_size);
}

// Updating a service
// -------------------
//
// Update the internal steate "database" with information from the given
// service (from a chirp message).
//
// This creates the service in our list if it didn't exist yet, and
// otherwise updates the existing entry.
//
// .. code-block:: cpp

static mon_service_t*
service_update(mon_service_t* msg_svc)
{
    mon_service_t* internal_svc = service_get(msg_svc);
    if (internal_svc == NULL) {
        internal_svc = service_create();
    }

    // Update our service with the new information
    memcpy(internal_svc, msg_svc, sizeof(*internal_svc));

    return internal_svc;
}

// Received callback
// -----------------
//
// This is where the magic happens: Chirp tells us that we have a new message.
// We decode the message into a ``mon_service_t`` struct and update our state
// with it.
//
// Chrip expects us to release the recv handler when done, so never forget
// that!
//
// .. code-block:: cpp

static void
new_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
{
    (void)(chirp);
    /* Note: This makes some very naive assumptions regarding the peer.  Any
     * respectable network code should NEVER EVER do this! But it keeps the
     * tutorial short, as we don't need code to serialize/unserialize. */
    mon_service_t* svc = (mon_service_t*) msg->data;

    if (svc->status & SERVICE_STATUS_AGENT_ALIVE) {
        service_update(svc);
    } else {
        // Agent is exiting - remove service if exists
        service_remove_if_exists(svc);
    }

    ch_chirp_release_message(msg);
}

// Monitoring output
// =================
//
// Now that we have our simple service database, let's show it to the user.
// We do this by printing out a header, followed by a line for each service.

// Output - Show monitoring state
// ------------------------------
//
// First, let's define how to print a single service. Since we want the same
// alignment for the table headers and the service line, we're using the same
// function to do it.
//
// .. code-block:: cpp

static void
show_service_status(mon_service_t* svc, int show_header)
{
    char* format = "%-32s %22s %10s %-15s\n";

    if (show_header) {
        printf(format, "Service", "Last update", "Interval", "State");
    }

    if (svc != NULL) {
        char      interval_buf[30];
        char      lastcheck_buf[30];
        struct tm lastcheck;
        // Parse polling interval into a number of seconds
        snprintf(interval_buf, 30, "%ds", svc->polling_interval);

        // Parse the last check timestamp
        localtime_r(&svc->last_check, &lastcheck);
        strftime(lastcheck_buf, 30, "%Y-%m-%d %H:%M:%S", &lastcheck);

        printf(format,
               svc->name,
               lastcheck_buf,
               interval_buf,
               service_status_str(svc->status));
    }
}

// Convert a status bitmask into string
// ------------------------------------
//
// The status field in our service struct is in fact a bitmask that can tell
// us several things. Notably the status of the service itself, but also the
// status of the agent.
//
// .. code-block:: cpp

static char*
service_status_str(int status)
{
    switch (status & (SERVICE_STATUS_AGENT_ALIVE | SERVICE_STATUS_ALIVE)) {
    case SERVICE_STATUS_AGENT_ALIVE | SERVICE_STATUS_ALIVE:
        return "Service OK";
        break;
    case SERVICE_STATUS_AGENT_ALIVE:
        return "Service DOWN";
        break;
    case SERVICE_STATUS_ALIVE:
        return "Agent DOWN";
        break;
    default:
        // This is when the service should actually be removed..
        return "UNKNOWN";
    };
}

// Clear the screen
// ----------------
//
// We want a nice table that always starts on top of the screen. We can achieve
// this by running the program ``clear`` on the command line.. Or by just
// printing out the escape sequence that this program would print... So let's
// do that:
//
// .. code-block:: cpp

static void
clear_screen()
{
    char* clear_seq = "\033[3\033[H\033[2J";
    printf("%s", clear_seq);
}

// Output - Show monitoring state
// ------------------------------
//
// We need something to print out the status of our services. One service per
// line, nicely formatted.
//
// .. code-block:: cpp

static void
show_services()
{
    clear_screen();
    printf("Monitor, listening on port %d\n", port);

    show_service_status(NULL, 1);
    for (size_t i = 0; i < _services_size; i++) {
        show_service_status(&_services[i], 0);
    }
}

// Meta-Monitoring and status output
// =================================
//
// Now we're slowly getting to the meat of it. We need a periodic check
// to make sure our agents are still around. And at the same time, we want
// to let the user know if something changed! So we have a periodic timer
// that we use for a sanity check and status update.

// Health check timer
// ------------------
//
// We periodically check our list of services for sanity. Basically, we want to
// know if the agents really update as fast as they promise (ie if they say
// their interval is 10s, we assume after 20s that the agent is gone, and
// update the service accordingly).
//
// .. code-block:: cpp

static uv_timer_t poll_timer;

// Poll and print callback
// -----------------------
//
// In this function, we verify that all monitored services have
// been updated recently. If not, we update the service accordingly.
//
// .. code-block:: cpp

static void
poll_and_print_status_cb(uv_timer_t* timer)
{
    (void)(timer);

    time_t now = time(NULL);
    for (size_t i = 0; i < _services_size; i++) {

        if (now > _services[i].last_check + 2 * _services[i].polling_interval) {
            _services[i].status &= ~SERVICE_STATUS_AGENT_ALIVE;
        }
    }

    /* We show the service's status each second, instead of on update,
     * as this may become quite slow (and flickery) if a larger number of
     * services are updated in a short time */
    show_services();
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

static uv_signal_t sighandler;

// Signal handler for exiting
// --------------------------
//
// We want to properly clean up when the user hits :kbd:`Ctrl-C`. This signal
// handler needs to clean up our polling timer, free the services data
// structure, and let Chirp know we're done.
//
// .. code-block:: cpp

static void
sig_handler_cb(uv_signal_t* handle, int signum)
{
    (void)(handle);
    (void)(signum);

    /* First, we stop the sanity check timer. It's not needed anymore */
    uv_timer_stop(&poll_timer);

    /* Then, free the services list */
    free(_services);
    _services_size = 0;
    ch_chirp_close_ts(_chirp_instance);
}

// Startup and Initialisation
// ==========================

// Startup callback
// ----------------
//
// Chirp triggers a callback when it has finished initializing itself.  We
// setup our check timer and signal handler here, and show the (still empty)
// list of services for the first time.
//
// .. code-block:: cpp

static void
chirp_started_cb(ch_chirp_t* chirp)
{
    // Setup the sanity poll timer, every two seconds
    uv_timer_init(ch_chirp_get_loop(chirp), &poll_timer);
    poll_timer.data = chirp;
    uv_timer_start(&poll_timer, poll_and_print_status_cb, 1, 1000);

    // Setup a signal handler, so we can shut down gracefully on Ctrl+C
    uv_signal_init(ch_chirp_get_loop(chirp), &sighandler);
    uv_signal_start_oneshot(&sighandler, sig_handler_cb, SIGINT);

    // Show the services list for the first time
    show_services();
}

// The main program
// ----------------
//
// This is where it all begins. We parse a single parameter, the TCP port where
// our instance will run, and validate it minimally. Then, we initialize chirp
// and start it, telling it which callback to run when a message arrives..
//
// .. code-block:: cpp

int
main(int argc, char* argv[])
{
    /* Parse the commandline arguments. The monitor is the simplest of all,
     * just accepting a single port number to listen on.
     */
    if (argc < 2) {
        fprintf(stderr, "%s listen_port\n", argv[0]);
        exit(1);
    }
    port = strtol(argv[1], NULL, 10);
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

    /* Initialize chirp. This initializes just the global data structures, and
     * needs to be done once per program run (not for each chirp instance!)  */
    ch_libchirp_init();

    /* Setup a config structure. The defaults are quite sane, so no need to
     * change everything. Just note that for this tutorial, we disable
     * encryption, so we don't have to setup all the TLS keys anc certs. */
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
            NULL);

    // Before we exit, let's do some cleanup of the global data structures.
    ch_libchirp_cleanup();

    return 0;
}

// .. vim: set sw=4 ts=4 et:
