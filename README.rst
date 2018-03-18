========
libchirp
========

Message-passing for everyone

|travis| |rtd| |mpl| |works|

.. |travis| image:: https://travis-ci.org/concretecloud/chirp.svg?branch=master
   :target: https://travis-ci.org/concretecloud/chirp
.. |rtd| image:: https://1042.ch/ganwell/docs-master.svg
   :target: https://docs.adfinis-sygroup.ch/public/chirp/
.. |mpl| image:: https://img.shields.io/badge/license-MPL%202.0-blue.svg
   :target: http://mozilla.org/MPL/2.0/
.. |works| image:: https://img.shields.io/badge/hypothesis-works-blue.svg
   :target: http://hypothesis.works

`Read the Docs`_

.. _`Read the Docs`: https://docs.adfinis-sygroup.ch/public/chirp/

.. raw:: html

    <img alt="chirp"
    src="https://raw.githubusercontent.com/concretecloud/chirp/master/doc/_static/chirp.png"
    width="33%">

Supported by |adsy|

.. |adsy| image:: https://1042.ch/ganwell/adsy-logo.svg
   :target: https://adfinis-sygroup.ch/


BETA-RELEASE: 1.1.0
===================

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

Install
=======

libchirp is distributed as an amalgamation (only needs make and compiler). See
`DIST-README.rst`_. The information below applies to the git repository.

.. _`DIST-README.rst`: https://github.com/concretecloud/chirp/blob/master/mk/DIST-README.rst

Unix
----

Dependencies:

* libuv

* openssl or libressl

Build dependencies:

* python3 [3]_

* make

* gcc or clang

Documentation build dependencies:

* sphinx

* graphviz

Install to prefix /usr/local. (with docs)

.. code-block:: bash

   cd build
   ../configure --doc
   make
   make check doc
   sudo make install

Install to prefix /usr. (without docs)

.. code-block:: bash

   cd build
   ../configure --prefix /usr
   make
   make check
   sudo make install

Install to prefix /usr, but copy to package dir. (Package creation)

.. code-block:: bash

   cd build
   ../configure --prefix /usr
   make
   make check
   make install DEST=pkgdir

.. note::

   On Mac you need to install clang-format using

   brew install clang-format

.. _source_dist:

How to create a source distribution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Building a source distribution is useful when you need to include libchirp in
your project, but don't want to use it's build system. A source distribution can
easily be compiled with just a ``make`` call.

.. code-block:: bash

   cd build
   ../configure --dev --doc
   make dist
   ls dist

.. [3] Script-headers can be patched to work with python2.

Windows
-------

We want to support Windows, but we are currently not building on Windows. VS
2015 or newer should support all C99 feature we use.


Development
===========

Test dependencies:

* cppcheck
* abi-compliance-checker

Unix
----

.. code-block:: bash

   cd build
   ../configure --dev
   make test

In development mode the make file has a help:

.. code-block:: bash

   make

Chirp has a mode to debug macros:

.. code-block:: bash

   ../configure --dev
   make clean all MACRO_DEBUG=True
   gdb src/message_etest

This requires clang-format to be installed.

Running pytest manually with -s for example:

.. code-block:: bash

   cd build
   make all
   pytest -s ../src

Docker
------

If a tool is not available on your platform or you have a old version of
cppcheck (cppcheck is known to behave very different across versions), you can
use the docker based tests.

.. code-block:: bash

   ./ci/alpine.sh

Travis will also run this script, so you can also use it to reproduce errors on
travis.

You can also run a shell.

.. code-block:: bash

   ./ci/alpine.sh shell

.. code-block:: bash

   ./ci/arch.sh shell

Note: Docker must have IPv6 enabled. Since we only need loopback, you can
configure a unique local subnet. For some reason docker doesn't support loopback
only anymore. I consider it a bug, the corresponding issue told me it isn't.

.. code-block:: bash

   DOCKER_OPTS="--ipv6 --fixed-cidr-v6 fc00:beef:beef::/40"

If IPv6 is working in your docker, you don't have to change anything. We only
need to loopback. The above is just how I solved the problem.

Windows
-------

No development build available.

Check vs test
-------------

make check
    Not instrumented (release mode), goal: checking compatibility

make test
    Instrumented (dev mode), goal: helping developers to find bugs

Clang-format
------------

We enforce a specific format using clang-format. To format all the code do in
the build folder:

.. code-block:: bash

   make format

You can use the clang-format vim plugin:

.. code-block:: vim

   Plugin 'rhysd/vim-clang-format'
   au FileType c ClangFormatAutoEnable

If you have a different version of clang-format than our CI, the result of
clang-format might differ. To format using the ci do:

.. code-block:: bash

   ci/alpine.sh shell
   make format


Syntastic
---------

By default vim will treat \*.h files as cpp, but syntastic has no make-checker
for cpp, so \*.h would not get checked.

.. code-block:: bash

   let g:syntastic_c_checkers = ['make']
   au BufNewFile,BufRead *.h set ft=c

With this setting syntastic will check the following:

* Clang-based build errors
* Line length
* Trailing whitespaces

Clang complete
--------------

If you use clang complete, we recommend

.. code-block:: vim

   let g:clang_auto_select     = 1
   let g:clang_snippets        = 1
   let g:clang_snippets_engine = 'clang_complete'

Changes
=======

2018-01-03 - 0.2.0-beta
-----------------------

* Initial public beta release

* All functional features implemented

* Some performance/build features missing


2018-02-17 - 0.2.1-beta
-----------------------

* Remove unnecessary copy of reconnect_remotes datastructure

* Simplify iterations in gc_connections

* Send probe message the first time it is needed

* Fix memory leak in buffer pool

* Switch from uv_hrtime to uv_now

2018-03-17 - 1.0.0-beta
-----------------------

* Rename config.ACKNOWLEDGE to config.SYNCHRONOUS (ABI break)

* Update to rbtree 0.7

* Correctly handle ENOMEM when connecting

* Always ch_cn_shutdown on conn init errors (used to just ch_free sometimes)

* Fix AF_INET assert

* Fix late send_ts_queue_lock destroy

* Do not log missing recv callback

* Add ch_chirp_t and ch_release_cb_t to ch_chirp_release_msg_slot (ABI break)

  * Allows to continue when message has been released

* Add ch_chirp_release_msg_slot_ts

  * Allows to release message in a different thread

* Recover from partial ch_chirp_init() properly

* Renaming buffer-handlers to message-slots (ABI break)

* Add _ssl_context to message for future use (ABI break)

* Move srand() to ch_libchirp_init

* ch_msg_free_data now also frees the upper-layer protocol header

* Missing lock in ch_at_allocated

2018-??-?? - 1.1.0-beta
-----------------------

Not yet released

* Implement scatter-gather write API

  * Improves peak performance to 240'000 msg/s

Thanks
======

For letting me do this:

* `Adfinis SyGroup`_

.. _`Adfinis SyGroup`: https://www.adfinis-sygroup.ch/

For helping me with the architecture:

* David Vogt @winged
* Sven Osterwalder @sosterwalder

For helping me with the documentation:

* Sven Osterwalder @sosterwalder
* David Vogt @winged

For reviewing my pull requests:

* Oliver Sauder @sliverc
* David Vogt @winged
* Tobias Rueetschi @keachi

License
=======

libchirp is subject to the terms of the Mozilla Public License, v. 2.0. Creating
a "Larger Work" under the GNU (Lesser) General Public License is explicitly
allowed. Contributors to libchirp must agree to the Mozilla Public License, v.
2.0.

Contributing
============

Please open an issue first. Contributions of missing features are very welcome, but
we want to keep to scope of libchirp minimal, so additional features should
probably be implemented in an upper layer.

Most valuable contributions:

* If you run continuous integration on your app, build chirp with
  CH_ENABLE_ASSERTS and report bugs.

* Contribute any kind of tests or fuzzing (if possible hypothesis_ based)

* Make bindings for your favorite language

* Make packages for your favorite distribution

* Promote libchirp

.. _hypothesis: https://hypothesis.readthedocs.io/en/latest/
