// SPDX-FileCopyrightText: David Vogt <david.vogt@adfinis-sygroup.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =========================================
// Some common data structures and functions
// =========================================
//
// First, we construct some common data structures that are shared between the
// monitor and agent.
//
//
// Includes
// ========
//
// We need stdint.h for the exact integer types we're using.
//
// .. code-block:: cpp

#include <stdint.h>
#include <time.h>

// Service and status
// ==================
//
// We need to be able to represent a service, and it's status.
//
// The service status is a simple bitfield that contains slots
// for representing the status of a service as well as that of an agent.
//
// .. code-block:: cpp

typedef enum {
    SERVICE_STATUS_ALIVE       = 1 << 0,
    SERVICE_STATUS_AGENT_ALIVE = 1 << 1,
} service_status_t;

// The service status is a bitmask that fulfills several purposes. First, it
// tells the upstream about the service status (alive, or not). Second, it
// tells about it's own status. This is used as an "unsubscribe" request: If
// the ``SERVICE_STATUS_AGENT_ALIVE`` flag is set to zero, this will most
// likely be the last message from the agent, and upstream should remove the
// service from it's list.
//
// Said flag is also used within the monitor service itself, for a slightly
// different purpose: It will be set if the agent didn't send a messgae for a
// given amount of time, in which case the service's status will be updated
// accordingly in the output.

// Service object
// --------------
//
// .. code-block:: cpp

typedef struct mon_service_s {
    char     name[32];
    uint16_t polling_interval;
    time_t   last_check;
    uint8_t  status;
} mon_service_t;

// The service object represents a service with it's status, both on the wire
// and in memory of the monitor service. Next to the service's name, we tell
// the upstream the polling interval that's being used, and a timestamp of
// the last time a check has been done. The last part is the service's status as
// described above.

// A helper for parsing host:port strings
// ======================================
//
// Note: this destructively parses the ``hostport`` parameter, so that
// it can be used as the host after the parsing. the port is converted
// to an int and returned via output parameter ``port``.
//
// .. code-block:: cpp

static void
parse_hostport_into_port(char* hostport, int* port)
{
    char* port_str = strstr(hostport, ":");
    if (port_str == NULL) {
        fprintf(stderr, "Upstream format must be host:port\n");
        exit(1);
    }
    port_str++; // port start AFTER the colon..
    *port = strtol(port_str, NULL, 10);

    /* Hack - replace colon with NUL byte, so hostport becomes a string
     * that only contains the hostname. This is "safe", as we've checked for
     * the existance of the colon already */
    *strstr(hostport, ":") = '\0';
}

// .. vim: set sw=4 ts=4 et:
