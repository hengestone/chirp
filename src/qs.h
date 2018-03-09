// =================
// Queue / Stack 0.7
// =================
//
// Queue and stack with a rbtree-style interface. Both queue and stack use one
// next pointer and can be described as linked lists, except that the queue is
// actually an ring.
//
// Installation
// ============
//
// Copy qs.h into your source.
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
// Queue
// -----
//
// qs_queue_bind_decl_m(context, type) alias rb_bind_decl_cx_m
//    Bind the qs_queue function declarations for *type* to *context*. Usually
//    used in a header.
//
// qs_queue_bind_impl_m(context, type)
//    Bind the qs_queue function implementations for *type* to *context*.
//    Usually used in a c-file. This variant uses the standard rb_*_m traits.
//
// qs_queue_bind_impl_cx_m(context, type)
//    Bind the qs_queue function implementations for *type* to *context*.
//    Usually used in a c-file. This variant uses cx##_*_m traits, which means
//    you have to define them.
//
// qs_queue_bind_cx_m/qs_queue_bind_m(context, type)
//    Shortcut to bind declaration and implementation at once.
//
// Then the following functions will be available.
//
// cx##_enqueue(type** queue, type* item)
//    Enqueue an item to the queue.
//
// cx##_dequeue(type** queue, type** item)
//    Dequeue an item from the queue. Queue will be set to NULL when empty.
//
// cx##_head(type* queue, type* item)
//    *item* will be set to the item in front of the queue, which equals the
//    oldest item or the item that is going to be dequeud. Is NULL when the
//    queue is empty.
//
// cx##_head(type* queue, type* item)
//    *item* will be set to the item in the back of the queue, which equals the
//    newest item. Is NULL when the queue is empty.
//
// cx##_iter_init(type* tree, cx##_iter_t* iter, type** elem)
//    Initializes *elem* to point to the first element in the queue. Use
//    qs_queue_iter_decl_m to declare *iter* and *elem*. If the queue is empty
//    *elem* will be a NULL-pointer.
//
// cx##_iter_next(cx##_iter_t* iter, type** elem)
//    Move *elem* to the next element in the queue. *elem* will point to
//    NULL at the end.
//
// cx##_top(type* stack, type* item)
//    *item* will be set to the item on top of the stack.
//
// You can use rb_for_m from rbtree.h with qs.
//
// Stack
// -----
//
// qs_stack_bind_decl_m(context, type) alias rb_bind_decl_cx_m
//    Bind the qs_stack function declarations for *type* to *context*. Usually
//    used in a header.
//
// qs_stack_bind_impl_m(context, type)
//    Bind the qs_stack function implementations for *type* to *context*.
//    Usually used in a c-file. This variant uses the standard rb_*_m traits.
//
// qs_stack_bind_impl_cx_m(context, type)
//    Bind the qs_stack function implementations for *type* to *context*.
//    Usually used in a c-file. This variant uses cx##_*_m traits, which means
//    you have to define them.
//
// qs_stack_bind_cx_m/qs_stack_bind_m(context, type)
//    Shortcut to bind declaration and implementation at once.
//
// Then the following functions will be available.
//
// cx##_push(type** stack, type* item)
//    Push an item to the stack.
//
// cx##_pop(type** stack, type** item)
//    Pop an item from the stack. Stack will be set to NULL when empty.
//
// cx##_iter_init(type* tree, cx##_iter_t* iter, type** elem)
//    Initializes *elem* to point to the first element in the stack. Use
//    qs_stack_iter_decl_m to declare *iter* and *elem*. If the stack is empty
//    *elem* will be a NULL-pointer.
//
// cx##_iter_next(cx##_iter_t* iter, type** elem)
//    Move *elem* to the next element in the stack. *elem* will point to
//    NULL at the end.
//
// You can use rb_for_m from rbtree.h with qs.
//
// Implementation
// ==============
//
// Includes
// --------
//
// .. code-block:: cpp
//
#ifndef qs_stack_queue_h
#define qs_stack_queue_h
#include <assert.h>

