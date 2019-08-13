// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

// =========
// Constants
// =========
//
// Defines constants used throughout the project.
//
// .. code-block:: cpp

#ifndef ch_libchirp_const_h
#define ch_libchirp_const_h

// System includes
// ===============
//
// .. code-block:: cpp
//
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else // _WIN32
#include <arpa/inet.h>
#endif // _WIN32

// Declarations
// ============

// .. c:type:: ch_ip_protocol_t
//
//    This definition is for documentation purposes only. The user should use
//    AF_INET/AF_INET6.
//
//    IP protocol definition. This is either IPV4 or IPV6.
//
//    .. c:member:: CH_IPV4
//
//       Defines the usage of IP protocol version 4.
//
//    .. c:member:: CH_IPV6
//
//       Defines the usage of IP protocol version 6.
//
// .. code-block:: cpp
//
typedef enum { _CH_IPV4 = AF_INET, _CH_IPV6 = AF_INET6 } ch_ip_protocol_t;

// The maximum size of an IP address
//
// .. code-block:: cpp
//
#define CH_IP_ADDR_SIZE 16

// The size of an IP4 address
//
// .. code-block:: cpp
//
#define CH_IP4_ADDR_SIZE 4

// The size of an id
//
// .. code-block:: cpp
//
#define CH_ID_SIZE 16

#endif // ch_libchirp_const_h
