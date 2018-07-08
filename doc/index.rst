========
libchirp
========

.. image:: https://raw.githubusercontent.com/concretecloud/chirp/master/doc/_static/chirp.png
   :width: 30%

Supported by |adsy|

.. |adsy| image:: https://1042.ch/ganwell/adsy-logo.svg
   :target: https://adfinis-sygroup.ch/

Message-passing for everyone
============================

.. note::

   On some platforms libuv writes can cause SIGPIPE, generally you want to
   ignore the SIGPIPE and handle the error. Most applications ignore SIGPIPE.
   Since libuv and libchirp are libraries you have to ignore SIGPIPE, if you
   want to.

   .. code-block:: cpp

      signal(SIGPIPE, SIG_IGN);

Features
========

* Fully automatic connection setup

* TLS support

  * Connections to 127.0.0.1 and ::1 aren't encrypted
  * We support and test with OpenSSL and LibreSSL

* Easy message routing

* Robust

  * No message can be lost without an error (in sync mode)

* Very thin API

* Minimal code-base, all additional features will be implemented as modules in
  an upper layer

* Fast

  * Up to 240'000 msg/s on a single-connection, in an unrealistic test:

    * Run on loopback

    * Nothing was executed

    * No encryption

    * Asynchronous

    * The test shows that chirp is highly optimized, but if the network
      delay is bigger star- or mesh-topology can improve throughput.

  * Up to 55'000 msg/s in synchronous mode

.. _modes-of-operation:

Modes of operation
==================

The programming interface and internal operation is always asynchronous.
libchirp is asynchronous across multiple connections. Often you want one peer to
be synchronous and the other asynchronous, depending on what pattern you
implement.

Connection-synchronous (config.SYNCHRONOUS=1)
---------------------------------------------

* The sender requests and waits for a acknowledge message

* The send callback only returns a success when the remote has called
  ch_chirp_release_msg_slot

* No message can be lost by chirp

* If the application calls ch_chirp_release_msg_slot after the operation is
  finished, messages will automatically be throttled. Be aware of the timeout:
  if the applications operation takes longer either increase the timeout or copy
  the message (with copying you lose the throttling)

* Slower

Connection-asynchronous (config.SYNCHRONOUS=0)
----------------------------------------------

* The send callback returns a success when the message is successfully written to
  the operating system

* If unexpected errors (ie. remote dies) happen, the message can be lost in the
  TCP-buffer

* Automatic concurrency, by default chirp uses 16 concurrent message-slots

* The application needs a scheduler that periodically checks that operations
  have completed

* Faster

What should I use?
------------------

Rule of thumb:

* Consumers (workers) are not synchronous (SYNCHRONOUS=0)

* Producers are synchronous if they don't do bookkeeping (SYNCHRONOUS=1)

* If you route messages from a synchronous producer, you want to be synchronous
  too: Timeouts get propagated to the producer.

For simple message transmission, for example sending events to a time-series
database we recommend config.SYNCHRONOUS=1, since chirp will cover this process
out of the box.

For more complex application where you have to schedule your operations anyway,
use config.SYNCHRONOUS=0, do periodic bookkeeping and resend failed
operations.

Diving in
=========

.. toctree::
   :maxdepth: 2

   building.rst
   example.rst
   tutorial/index.rst

API Reference
=============

.. toctree::
   :maxdepth: 3

   config.defs.h.rst
   include/libchirp/chirp.h.rst
   include/libchirp/message.h.rst
   include/libchirp/callbacks.h.rst
   include/libchirp/const.h.rst
   include/libchirp.h.rst
   include/libchirp/encryption.h.rst
   include/libchirp/error.h.rst
   include/libchirp/wrappers.h.rst
   include/libchirp/common.h.rst

.. note::

   The API is not thread-safe except where stated: Functions have \*_ts suffix.
   uv_async_send() can be used.

   Only one thread per :c:type:`ch_chirp_t` object is possible, the
   :c:type:`uv_loop_t` has to run in that thread. \*_ts functions can be called
   from any thread.


Additional information
======================

.. toctree::
   :maxdepth: 3

   development.rst


* :ref:`genindex`
* :ref:`search`