// Traits
// ------
//
// .. code-block:: cpp
//
#define qs_next_m(x) (x)->next

// Queue
// -----
//
// Common arguments
//
// next
//    Get the next item of the queue
//
//
// .. code-block:: text
//
//    .---.        .---.
//    | 2 |<-next--| 1 |
//    '---'        '---'
//      |next        ^
//      v        next|
//    .---.        .---.
//    | 3 |--next->| 4 |<--queue--
//    '---'        '---'
//
// qs_enqueue_m
// ------------
//
// Bound: cx##_enqueue
//
// Enqueues an item to the queue.
//
// queue
//    Beginning of the queue
//
// item
//    Item to enqueue.
//
// .. code-block:: cpp
//
#define qs_enqueue_m( \
        next, \
        queue, \
        item \
) \
{ \
    assert(next(item) == NULL && "Item already in use"); \
    if(queue == NULL) { \
        next(item) = item; \
    } else { \
        next(item) = next(queue); \
        next(queue) = item; \
    } \
    queue = item; \
} \


// qs_dequeue_m
// ------------
//
// Bound: cx##_dequeue
//
// Dequeue an item from the queue. Returns the first item in the queue (FIFO).
// Does nothing if the queue is empty.
//
// queue
//    Beginning of the queue
//
// item
//    Item dequeued.
//
// .. code-block:: cpp
//
#define qs_dequeue_m( \
        next, \
        queue, \
        item \
) \
{ \
    if(queue != NULL) { \
        item = next(queue); \
        if(next(queue) == queue) { \
            queue = NULL; \
        } else { \
            next(queue) = next(item); \
        } \
        next(item) = NULL; \
    } else { \
        item = NULL; \
    } \
} \


// qs_queue_bind_decl_m
// --------------------
//
// Alias: qs_queue_bind_decl_cx_m
//
// Bind queue functions to a context. This only generates declarations.
//
// cx
//    Name of the new context.
//
// type
//    The type of the items of the queue.
//
// .. code-block:: cpp
//
#define qs_queue_bind_decl_m(cx, type) \
    typedef type cx##_iter_t; \
    typedef type cx##_type_t; \
    void \
    cx##_enqueue( \
            type** queue, \
            type* item \
    ); \
    void \
    cx##_dequeue( \
            type** queue, \
            type** item \
    ); \
    void \
    cx##_iter_init( \
            type* queue, \
            cx##_iter_t** iter, \
            type** elem \
    ); \
    void \
    cx##_iter_next( \
            cx##_iter_t* iter, \
            type** elem \
    ); \
    void \
    cx##_head( \
            type* queue, \
            type** item \
    ); \
    void \
    cx##_tail( \
            type* queue, \
            type** item \
    ); \


#define qs_queue_bind_decl_cx_m(cx, type) \
    qs_queue_bind_decl_m(cx, type) \


// qs_queue_bind_impl_m
// ---------------------
//
// Bind queue functions to a context. This only generates implementations.
//
// qs_queue_bind_impl_m uses qs_next_m. qs_queue_bind_impl_cx_m uses
// cx##_next_m.
//
// cx
//    Name of the new context.
//
// type
//    The type of the items of the queue.
//
// .. code-block:: cpp
//
#define _qs_queue_bind_impl_tr_m(cx, type, next) \
    void \
    cx##_enqueue( \
            type** queue, \
            type* item \
    ) qs_enqueue_m( \
            next, \
            *queue, \
            item \
    ) \
    void \
    cx##_dequeue( \
            type** queue, \
            type** item \
    ) qs_dequeue_m( \
            next, \
            *queue, \
            *item \
    ) \
    void \
    cx##_iter_init( \
            type* queue, \
            cx##_iter_t** iter, \
            type** elem \
    ) \
    { \
        qs_queue_iter_init_m( \
            next, \
            queue, \
            *iter, \
            *elem \
        ); \
    } \
    void \
    cx##_iter_next( \
            cx##_iter_t* iter, \
            type** elem \
    ) \
    { \
        qs_queue_iter_next_m( \
            next, \
            iter, \
            *elem \
        ) \
    } \
    void \
    cx##_head( \
            type* queue, \
            type** item \
    ) { \
        if(queue != NULL) { \
            *item = next(queue); \
        } else { \
            *item = NULL; \
        } \
    } \
    void \
    cx##_tail( \
            type* queue, \
            type** item \
    ) { \
        *item = queue; \
    } \


