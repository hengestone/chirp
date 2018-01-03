=============================
Building and installing Chirp
=============================

This relates to the source distribution of libchirp. For building the git
repository see README.rst_

.. _README.rst: https://github.com/concretecloud/chirp/blob/master/README.rst

Project home and documentation
==============================

`Project home`_

.. _`Project home`: https://github.com/concretecloud/chirp

`Read the docs`_

.. _`Read the docs`: http://1042.ch/chirp/

How to build and install
========================

By default it will be installed in /usr/local.

Example: Install in /usr

.. code-block:: bash

   make STRIP=True
   sudo make install PREFIX=/usr

Example: Packaging (no strip since distributions usually want to control strip)

.. code-block:: bash

   make
   make install PREFIX=/usr DEST=./pkgdir

Example: Debug

.. code-block:: bash

   CFLAGS=-O0 make
   make install PREFIX=/usr/local

#ifndef NDEBUG is stripped in source distribution, so still no debug code.

Windows
-------

We want to support Windows, but we are currently not building on Windows. VS
2015 or newer should support all C99 feature we use.
