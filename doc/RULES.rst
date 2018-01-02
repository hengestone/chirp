=====
RULES
=====

* Mantra

  - Make it work
  - Make it well tested
  - Make it used by people
  - Make it fast (only if really needed)
  - Make it beautiful

* Do not use static functions in headers.
* Log and assert a lot
* Use C style comments. // is reserved for restructured text.
* The following are our given abstractions:

  - libuv
  - rbtree
  - openssl
  - We try not do add our own abstractions, to keep complexity low

* We use defined length integers ie. uint8_t for file-formates, wire-protocols
  and when plain int is really really wasteful
* Buffers and chars:

  - char* for C-strings
  - void* for buffers (in interfaces)
  - ch_buf* for buffer-instances (an alias for char*)
  - uint8_t* for bytes (for example the identity)

* Use one flags member instead of many bools (int)
* Structs end in _s
* All the structs have a typedef ending in _t
* Types end in _t
* Callbacks end in _cb

  - Use natural names and _cb will indicated that it probably happened AFTER

* Callback types end in _cb_t
* Sort symbols alphabetically including underscores "_"
  - When independent

* Forward declarations go to common.h. Example

.. code-block:: cpp

   struct ch_chirp_s;
   typedef struct ch_chirp_s ch_chirp_t;

* Basic layout, follow where feasible

.. code-block:: text

   internal includes

   external includes

   defines

   other symbols (sorted, when independent)

   function symbols (sorted)

* Always unpack handles in callbacks and user called functions till you can
  verify the chirp magic
* Embeddable: allocate memory via user callback
* Embrace libuv styles and use it for chirp API
* Literate programming (kinda)
* Localhost connections bypass TLS
* Use C99
* Use the clang-format defined in chirp/.clang-format
* Tests don't have to be documented, so people can write tests fast and in flow.
* Sending small messages may not allocate memory

  - Only things that happen seldom may allocate
  - Luckily chirp is already designed that way

* Provide distro packages

=========
Questions
=========

Things that aren't rules yet
