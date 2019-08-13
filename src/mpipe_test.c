// SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mpipe_test.h"

#include <assert.h>

int
writeall(int fd, void *buf, size_t n)
{
    char *buffer = buf;
    while(n) {
        ssize_t r = write(fd, buffer, n);
        if(r < 0) {
            return -1;
        }
        buffer += (size_t)r;
        n -= (size_t)r;
    }
    return n;
}

ssize_t
readall(int fd, void *buf, size_t n)
{
    char *buffer = buf;
    size_t ptr = 0;
    for(;;) {
        ssize_t r = read(fd, buffer + ptr, n - ptr);
        if(r < 0) {
            return -1;
        }
        if(r == 0) {
            break;
        }
        ptr += (size_t)r;
    }
    return ptr;
}

void
mpp_init_context(mpp_context_t* context)
{
    memset(context, 0, sizeof(mpp_context_t));
    context->last = mpp_none;
    context->rpc_mode = 1;
}

mpack_node_t*
mpp_read_message(mpp_context_t* context)
{
    return mpp_fdread_message(STDIN_FILENO, context);
}

mpack_writer_t*
mpp_write_message(mpp_context_t* context)
{
    return mpp_fdwrite_message(STDOUT_FILENO, context);
}

mpack_node_t*
mpp_fdread_message(int fd, mpp_context_t* context)
{
    if(context->rpc_mode && context->last == mpp_read) {
        assert(0 && "Consecutive read not allowed in rpc_mode");
        return NULL;
    }
    context->last = mpp_read;
    mpp_read_ctx_t* read_ctx = &context->read;
    ssize_t read_size;
    size_t msg_size;
    read_size = readall(fd, &msg_size, sizeof(size_t));
    if(read_size != sizeof(size_t)) {
        return NULL;
    }
    read_ctx->data = malloc(msg_size);
    read_size = readall(fd, read_ctx->data, msg_size);
    if(read_size != (ssize_t) msg_size) {
        return NULL;
    }
    mpack_tree_init(&read_ctx->tree, read_ctx->data, msg_size);
    read_ctx->node = mpack_tree_root(&read_ctx->tree);
    return &read_ctx->node;
}

int
mpp_read_message_fin(mpp_context_t* context)
{
    mpp_read_ctx_t* write_ctx = &context->read;
    mpack_error_t err = mpack_tree_destroy(&write_ctx->tree);
    free(write_ctx->data);
    return err;
}

mpack_writer_t*
mpp_fdwrite_message(int fd, mpp_context_t* context)
{
    if(context->rpc_mode && context->last == mpp_write) {
        assert(0 && "Consecutive write not allowed in rpc_mode");
        return NULL;
    }
    context->last = mpp_write;
    mpp_write_ctx_t* write_ctx = &context->write;
    write_ctx->fd = fd;
    mpack_writer_init_growable(
        &write_ctx->writer,
        &write_ctx->data,
        &write_ctx->size
    );
    return &write_ctx->writer;
}

int
mpp_write_message_fin(mpp_context_t* context)
{
    mpp_write_ctx_t* write_ctx = &context->write;
    int ret;
    mpack_error_t err = mpack_writer_destroy(&write_ctx->writer);
    if(err != mpack_ok) {
        return err;
    }
    ret = writeall(
        write_ctx->fd,
        &write_ctx->size,
        sizeof(size_t)
    );
    if(ret != 0) {
        return -1;
    }
    ret = writeall(
        write_ctx->fd,
        write_ctx->data,
        write_ctx->size
    );
    if(ret != 0) {
        return -1;
    }
    free(write_ctx->data);
    context->current = mpp_none;
    return 0;
}

int
mpp_runner(mpp_handler_cb_t func)
{
    mpp_context_t context;
    mpack_writer_t* writer;
    mpp_init_context(&context);
    for(;;) {
        mpack_node_t* node = mpp_read_message(&context);
        if(node == NULL) {
            return 9;
        }
        int function = mpack_node_int(
            mpack_node_array_at(*node, 0)
        );
        if(function == 0) {
            if(mpp_read_message_fin(&context) != 0) {
                return 9;
            }
            return 0;
        } else {
            writer = mpp_write_message(&context);
            func(*node, writer);
            if(mpp_write_message_fin(&context) != 0)
                return 9;
        }
        if(mpp_read_message_fin(&context) != 0) {
            return 9;
        }
    }
    return 0;
}