#define qs_queue_bind_impl_cx_m(cx, type) \
    _qs_queue_bind_impl_tr_m(cx, type, cx##_next_m) \


#define qs_queue_bind_impl_m(cx, type) \
    _qs_queue_bind_impl_tr_m(cx, type, qs_next_m) \


#define qs_queue_bind_cx_m(cx, type) \
    qs_queue_bind_decl_cx_m(cx, type) \
    qs_queue_bind_impl_cx_m(cx, type) \


#define qs_queue_bind_m(cx, type) \
    qs_queue_bind_decl_m(cx, type) \
    qs_queue_bind_impl_m(cx, type) \


// qs_queue_iter_decl_m
// ---------------------
//
// Also: qs_queue_iter_decl_cx_m
//
// Declare iterator variables.
//
// iter
//    The new iterator variable.
//
// elem
//    The pointer to the current element.
//
// .. code-block:: cpp
//
#define qs_queue_iter_decl_m(type, iter, elem) \
    type* iter = NULL; \
    type* elem = NULL; \


#define qs_queue_iter_decl_cx_m(cx, iter, elem) \
    cx##_type_t* iter = NULL; \
    cx##_type_t* elem = NULL; \


// qs_queue_iter_init_m
// ---------------------
//
// Bound: cx##_iter_init
//
// Initialize iterator. It will point to the first element or NULL if the queue
// is empty.
//
// queue
//    The queue.
//
// iter
//    The iterator.
//
// elem
//    The pointer to the current element.
//
//
// .. code-block:: cpp
//
#define qs_queue_iter_init_m(next, queue, iter, elem) \
{ \
    iter = queue; \
    if(queue == NULL) { \
        elem = NULL; \
    } else { \
        elem = next(queue); \
    } \
} \


// qs_queue_iter_next_m
// --------------------
//
// Bound: cx##_iter_next
//
// Initialize iterator. It will point to the first element. The element will be
// NULL, if the iteration is at the end.
//
// queue
//    The queue.
//
// elem
//    The pointer to the current element.
//
// .. code-block:: cpp
//
#define qs_queue_iter_next_m( \
        next, \
        queue, \
        elem \
) \
{ \
    if(elem == queue) { \
        elem = NULL; \
    } else { \
        elem = next(elem); \
    } \
} \



// Stack
// -----
//
// Common arguments
//
// next
//    Get the next item of the stack
//
// qs_push_m
// ---------
//
// Bound: cx##_push
//
// Push an item to the stack.
//
// stack
//    Base pointer to the stack.
//
// item
//    Item to push.
//
// .. code-block:: cpp
//
#define qs_push_m( \
        next, \
        stack, \
        item \
) \
{ \
    assert(next(item) == NULL && "Item already in use"); \
    next(item) = stack; \
    stack = item; \
} \


// qs_pop_m
// --------
//
// Bound: cx##_pop
//
// Pop an item from the stack. Returns the last item in the stack (LIFO).
// Does nothing if the stack is empty.
//
// stack
//    Base pointer to the stack.
//
// item
//    Item popped.
//
// .. code-block:: cpp
//
#define qs_pop_m( \
        next, \
        stack, \
        item \
) \
{ \
    item = stack; \
    if(stack != NULL) { \
        stack = next(stack); \
        next(item) = NULL; \
    } \
} \


