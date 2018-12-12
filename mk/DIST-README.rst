
Project home and documentation
==============================

`Project home`_

.. _`Project home`: https://github.com/concretecloud/chirp

`Read the docs`_

.. _`Read the docs`: https://docs.adfinis-sygroup.ch/public/chirp-1.2.1/

How to build and install
========================

By default it will be installed in /usr/local.

Example: Install in /usr

.. code-block:: bash

   make STRIP=True
   sudo make install PREFIX=/usr

Example: Install in /usr without TLS

.. code-block:: bash

   make STRIP=True WITHOUT_TLS=True
   sudo make install PREFIX=/usr

Example: Packaging (no strip since distributions usually want to control strip)

.. code-block:: bash

   make
   make install PREFIX=/usr DEST=./pkgdir

Example: Debug

.. code-block:: bash

   CFLAGS=-O0 make
   sudo make install PREFIX=/usr/local

Most debug code is activated with CH_ENABLE_ASSERTS. In fact if you are running
continuous integration on your application, please build libchirp with assert
and report bugs.

.. code-block:: bash

   CFLAGS="-O0 -DCH_ENABLE_ASSERTS" make
   sudo make install PREFIX=/usr/local
