// =========
// mpipe 0.6
// =========
//
// Send message-packs over a pipe. Meant as a very simple RPC. It is compatible
// with async coding and therefore used in rbtree and chirp to run hypothesis
// tests on C code. (Segfaults will be detected instead of killing the pytest
// run)
//
// Installation
// ============
//
// Copy mpipe.h and mpipe.c into your source. And add mpack_ to your project.
// Use src/mpipe.py to send messages.
//
// mpipe.py
// ========
//
// .. code-block:: python
//
//    proc = mpipe.open(["./build/test_mpipe"])
//    mpipe.write(proc, (1, 0))
//    res = mpipe.read(proc)
//    print(res)
//    mpipe.write(proc, (0,))
//    mpipe.close(proc)
//
// If you set the environment variable MPP_GDB to True. mpipe.open will attach
// a gdb to the process. You need
//
// .. code-block:: bash
//
//    sudo sh -c "echo 0 > /proc/sys/kernel/yama/ptrace_scope"
//
// for that.
//
// If you set the environment variable MPP_RR to True. mpipe.open will record
// an rr session. You need
//
// .. code-block:: bash
//
//    sudo sh -c "echo 1 > /proc/sys/kernel/perf_event_paranoid"
//
// for that.
//
// If you set the environment variable MPP_MC to True. mpipe.open will run
// valgrind --tool=memcheck --leak-check=full --error-exitcode=1
//
// Development
// ===========
//
// See `README.rst`_
//
// .. _`README.rst`: https://github.com/concretecloud/rbtree
//
// API
// ===
//
// void mpp_init_context(mpp_read_t* context)
//   Initialize the mpipe context.
//
// mpack_node_t* mpp_read_message(mpp_read_t* context)
//   Read a message and return the root node of a message. *tree* has to be
//   released using mpp_*read_message_fin*. If your program accesses data of
//   multiple messages you can use `context garbage-collection`_. Returns NULL
//   on error.
//
//   Also: mpp_fdread_message(int fd, mpp_context_t* context) use specified fd
//   instead of stdin.
//
// int mpp_read_message_fin(mpp_context_t* context)
//   Release all resources associated with the message. Returns 0 on success
//   (see mpack_error_t).
//
// mpack_writer_t* mpp_write_message(mpp_context_t* context)
//   Start writing a message. Use *mpack_writer_t* to created the message. See
//   mpack_. After the message is finished call *mpp_write_message_fin*.
//   Returns null on error.
//
//   Also: mpp_fdwrite_message(int fd, mpp_context_t* context) use specified fd
//   instead of stdout.
//
// int mpp_write_message_fin(mpp_context_t* context)
//   Release all resources associated with the message. Returns 0 on success
//   (see mpack_error_t)
//
// int mpp_runner(mpp_function_handler func);
//   Run a mpipe loop.
//
// .. _mpack: https://github.com/ludocode/mpack
//
// Context garbage-collection
// ==========================
//
// .. _`context garbage-collection`:
//
// It can be quite painful to release memory immediately after it is used last.
// For example if you convert a document, you have temporary strings that are
// generated by a sub-function and is used by other sub-functions. Who owns
// that memory, where should it be freed? Simple, you know that after
// converting the document, all the temporary strings can be freed, so you add
// them to a stack_ and after the document is converted you free them all.
//
// .. _stack: https://github.com/concretecloud/rbtree/blob/master/qs.rst
//
// Declarations
// ============
//
// Includes
// --------
//
// .. code-block:: cpp
//
#ifndef mpp_mpipe_h
#define mpp_mpipe_h
#include <unistd.h>
#include <string.h>

#include "mpack_test.h"

// Structs
// -------
//
// .. code-block:: cpp
//
//
enum mpp_action {
    mpp_none = 0,
    mpp_write = 1,
    mpp_read = 2
};

struct mpp_read_ctx_s;
typedef struct mpp_read_ctx_s mpp_read_ctx_t;
struct mpp_read_ctx_s {
    mpack_tree_t tree;
    mpack_node_t node;
    char* data;
};

struct mpp_write_ctx_s;
typedef struct mpp_write_ctx_s mpp_write_ctx_t;
struct mpp_write_ctx_s {
    int fd;
    mpack_writer_t writer;
    char* data;
    size_t size;
};

struct mpp_context_s;
typedef struct mpp_context_s mpp_context_t;
struct mpp_context_s {
    char current;
    char last;
    char rpc_mode;
    mpp_write_ctx_t write;
    mpp_read_ctx_t read;
};

// Callbacks
// ---------
//
// .. code-block:: cpp
//

typedef void (*mpp_handler_cb_t)(mpack_node_t data, mpack_writer_t* writer);

// Functions
// ---------
//
// .. code-block:: cpp
//

mpack_node_t*
mpp_fdread_message(int fd, mpp_context_t* context);
int
mpp_read_message_fin(mpp_context_t* context);

mpack_writer_t*
mpp_fdwrite_message(int fd, mpp_context_t* context);
int
mpp_write_message_fin(mpp_context_t* context);

int
mpp_runner(mpp_handler_cb_t func);

void
mpp_init_context(mpp_context_t* context);

// STDIO Functions
// ---------------
//
// .. code-block:: cpp
//

mpack_node_t*
mpp_read_message(mpp_context_t* context);

mpack_writer_t*
mpp_write_message(mpp_context_t* context);

#endif //mpp_mpipe_h

// MIT License
// ===========
//
// Copyright (c) 2017 Jean-Louis Fuchs
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
