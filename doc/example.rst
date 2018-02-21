Example echo-server
===================

.. code-block:: cpp

   #include <libchirp.h>

   static
   void
   recv_message_cb(ch_chirp_t* chirp, ch_message_t* msg);

   static
   void
   sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status);

   int
   main(int argc, char *argv[])
   {

Next we have to initialize libchirp using :c:func:`ch_libchirp_init`. This will
initialize the TLS library (LibreSSL or OpenSSL) and a mutex used during
initialization of chirp-obejcts.

.. code-block:: cpp

       ch_libchirp_init();

:c:func:`ch_chirp_config_init` will initialize the config to its default values.

.. code-block:: cpp

       ch_config_t config;
       ch_chirp_config_init(&config);

Next we have to set a certification chain. You can create it using the makepki
Makefile_ on github. If you want to create it manually the chain has to contain:

* The certification authority's public key

* The client public key (signed by CA)

* The client private key

Any client-key signed by the CA will be able to connect.

.. _Makefile: https://github.com/concretecloud/chirp/tree/master/mk/makepki
       
.. code-block:: cpp

       config.CERT_CHAIN_PEM = "./cert.pem";
       config.DH_PARAMS_PEM  = "./dh.pem";

:c:func:`ch_chirp_run` initializes libuv, chirp, and runs main-loop.

.. code-block:: cpp

       ch_chirp_t chirp;
       ch_chirp_run(
           &config,
           &chirp,
           recv_message_cb,
           NULL,
           NULL,
           NULL
       );

If the user hits ctrl-c, the main-loop will close and we cleanup libchirp using
:c:func:`ch_libchirp_cleanup`.

.. code-block:: cpp

       ch_libchirp_cleanup();
   }

:c:func:`ch_chirp_send` will send the received message and then call send_cb.

.. code-block:: cpp

   static
   void
   recv_message_cb(ch_chirp_t* chirp, ch_message_t* msg)
   {
       ch_chirp_send(chirp, msg, sent_cb);
   }

The memory reserved for the message has to be retained till the message is sent
or has failed, so we release the slot after message is successfully sent.
:c:func:`ch_chirp_release_msg_slot`

.. code-block:: cpp

   static
   void
   sent_cb(ch_chirp_t* chirp, ch_message_t* msg, int status)
   {
       ch_chirp_release_msg_slot(msg);
   }

.. vim: set spell spelllang=en foldmethod=marker sw=2 ts=2 et tw=76: .. }}}