// qs_stack_bind_decl_m
// --------------------
//
// Alias: qs_stack_bind_decl_cx_m
//
// Bind stack functions to a context. This only generates declarations.
//
// cx
//    Name of the new context.
//
// type
//    The type of the items of the stack.
//
// .. code-block:: cpp
//
#define qs_stack_bind_decl_m(cx, type) \
    typedef type cx##_iter_t; \
    typedef type cx##_type_t; \
    void \
    cx##_push( \
            type** stack, \
            type* item \
    ); \
    void \
    cx##_pop( \
            type** stack, \
            type** item \
    ); \
    void \
    cx##_iter_init( \
            type* stack, \
            cx##_iter_t** iter, \
            type** elem \
    ); \
    void \
    cx##_iter_next( \
            cx##_iter_t* iter, \
            type** elem \
    ); \
    void \
    cx##_top( \
            type* stack, \
            type** item \
    ); \


#define qs_stack_bind_decl_cx_m(cx, type) qs_stack_bind_decl_m(cx, type)

// qs_stack_bind_impl_m
// ---------------------
//
// Bind stack functions to a context. This only generates implementations.
//
// qs_stack_bind_impl_m uses qs_next_m. qs_stack_bind_impl_cx_m uses
// cx##_next_m.
//
// cx
//    Name of the new context.
//
// type
//    The type of the items of the stack.
//
// .. code-block:: cpp
//
#define _qs_stack_bind_impl_tr_m(cx, type, next) \
    void \
    cx##_push( \
            type** stack, \
            type* item \
    ) qs_push_m( \
            next, \
            *stack, \
            item \
    ) \
    void \
    cx##_pop( \
            type** stack, \
            type** item \
    ) qs_pop_m( \
            next, \
            *stack, \
            *item \
    ) \
    void \
    cx##_iter_init( \
            type* stack, \
            cx##_iter_t** iter, \
            type** elem \
    ) \
    { \
        (void)(iter); \
        qs_stack_iter_init_m( \
            next, \
            stack, \
            *elem \
        ); \
    } \
    void \
    cx##_iter_next( \
            cx##_iter_t* iter, \
            type** elem \
    ) \
    { \
        (void)(iter); \
        qs_stack_iter_next_m( \
            next, \
            *elem \
        ) \
    } \
    void \
    cx##_top( \
            type* stack, \
            type** item \
    ) { \
        *item = stack; \
    } \


#define qs_stack_bind_impl_cx_m(cx, type) \
    _qs_stack_bind_impl_tr_m(cx, type, cx##_next_m) \


#define qs_stack_bind_impl_m(cx, type) \
    _qs_stack_bind_impl_tr_m(cx, type, qs_next_m) \


#define qs_stack_bind_cx_m(cx, type) \
    qs_stack_bind_decl_cx_m(cx, type) \
    qs_stack_bind_impl_cx_m(cx, type) \


#define qs_stack_bind_m(cx, type) \
    qs_stack_bind_decl_m(cx, type) \
    qs_stack_bind_impl_m(cx, type) \


// qs_stack_iter_decl_m
// ---------------------
//
// Also: qs_stack_iter_decl_cx_m
//
// Declare iterator variables.
//
// iter
//    The new iterator variable.
//
// elem
//    The pointer to the current element.
//
// .. code-block:: cpp
//
#define qs_stack_iter_decl_m(type, iter, elem) \
    type* iter = NULL; \
    type* elem = NULL; \


#define qs_stack_iter_decl_cx_m(cx, iter, elem) \
    cx##_type_t* iter = NULL; \
    cx##_type_t* elem = NULL; \


// qs_stack_iter_init_m
// ---------------------
//
// Bound: cx##_iter_init
//
// Initialize iterator. It will point to the first element or NULL if the stack
// is empty.
//
// stack
//    Base pointer to the stack.
//
// elem
//    The pointer to the current element.
//
//
// .. code-block:: cpp
//
#define qs_stack_iter_init_m(next, stack, elem) \
{ \
    elem = stack; \
} \


// qs_stack_iter_next_m
// --------------------
//
// Bound: cx##_iter_next
//
// Initialize iterator. It will point to the first element. The element will be
// NULL, if the iteration is at the end.
//
// elem
//    The pointer to the current element.
//
// .. code-block:: cpp
//
#define qs_stack_iter_next_m( \
        next, \
        elem \
) \
{ \
    elem = next(elem); \
} \

#endif //qs_stack_queue_h

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
