// .. image:: https://travis-ci.org/concretecloud/rbtree.svg?branch=master
//    :target: https://travis-ci.org/concretecloud/rbtree/
//
// ==================
// Red-Black Tree 0.7
// ==================
//
// * Bonus: `qs.h`_ (Queue / Stack), mpipe_ (message-pack over pipe)
// * Textbook implementation
// * Extensive tests
// * Has parent pointers and therefore faster delete_node and constant time
//   replace_node
// * Composable
// * Readable
// * Generic
// * C99
// * Easy to use, a bit complex to extend because it is generic [1]_
// * MIT license
// * By Jean-Louis Fuchs <ganwell@fangorn.ch>
// * Based on Introduction To Algorithms
// * Text review by Eva Fuchs
// * Code review by Oliver Sauder @sliverc
// * Thanks a lot to both
//
// .. [1] My rgc preprocessor and make MACRO_DEBUG=True are very helpful.
//
// .. _`qs.h`: https://github.com/concretecloud/rbtree/blob/master/qs.rst
// .. _mpipe: https://github.com/concretecloud/rbtree/blob/master/mpipe.rst
//
// Installation
// ============
//
// Copy rbtree.h into your source.
//
// Changes
// =======
//
// 0.3 -> 0.4
// ----------
//
// * Correctly enable cppcheck
// * Fix style errors reported by cppcheck
// * Fix bad asserts in performance tests
//
// 0.4 -> 0.5
// ----------
//
// * Remove static from functions (its a bad habit)
// * Typos
//
// 0.5 -> 0.6
// ----------
//
// * Use same code-guidelines as libchirp
// * Allow msgpack or umsgpack in mpipe.py
//
// 0.6 -> 0.7
// ----------
//
// * Remove next from _qs_queue_bind_decl_tr_m and renaming it to
//   qs_queue_bind_decl_m
// * Document bind shortcuts for rb and qs
//
// Development
// ===========
//
// Developed on github_
//
// .. _github: https://github.com/concretecloud/rbtree
//
// Requirements
// ------------
//
// * gcc or clang
// * pytest
// * hypothesis
// * u-msgpack-python
// * cffi
// * rst2html
//
// Build and test
// --------------
//
// .. code-block:: bash
//
//    make
//
// Debug macros
// ------------
//
// .. code-block:: bash
//
//    make clean all MACRO_DEBUG=True
//
// This will expand the macros into .c files, so gdb can step into them.
//
// Usage
// =====
//
// Getting started
// ---------------
//
// The example is on github: example_h_, example_c_
//
// .. _example_h: https://github.com/concretecloud/rbtree/blob/master/src/example.h
// .. _example_c: https://github.com/concretecloud/rbtree/blob/master/src/example.c
//
// First we have to define the struct we use in the red-black tree
// (example.h).
//
// .. code-block:: cpp
//
//    struct book_s;
//    typedef struct book_s book_t;
//    struct book_s {
//        char    isbn[14];
//        char*   title;
//        char*   author;
//        char    color;
//        book_t* parent;
//        book_t* left;
//        book_t* right;
//    };
//
// You can add the fields color, parent, left and right to any existing struct.
// It is also possible to use different names than the above (see Extended_).
//
// Next we have to define the comparator function, since we want to lookup
// books using the ISBN-number, we compare it using memcmp.
//
// .. code-block:: cpp
//
//    #define bk_cmp_m(x, y) memcmp(x->isbn, y->isbn, 13)
//
// Since rbtree is implemented as macros, using uppercase for macros isn't a
// good convention, therefore we use the suffix _m to denote macros.
//
// The prefix bk\_ is used as convenience, see context_.
//
// Note if you do something like:
//
// .. code-block:: cpp
//
//    #define bk_cmp_m(x, y) (x->value - y->value)
//
// You may only use values from (MIN_INT / 4) - 1 to (MAX_INT / 4) since rbtree
// uses a int to store the result. To be safe write the comparator as:
//
// .. code-block:: cpp
//
//    #define rb_safe_cmp_m(x, y) (((x)>(y) ? 1 : ((x)<(y) ? -1 : 0)))
//    #define bk_cmp_m(x, y) rb_safe_cmp_m(x->value, y->value)
//
// rb_safe_cmp_m is provided by rbtree.
//
// .. _context:
//
// Then we have to declare all the rbtree functions. rbtree uses a concept, I
// call context, to find functions it needs. For example the rbtree functions
// look for a macro called $CONTEXT_cmp_m. I developed this concept to make
// functions composable without being too verbose.
//
// .. code-block:: cpp
//
//    rb_for_m(bk, tree, bk_iter, bk_elem)
//
// will look for the functions bk_iter_init and bk_iter_next.
//
// rb_bind_decl_m takes the context, bk in this case and the type as arguments.
//
// .. code-block:: cpp
//
//    rb_bind_decl_m(bk, book_t)
//
// Now we switch to example.c and define all the rbtree functions and the trees
// root node.
//
// .. code-block:: cpp
//
//    #include "example.h"
//    rb_bind_impl_m(bk, book_t)
//    book_t* tree;
//
// In order to use the tree, we have to initialize it, which actually is
// assigning *bk_nil_ptr* to it.
//
// .. code-block:: cpp
//
//    bk_tree_init(&tree);
//
// Now we can register a book:
//
// .. code-block:: cpp
//
//    void
//    register_book(char isbn[14], char* title, char* author)
//    {
//        book_t* book = malloc(sizeof(book_t));
//        bk_node_init(book);
//        book->title  = title;
//        book->author = author;
//        memcpy(book->isbn, isbn, 14);
//        bk_insert(&tree, book);
//    }
//
// Note that we pass a double pointer to bk_insert, since it might need to change
// the root node.
//
// Or we can lookup a book:
//
// .. code-block:: cpp
//
//    void
//    lookup_book(char isbn[14])
//    {
//        book_t* book;
//        book_t key;
//        memcpy(key.isbn, isbn, 14);
//        bk_find(tree, &key, &book);
//        printf(
//            "ISBN:   %s\nTitle:  %s\nAuthor: %s\n\n",
//            book->isbn,
//            book->title,
//            book->author
//        );
//    }
//
// The *key* is just another node, we don't have to initialize it, but only set
// the fields used by the comparator. bk_find will set *book* to the node found.
//
// We can also iterate over the tree, the result will be sorted, lesser element
// first. The tree may not be modified during iteration.
//
// .. code-block:: cpp
//
//    rb_iter_decl_cx_m(bk, bk_iter, bk_elem);
//    rb_for_m(bk, tree, bk_iter, bk_elem) {
//        printf("%s\n", bk_elem->isbn);
//    }
//
// Removing a book is straight forward.
//
// .. code-block:: cpp
//
//    void
//    remove_book(book_t* book)
//    {
//        printf("Removing %s\n", book->isbn);
//        bk_delete_node(&tree, book);
//        free(book);
//    }
//
// But we cannot use the iterator. Therefore we just remove the root till the
// tree is empty.
//
// .. code-block:: cpp
//
//    while(tree != bk_nil_ptr) {
//        remove_book(tree);
//    }
//
// API
// ---
//
// rb_bind_decl_m(context, type) alias rb_bind_decl_cx_m
//    Bind the rbtree function declarations for *type* to *context*. Usually
//    used in a header.
//
// rb_bind_impl_m(context, type)
//    Bind the rbtree function implementations for *type* to *context*. Usually
//    used in a c-file. This variant uses the standard rb_*_m traits.
//
// rb_bind_impl_cx_m(context, type)
//    Bind the rbtree function implementations for *type* to *context*. Usually
//    used in a c-file. This variant uses cx##_*_m traits, which means you have
//    to define them.
//
// rb_bind_cx_m/rb_bind_m(context, type)
//    Shortcut to bind declaration and implementation at once.
//
// rb_safe_value_cmp_m(x, y)
//    Basis for safe value comparators. *x* and *y* are comparable values of
//    the same type.
//
// Then the following functions will be available.
//
// cx##_tree_init(type* tree)
//    Initialize *tree* by assigning *cx##_nil_ptr* to it.
//
// cx##_node_init(type* node)
//    Initialize *node* by initializing the color, parent, left and right fields.
//
// cx##_insert(type** tree, type* node)
//    Insert *node* into *tree*. If a node with the same key exists the
//    function returns 1 and *node* is not inserted, 0 on success.
//
// cx##_delete_node(type** tree, type* node)
//    Delete the known *node* from *tree*.
//
// cx##_delete(type** tree, type* key, type** node)
//    Delete the node matching *key* from *tree*. If *key* is not in the tree
//    the function returns 1, 0 on success. On success *node* is set to the
//    deleted node.
//
// cx##_replace_node(type** tree, type* old, type* new)
//    Replace known node *old* with *new*. If *old* and *new* are not equal the
//    function will not do anything and returns 1, 0 on success.
//
// cx##_replace(type** tree, type* key, type* new, type** old)
//    Replace the node matching *key* with *new*. If *key* and *new* are not
//    equal the function will not do anything and returns 1. If *key* is not in
//    the tree the function will not do anything and returns 1. It returns 0 on
//    success. On success *old* is set to the old node.
//
// cx##_find(type* tree, type* key, type** node)
//    Find the node matching *key* and assign it to *node*. If *key* is not in
//    the tree *node* will not be assigned and the function returns 1, 0 on
//    success.
//
// cx##_size(type* tree)
//    Returns the size of tree. By default RB_SIZE_T is int to avoid additional
//    dependencies. Feel free to define RB_SIZE_T as size_t for example. O(log
//    (N)).
//
// rb_iter_decl_m(cx, iter, elem)
//    Declares the variables *iter* and *elem* for the context *cx*.
//
// cx##_iter_init(type* tree, cx##_iter_t* iter, type** elem)
//    Initializes *elem* to point to the first element in tree. Use
//    rb_iter_decl_m to declare *iter* and *elem*. If the tree is empty
//    *elem* will be NULL.
//
// cx##_iter_next(cx##_iter_t* iter, type** elem)
//    Move *elem* to the next element in the tree. *elem* will point to
//    NULL at the end.
//
// cx##_check_tree(type* tree)
//    Check the consistency of a tree. Only interesting for development of
//    rbtree itself. If will fail with an assert if there is an inconsistency.
//
// Extended
// --------
//
// .. _Extended:
//
// Many functions x come in two flavors
//
// cx_x
//    These functions are bound to a type. Traits and the comparator are mapped
//    to the context. You have to define the type and the traits for the
//    context and then you bind the function.
//
//    .. code-block:: cpp
//
//       #define my_color_m(x) (x)->color
//       #define my_parent_m(x) (x)->parent
//       #define my_left_m(x) (x)->left
//       #define my_right_m(x) (x)->right
//       #define my_cmp_m(x, y) rb_safe_value_cmp_m(x, y)
//       rb_bind_cx_m(my, node_t)
//
//    .. code-block:: cpp
//
//       my_tree_init(&tree);
//       my_node_init(node);
//
//    There is also a shortcut if you know your are going to use all standard
//    fields in your struct (color, parent, left right)
//
//    .. code-block:: cpp
//
//       #define my_cmp_m(x, y) rb_safe_value_cmp_m(x, y)
//       rb_bind_m(my, node_t)
//
//    .. code-block:: cpp
//
//       my_tree_init(&tree);
//       my_node_init(node);
//
//    Of course usually, you want to split declaration and implementation of the
//    function, so it is: header.h:
//
//    .. code-block:: cpp
//
//       #define my_cmp_m(x, y) rb_safe_value_cmp_m(x, y)
//       rb_bind_decl_m(my, node_t)
//
//    And object.c:
//
//    .. code-block:: cpp
//
//       #include "header.h"
//       rb_bind_impl_m(my, node_t)
//
//       int main(void) { my_node_init(node); return 0; }
//
// rb_x_m
//    These functions are macros and take a type and traits as standard
//    arguments and are the most verbose. Used to extend rbtree.
//
//    To use the rb_x_m functions you also need to initialize the nil pointer.
//
//    .. code-block:: cpp
//
//       tree = my_nil_ptr;
//       rb_node_init_m(
//           my_nil_ptr,
//           rb_color_m,
//           rb_parent_m,
//           rb_left_m,
//           rb_right_m,
//           my_nil_ptr
//       ); // Instead of my_tree_init in the bound functions
//
// Questions
// =========
//
// Why don't you just generate typed functions from the beginning?
//    I want to be able to reuse and compose my code. Especially for
//    composability I need access to the generic functions.
//
// Why is the iterator so complicated?
//    rbtree may become part of a larger set of data-structures, some need more
//    complicated iterator setups, to make the data-structures interchangeable,
//    all have to follow the iterator protocol. Use rb_for_m.
//
// Why yet another red-black tree?
//    I often joke that C programmers will reimplement every thing till it
//    perfectly fits their use-case/payload. I need the replace_node function
//    in my project. I found no way to avoid creating rbtree. sglib is the only
//    generic red-black tree implementation I know of and it has no parent
//    pointers, which makes replace_node impossible.
//
// Performance
// ===========
//
// I compare with sglib_, because it is the best and greatest I know. Kudos to
// Marian Vittek.
//
// .. _sglib: http://sglib.sourceforge.net/
//
// .. image:: https://github.com/concretecloud/rbtree/raw/master/perf_insert.png
//    :width: 90%
//    :align: center
//    :alt: insert
//
// .. image:: https://github.com/concretecloud/rbtree/raw/master/perf_delete.png
//    :width: 90%
//    :align: center
//    :alt: delete
//
// sglib has no delete_node. For many applications, a delete_node and a
// replace_node function is handy, since the application already has the right
// node to delete or replace.
//
// .. image:: https://github.com/concretecloud/rbtree/raw/master/perf_replace.png
//    :width: 90%
//    :align: center
//    :alt: replace
//
// Because we have parent pointer we can implement replace_node in constant
// time O(1). With sglib we have to add/remove for a replacement.
//
// Code size
// =========
//
// .. code-block:: text
//
//    0x018 T my_node_init
//    0x01b T my_tree_init
//    0x020 C my_nil_mem
//    0x02d T my_size
//    0x032 T my_iter_init
//    0x03d T my_find
//    0x042 T my_check_tree
//    0x043 T my_check_tree_rec
//    0x048 T my_iter_next
//    0x05d T my_replace
//    0x060 T my_delete
//    0x08b T my_replace_node
//    0x20e T my_insert
//    0x356 T my_delete_node
//
// About 2100 bytes. If NDEBUG or RB_NO_CHECK is defined the my_check_tree and
// my_check_tree_rec will be removed.
//
// Also _rb_rotate_left_m could be bound and called by delete and insert. But
// in my opinion 2100 bytes is small.
//
// Lessons learned
// ===============
//
// I thought I don't have to understand the red-black trees and could simply
// adjust an existing implementation. I chose poorly and the thing was
// inherently broken. I wasted a lot of time on it. They replaced the nil
// pointer with NULL and it resulted in a tree that works, but is not balanced.
// So my check_tree function failed and I tried to fix that implementation. It
// turns out bottom-up-fixups are very difficult to implement with NULL
// pointers. So after many hours wasted I just read Introductions to Algorithms
// and fixed my implementation.
//
// I thought I could adapt this code easily to make a persistent data-structure,
// but I found it is more important to have the parent pointers and therefore
// keep complexity at bay. If I am going to implement any persistent
// data-structures, I am going to build the persistent vector as used in closure
// and then convert the red-black tree to use vector-indexes and make it
// persistent on top of the persistent vector. It seems like the persistent
// vector can be built using reference-counting: pyrsistent_, so it should be
// possible.
//
// With the right mindset, generic and composable programming in C is awesome.
// Well, you need to expand the macros to debug. The following command will do
// that:
//
// .. code-block:: bash
//
//    $CC $CFLAGS -E -P infile.c | clang-format > outfile.c
//
// .. _pyrsistent: https://github.com/tobgu/pyrsistent/blob/master/pvectorcmodule.c
//
// Implementation
// ==============
//
// Based on Introduction to Algorithms: official_, wiki_, web_, pdf_ and
// archive_.
//
// .. _official: https://mitpress.mit.edu/books/introduction-algorithms
// .. _wiki: https://en.wikipedia.org/wiki/Introduction_to_Algorithms
// .. _web: http://staff.ustc.edu.cn/~csli/graduate/algorithms/book6/chap14.htm
// .. _pdf: http://www.realtechsupport.org/UB/SR/algorithms/Cormen_Algorithms_3rd.pdf
// .. _archive: https://archive.org/details/IntroductionToAlgorithms3edCorman_201508
//
// Properties
// ----------
//
// A binary search tree is a red-black tree if it satisfies the following
// red-black properties:
//
// 1. Every node is either red or black.
//
// 2. Every leaf (NIL) is black.
//
// 3. If a node is red, then both its children are black.
//
// 4. Every simple path from a node to a descendant leaf contains the same
//    number of black nodes.
//
// In order to understand the deletion, the concept of double (extra) blackness
// is introduced. If a black node was deleted its blackness is pushed down and a
// child can become extra black. This is the way property 1 can be violated.
//
// Definitions
// ===========
//
// RB_SIZE_T can be defined by the user to use size_t for example.
//
// .. code-block:: cpp
//
#ifndef rb_tree_h
#define rb_tree_h
#include <assert.h>
#ifndef RB_SIZE_T
#   define RB_SIZE_T int
#endif
#ifdef NDEBUG
#   define RB_NO_CHECK
#endif
//
// Basic traits
// ============
//
// Traits used by default (rb_x_m macros)
//
// .. code-block:: cpp
//
#define rb_color_m(x) (x)->color
#define rb_parent_m(x) (x)->parent
#define rb_left_m(x) (x)->left
#define rb_right_m(x) (x)->right
#define rb_value_m(x) (x)->value
//
// Context creation
// ================
//
// Create the type aliases. Actually only cx##_iter_t is used, since we can
// just refer to *type*. Note the const before cx##_nil_ptr, is the secret
// to make the code so small: the compiler just inserts the value into all
// comparisons with nil.
//
// .. code-block:: cpp
//
#define rb_new_context_m(cx, type) \
    typedef type cx##_type_t; \
    typedef type cx##_iter_t; \
    extern cx##_type_t* const cx##_nil_ptr; \


// Comparators
// ===========
//
// Some basic comparators, you would usually define your own.
//
// rb_safe_cmp_m
// ----------------
//
// Base for safe value comparators.
//
// x, y
//    Values to compare
//
// .. code-block:: cpp
//
#define rb_safe_cmp_m(x, y) \
    (((x)>(y) ? 1 : ((x)<(y) ? -1 : 0))) \

//
// rb_pointer_cmp_m
// ----------------
//
// Compares pointers.
//
// x, y
//    Nodes to compare
//
// .. code-block:: cpp
//
#define rb_pointer_cmp_m(x, y) \
    rb_safe_cmp_m(x, y) \


// rb_safe_value_cmp_m
// --------------------
//
// Safe value comparator. Compares nodes that have the rb_value_m trait.
//
// x, y
//    Nodes to compare
//
// .. code-block:: cpp
//
#define rb_safe_value_cmp_m(x, y) \
    rb_safe_cmp_m(rb_value_m(x), rb_value_m(y)) \


// rb_value_cmp_m
// ---------------
//
// Compares nodes that have the rb_value_m trait. Only safe if you only use
// 30bit values.
//
// x, y
//    Nodes to compare
//
// .. code-block:: cpp
//
#define rb_value_cmp_m(x, y) \
    (rb_value_m(x) - rb_value_m(y)) \


// Colors
// ======
//
// The obvious colors.
//
// .. code-block:: cpp
//
#define RB_BLACK 0
#define RB_RED   1

#define rb_is_black_m(x)   (x == RB_BLACK)
#define rb_is_red_m(x)     (x == RB_RED)

#define rb_make_black_m(x) x = RB_BLACK
#define rb_make_red_m(x)   x = RB_RED

// API
// ===
//
// Functions that are part of the API. The standard arguments are documented
// once:
//
// type
//    The type of the nodes in the red-black tree.
//
// nil
//    A pointer to the nil object.
//
// color
//    The color trait of the nodes in the rbtree.
//
// parent
//    The parent trait of the nodes in the rbtree is a pointer back to the
//    parent node.
//
// left
//    The left trait of the nodes in the rbtree is a pointer to the left branch
//    of the node.
//
// right
//    The right trait of the nodes in the rbtree is a pointer to the right
//    branch of the node.
//
// rb_node_init_m
// --------------
//
// Bound: cx##_node_init
//
// Initializes a node by setting the color to black and all pointers to nil.
//
// node
//    The node to initialize.
//
// .. code-block:: cpp
//
#define rb_node_init_m( \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        node \
) \
{ \
    color(node) = RB_BLACK; \
    parent(node) = nil; \
    left(node) = nil; \
    right(node) = nil; \
} \


// rb_for_m
// --------
//
// Generates a for-loop-header using the iterator.
//
// iter
//    The new iterator variable.
//
// elem
//    The pointer to the current element.
//
// .. code-block:: cpp
//
#define rb_for_m(cx, tree, iter, elem) \
    for( \
            cx##_iter_init(tree, &iter, &elem); \
            elem != NULL; \
            cx##_iter_next(iter, &elem) \
    ) \


// rb_iter_decl_m
// ---------------
//
// Also: rb_iter_decl_cx_m
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
#define rb_iter_decl_m(type, iter, elem) \
    type* iter = NULL; \
    type* elem = NULL; \


#define rb_iter_decl_cx_m(cx, iter, elem) \
    cx##_type_t* iter = NULL; \
    cx##_type_t* elem = NULL; \


// rb_iter_init_m
// --------------
//
// Bound: cx##_iter_init
//
// Initialize iterator. It will point to the first element.
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// iter
//    The iterator.
//
// elem
//    The pointer to the current element. Is NULL if the tree is empty.
//
//
// .. code-block:: cpp
//
#define rb_iter_init_m(nil, left, tree, elem) \
{ \
    if(tree == nil) { \
        elem = NULL; \
    } else { \
        elem = tree; \
        while(left(elem) != nil) \
            elem = left(elem); \
    } \
    if(elem == nil) { \
        elem = NULL; \
    } \
} \


// rb_iter_next_m
// --------------
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
#define _rb_iter_next_m( \
    nil, \
    parent, \
    left, \
    right, \
    elem, \
    tmp \
) \
do { \
    tmp = right(elem); \
    if(tmp != nil) { \
        elem = tmp; \
        while(left(elem) != nil) \
            elem = left(elem); \
        break; \
    } \
    for(;;) { \
        /* Next would be the root, we are done. */ \
        if(parent(elem) == nil) { \
            elem = NULL; \
            break; \
        } \
        tmp = parent(elem); \
        /* tmp is a left node, therefore it is the next node. */ \
        if(elem == left(tmp)) { \
            elem = tmp; \
            break; \
        } \
        elem = tmp; \
    } \
} while(0) \


#define rb_iter_next_m( \
    nil, \
    type, \
    parent, \
    left, \
    right, \
    elem \
) \
{ \
    type* __rb_next_tmp_; \
    _rb_iter_next_m( \
        nil, \
        parent, \
        left, \
        right, \
        elem, \
        __rb_next_tmp_ \
    ); \
} \


// rb_insert_m
// ------------
//
// Bound: cx##_insert
//
// Insert the node into the tree. This function might replace the root node
// (*tree*). If an equal node exists in the tree, the node will not be added and
// will still be in its initialized state.
//
// The bound function will return 0 on success.
//
// cmp
//    Comparator (rb_pointer_cmp_m or rb_safe_value_cmp_m could be used)
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// node
//    The node to insert.
//
// .. code-block:: cpp
//
#define _rb_insert_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        tree, \
        node, \
        c, /* current */ \
        p, /* parent */ \
        r  /* result */ \
) \
do { \
    assert(tree != NULL && "Tree was not initialized"); \
    assert(node != nil && "Cannot insert nil node"); \
    assert(node != nil && "Cannot insert nil node"); \
    assert( \
        parent(node) == nil && \
        left(node) == nil && \
        right(node) == nil && \
        tree != node && \
        "Node already used or not initialized" \
    ); \
    if(tree == nil) { \
        tree = node; \
        rb_make_black_m(color(tree)); \
        break; \
    } else { \
        assert(( \
            parent(tree) == nil && \
            rb_is_black_m(color(tree)) \
        ) && "Tree is not root"); \
    } \
    c = tree; \
    p = NULL; \
    r = 0; \
    while(c != nil) { \
        /* The node is already in the rbtree, we break. */ \
        r = cmp((c), (node)); \
        if(r == 0) { \
            break; \
        } \
        p = c; \
        /* Lesser on the left, greater on the right. */ \
        c = r > 0 ? left(c) : right(c); \
    } \
    /* The node is already in the rbtree, we break. */ \
    if(c != nil) { \
        break; \
    } \
 \
    parent(node) = p; \
    rb_make_red_m(color(node)); \
 \
    if(r > 0) { \
        left(p) = node; \
    } else { \
        right(p) = node; \
    } \
 \
    _rb_insert_fix_m( \
            type, \
            nil, \
            color, \
            parent, \
            left, \
            right, \
            tree, \
            node \
    ); \
} while(0); \


#define rb_insert_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        tree, \
        node \
) \
{ \
    type* __rb_ins_current_; \
    type* __rb_ins_parent_; \
    int   __rb_ins_result_; \
    _rb_insert_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        tree, \
        node, \
        __rb_ins_current_, \
        __rb_ins_parent_, \
        __rb_ins_result_ \
    ) \
} \


// rb_delete_node_m
// ----------------
//
// Bound: cx##_delete_node
//
// Delete a node from the tree. This function acts on an actual tree
// node. If you don't have it; use rb_find_m first or rb_delete_m. The root node
// (*tree*) can change.
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// node
//    The node to delete.
//
// .. code-block:: cpp
//
#define _rb_delete_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        x, \
        y \
) \
{ \
    assert(tree != NULL && "Tree was not initialized"); \
    assert(tree != nil && "Cannot remove node from empty tree"); \
    assert(node != nil && "Cannot delete nil node"); \
    assert(( \
        parent(node) != nil || \
        left(node) != nil || \
        right(node) != nil || \
        rb_is_black_m(color(node)) \
    ) && "Node is not in a tree"); \
    if(left(node) == nil || right(node) == nil) { \
        /* This node has at least one nil node, delete is simple. */ \
        y = node; \
    } else { \
        /* We need to find another node for deletion that has only one child. \
         * This is tree-next. */ \
        y = right(node); \
        while(left(y) != nil) { \
            y = left(y); \
        } \
    } \
 \
    /* If y has a child we have to attach it to the parent. */ \
    if(left(y) != nil) { \
        x = left(y); \
    } else { \
        x = right(y); \
    } \
 \
    /* Remove y from the tree. */ \
    parent(x) = parent(y); \
    if(parent(y) != nil) { \
        if(y == left(parent(y))) { \
            left(parent(y)) = x; \
        } else { \
            right(parent(y)) = x; \
        } \
    } else { \
        tree = x; \
    } \
 \
    /* A black node was removed, to fix the problem we pretend to have pushed the \
     * blackness onto x. Therefore x is double black and violates property 1. */ \
    if(rb_is_black_m(color(y))) { \
        _rb_delete_fix_m( \
                type, \
                nil, \
                color, \
                parent, \
                left, \
                right, \
                tree, \
                x \
        ); \
    } \
 \
    /* Replace y with the node since we don't control memory. */ \
    if(node != y) { \
        if(parent(node) == nil) { \
            tree = y; \
            parent(y) = nil; \
        } else { \
            if(node == left(parent(node))) { \
                left(parent(node)) = y; \
            } else if(node == right(parent(node))) { \
                right(parent(node)) = y; \
            } \
        } \
        if(left(node) != nil) { \
            parent(left(node)) = y; \
        } \
        if(right(node) != nil) { \
            parent(right(node)) = y; \
        } \
        parent(y) = parent(node); \
        left(y) = left(node); \
        right(y) = right(node); \
        color(y) = color(node); \
    } \
    /* Clear the node. */ \
    parent(node) = nil; \
    left(node) = nil; \
    right(node) = nil; \
    color(node) = RB_BLACK; \
} \


#define rb_delete_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node \
) \
{ \
    type* __rb_del_x_; \
    type* __rb_del_y_; \
    _rb_delete_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        __rb_del_x_, \
        __rb_del_y_ \
    ) \
} \


// rb_find_m
// ---------
//
// Bound: cx##_find
//
// Find a node using another node as key. The node will be set to nil if the
// key was not found.
//
// The bound function will return 0 on success.
//
// cmp
//    Comparator (rb_pointer_cmp_m or rb_safe_value_cmp_m could be used).
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// key
//    The node used as search key.
//
// node
//    The output node.
//
// .. code-block:: cpp

#define rb_find_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        tree, \
        key, \
        node \
) \
{ \
    assert(tree != NULL && "Tree was not initialized"); \
    assert(key != nil && "Do not use nil as search key"); \
    if(tree == nil) { \
        node = nil; \
    } else { \
        node = tree; \
        int __rb_find_result_ = 1; \
        while(__rb_find_result_ && node != nil) { \
            __rb_find_result_  = cmp((node), (key)); \
            if(__rb_find_result_ == 0) { \
                break; \
            } \
            node = __rb_find_result_ > 0 ? left(node) : right(node); \
        } \
    } \
} \


// rb_replace_node_m
// -----------------
//
// Bound: cx##_replace_node
//
// Replace a node with another. The cmp(old, new) has to return 0 or the
// function won't do anything.
//
// The bound function will return 0 on success.
//
// cmp
//    Comparator (rb_pointer_cmp_m or rb_safe_value_cmp_m could be used).
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// old
//    The node to be replaced.
//
// new
//    The new node. Has not to be initialized since all fields are replaced.
//
// .. code-block:: cpp

#define rb_replace_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        tree, \
        old, \
        new \
) \
{ \
    assert(tree != NULL && "Tree was not initialized"); \
    assert(tree != nil && "The tree can't be nil"); \
    assert(old != nil && "The old node can't be nil"); \
    assert(new != nil && "The new node can't be nil"); \
    assert(new != old && "The old and new node must differ"); \
    if(cmp((old), (new)) == 0) { \
        if(old == tree) { \
            tree = new; \
        } else { \
            if(old == left(parent(old))) { \
                left(parent(old)) = new; \
            } else { \
                right(parent(old)) = new; \
            } \
        } \
        if(left(old) != nil) { \
            parent(left(old)) = new; \
        } \
        if(right(old) != nil) { \
            parent(right(old)) = new; \
        } \
        parent(new) = parent(old); \
        left(new) = left(old); \
        right(new) = right(old); \
        color(new) = color(old); \
        /* Clear the old node. */ \
        parent(old) = nil; \
        left(old) = nil; \
        right(old) = nil; \
        color(old) = RB_BLACK; \
    } \
} \


// rb_bind_decl_m
// --------------
//
// Bind rbtree functions to a context. This only generates declarations.
//
// rb_bind_decl_cx_m is just an alias for consistency.
//
// cx
//    Name of the new context.
//
// type
//    The type of the nodes in the red-black tree.
//
// .. code-block:: cpp
//
#define rb_bind_decl_cx_m(cx, type) \
    rb_new_context_m(cx, type) \
    void \
    cx##_tree_init( \
            type** tree \
    ); \
    void \
    cx##_iter_init( \
            type* tree, \
            cx##_iter_t** iter, \
            type** elem \
    ); \
    void \
    cx##_iter_next( \
            cx##_iter_t* iter, \
            type** elem \
    ); \
    void \
    cx##_node_init( \
            type* node \
    ); \
    int \
    cx##_insert( \
            type** tree, \
            type* node \
    ); \
    void \
    cx##_delete_node( \
            type** tree, \
            type* node \
    ); \
    int \
    cx##_delete( \
            type** tree, \
            type* key, \
            type** node \
    ); \
    int \
    cx##_replace_node( \
            type** tree, \
            type* old, \
            type* new \
    ); \
    int \
    cx##_replace( \
            type** tree, \
            type* key, \
            type* new, \
            type** old \
    ); \
    int \
    cx##_find( \
            type* tree, \
            type* key, \
            type** node \
    ); \
    RB_SIZE_T \
    cx##_size( \
            type* tree \
    ); \
    rb_bind_decl_debug_cx_m(cx, type) \

#ifndef RB_NO_CHECK
#define rb_bind_decl_debug_cx_m(cx, type) \
    void \
    cx##_check_tree(type* tree); \
    void \
    cx##_check_tree_rec( \
            type* node, \
            int depth, \
            int *pathdepth \
    ); \

#else
#   define rb_bind_decl_debug_cx_m(cx, type)
#endif
#define rb_bind_decl_m(cx, type) rb_bind_decl_cx_m(cx, type)

// rb_bind_impl_m
// --------------
//
// Bind rbtree functions to a context. This only generates implementations.
//
// rb_bind_impl_m uses the standard traits: rb_color_m, rb_parent_m,
// rb_left_m, rb_right_m, whereas rb_bind_impl_cx_m expects you to create:
// cx##_color_m, cx##_parent_m, cx##_left_m, cx##_right_m.
//
// cx
//    Name of the new context.
//
// type
//    The type of the nodes in the red-black tree.
//
// .. code-block:: cpp
//
#define _rb_bind_impl_tr_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp \
) \
    cx##_type_t cx##_nil_mem; \
    cx##_type_t* const cx##_nil_ptr = &cx##_nil_mem; \
    void \
    cx##_tree_init( \
            type** tree \
    ) \
    { \
        rb_node_init_m( \
                cx##_nil_ptr, \
                color, \
                parent, \
                left, \
                right, \
                cx##_nil_ptr \
        ); \
        *tree = cx##_nil_ptr; \
    } \
    void \
    cx##_iter_init( \
            type* tree, \
            cx##_iter_t** iter, \
            type** elem \
    ) \
    { \
        (void)(iter); \
        rb_iter_init_m( \
            cx##_nil_ptr, \
            left, \
            tree, \
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
        rb_iter_next_m( \
            cx##_nil_ptr, \
            type, \
            parent, \
            left, \
            right, \
            *elem \
        ) \
    } \
    void \
    cx##_node_init( \
            type* node \
    ) \
    { \
        rb_node_init_m( \
                cx##_nil_ptr, \
                color, \
                parent, \
                left, \
                right, \
                node \
        ); \
    } \
    int \
    cx##_insert( \
            type** tree, \
            type* node \
    ) \
    { \
        rb_insert_m( \
            type, \
            cx##_nil_ptr, \
            color, \
            parent, \
            left, \
            right, \
            cmp, \
            *tree, \
            node \
        ); \
        return !( \
            parent(node) != cx##_nil_ptr || \
            left(node) != cx##_nil_ptr || \
            right(node) != cx##_nil_ptr || \
            *tree == node \
        ); \
    } \
    void \
    cx##_delete_node( \
            type** tree, \
            type* node \
    ) rb_delete_node_m( \
        type, \
        cx##_nil_ptr, \
        color, \
        parent, \
        left, \
        right, \
        *tree, \
        node \
    ) \
    int \
    cx##_delete( \
            type** tree, \
            type* key, \
            type** node \
    ) \
    { \
        if(cx##_find(*tree, key, node) == 0) { \
            cx##_delete_node(tree, *node); \
            return 0; \
        } \
        return 1; \
    } \
    int \
    cx##_replace_node( \
            type** tree, \
            type* old, \
            type* new \
    ) \
    { \
        rb_replace_node_m( \
            type, \
            cx##_nil_ptr, \
            color, \
            parent, \
            left, \
            right, \
            cmp, \
            *tree, \
            old, \
            new \
        ); \
        return !( \
            parent(old) == cx##_nil_ptr && \
            left(old) == cx##_nil_ptr && \
            right(old) == cx##_nil_ptr && \
            old != *tree \
        ); \
    } \
    int \
    cx##_replace( \
            type** tree, \
            type* key, \
            type* new, \
            type** old \
    ) \
    { \
        if(cx##_find(*tree, key, old) == 0) { \
            return cx##_replace_node(tree, *old, new); \
        } \
        return 1; \
    } \
    int \
    cx##_find( \
            type* tree, \
            type* key, \
            type** node \
    ) \
    { \
        rb_find_m( \
            type, \
            cx##_nil_ptr, \
            color, \
            parent, \
            left, \
            right, \
            cmp, \
            tree, \
            key, \
            *node \
        ); \
        return *node == cx##_nil_ptr; \
    } \
    RB_SIZE_T \
    cx##_size( \
            type* tree \
    ) \
    { \
        if(tree == cx##_nil_ptr) { \
            return 0; \
        } else { \
            return ( \
                cx##_size(left(tree)) + \
                cx##_size(right(tree)) + 1 \
            ); \
        } \
    } \
    _rb_bind_impl_debug_tr_m( \
            cx, \
            type, \
            color, \
            parent, \
            left, \
            right, \
            cmp \
    ) \

#ifndef RB_NO_CHECK
#define _rb_bind_impl_debug_tr_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp \
) \
    void \
    cx##_check_tree(type* tree) \
    { \
        int pathdepth = -1; \
        cx##_check_tree_rec(tree, 0, &pathdepth); \
    } \
    void \
    cx##_check_tree_rec( \
            type* node, \
            int depth, \
            int *pathdepth \
    ) rb_check_tree_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        node, \
        depth, \
        *pathdepth \
    ) \

#else
#define _rb_bind_impl_debug_tr_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp \
) \

#endif

#define rb_bind_impl_cx_m(cx, type) \
    _rb_bind_impl_tr_m( \
        cx, \
        type, \
        cx##_color_m, \
        cx##_parent_m, \
        cx##_left_m, \
        cx##_right_m, \
        cx##_cmp_m \
    ) \


#define rb_bind_impl_m(cx, type) \
    _rb_bind_impl_tr_m( \
        cx, \
        type, \
        rb_color_m, \
        rb_parent_m, \
        rb_left_m, \
        rb_right_m, \
        cx##_cmp_m \
    ) \


#define rb_bind_cx_m(cx, type) \
    rb_bind_decl_cx_m(cx, type) \
    rb_bind_impl_cx_m(cx, type) \


#define rb_bind_m(cx, type) \
    rb_bind_decl_m(cx, type) \
    rb_bind_impl_m(cx, type) \


// rb_check_tree_m
// ----------------
//
// Recursive: only works bound cx##_check_tree
//
// Check consistency of a tree
//
// node
//    Node to check.
//
// result
//    Zero on success, other on failure.
//
// .. code-block:: cpp
//
#define _rb_check_tree_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        node, \
        depth, \
        pathdepth, \
        tmp \
) \
{ \
    type* nil = cx##_nil_ptr; \
    if(node == nil) { \
        if(pathdepth < 0) { \
            pathdepth = depth; \
        } else { \
            assert(pathdepth == depth); \
        } \
    } else { \
        tmp = left(node); \
        if(tmp != nil) { \
            assert(parent(tmp) == node); \
            assert(cmp((tmp), (node)) < 0); \
        } \
        tmp = right(node); \
        if(tmp != nil) { \
            assert(parent(tmp) == node); \
            assert(cmp((tmp), (node)) > 0); \
        } \
        if(rb_is_red_m(color(node))) { \
            tmp = left(node); \
            if(tmp != nil) { \
                assert(rb_is_black_m(color(tmp))); \
            } \
            tmp = right(node); \
            if(tmp != nil) { \
                assert(rb_is_black_m(color(tmp))); \
            } \
            cx##_check_tree_rec(left(node), depth, &pathdepth); \
            cx##_check_tree_rec(right(node), depth, &pathdepth); \
        } else { \
            cx##_check_tree_rec(left(node), depth + 1, &pathdepth); \
            cx##_check_tree_rec(right(node), depth + 1, &pathdepth); \
        } \
    } \
} \

#define rb_check_tree_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        node, \
        depth, \
        pathdepth \
) \
{ \
    type* __rb_check_tmp_; \
    _rb_check_tree_m( \
        cx, \
        type, \
        color, \
        parent, \
        left, \
        right, \
        cmp, \
        node, \
        depth, \
        pathdepth, \
        __rb_check_tmp_ \
    ) \
} \


// Internal
// ========
//
// Functions that are used internally.
//
// _rb_rotate_left_m
// ------------------
//
// Internal: not bound
//
// A rotation is a local operation in a search tree that preserves in-order
// traversal key ordering. It is used to fix insert/deletion discrepancies.
// This operation might change the current root.
//
// _rb_rotate_right_m is _rb_rotate_left_m where left and right had been
// switched.
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// node
//    The node to initialize.
//
// .. code-block:: text
//
//               .---.   rotate_right   .---.
//               | y |     ------->     | x |
//               .---.                  .---.
//              /     ∖                /     ∖
//         .---'     .-'-.        .---'      .'--.
//         | x |     | C |        | A |      | y |
//         .---.     '---'        '---'      .---.
//        /     ∖                           /     ∖
//     .-'-.    .'--.                    .-'-.    .'--.
//     | A |    | B |      <------       | B |    | C |
//     '---'    '---'    rotate_left     '---'    '---'
//
// .. code-block:: cpp
//
#define __rb_rotate_left_m( \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        x, \
        y \
) \
{ \
    x = node; \
    y = right(x); \
 \
    /* Turn y's left sub-tree into x's right sub-tree. */ \
    right(x) = left(y); \
    if(left(y) != nil) { \
        parent(left(y)) = x; \
    } \
    /* y's new parent was x's parent. */ \
    parent(y) = parent(x); \
    if(parent(x) == nil) { \
        /* If x is root y becomes the new root. */ \
        tree = y; \
    } else { \
        /* Set the parent to point to y instead of x. */ \
        if(x == left(parent(x))) { \
            /* x was on the left of its parent. */ \
            left(parent(x)) = y; \
        } else { \
            /* x must have been on the right. */ \
            right(parent(x)) = y; \
        } \
    } \
    /* Finally, put x on y's left. */ \
    left(y) = x; \
    parent(x) = y; \
} \


#define _rb_rotate_left_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node \
) \
{ \
    type* __rb_rot_x_; \
    type* __rb_rot_y_; \
    __rb_rotate_left_m( \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        __rb_rot_x_, \
        __rb_rot_y_ \
    ); \
} \


#define _rb_rotate_left_tr_m(cx, tree, node) \
    _rb_rotate_left_m( \
        cx##_type_t, \
        cx##_nil_ptr, \
        rb_color_m, \
        rb_parent_m, \
        rb_left_m, \
        rb_right_m, \
        tree, \
        node \
    ) \


#define _rb_rotate_right_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node \
) \
    _rb_rotate_left_m( \
        type, \
        nil, \
        color, \
        parent, \
        right, /* Switched */ \
        left,  /* Switched */ \
        tree, \
        node \
    ) \


#define _rb_rotate_right_tr_m(cx, tree, node) \
    _rb_rotate_right_m( \
        cx##_type_t, \
        cx##_nil_ptr, \
        rb_color_m, \
        rb_parent_m, \
        rb_left_m, \
        rb_right_m, \
        tree, \
        node \
    ) \


// _rb_insert_fix_m
// ----------------
//
// Internal: not bound
//
// After inserting the new node is labeled red, and possibly destroys the
// red-black property. The main loop moves up the tree, restoring the red-black
// property.
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// node
//    The start-node to fix.
//
// .. code-block:: cpp
//
#define __rb_insert_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        x, \
        y \
) \
{ \
    x = node; \
    /* Move up the tree and fix property 3. */ \
    while( \
            (x != tree) && \
            rb_is_red_m(color(parent(x))) \
    ) { \
        if(parent(x) == left(parent(parent(x)))) { \
            _rb_insert_fix_node_m( \
                type, \
                nil, \
                color, \
                parent, \
                left, \
                right, \
                _rb_rotate_left_m, \
                _rb_rotate_right_m, \
                tree, \
                x, \
                y \
            ); \
        } else { \
            _rb_insert_fix_node_m( \
                type, \
                nil, \
                color, \
                parent, \
                right, /* Switched */ \
                left, /* Switched */ \
                _rb_rotate_left_m, \
                _rb_rotate_right_m, \
                tree, \
                x, \
                y \
            ); \
        } \
    } \
    rb_make_black_m(color(tree)); \
} \


#define _rb_insert_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node \
) \
{ \
    type* __rb_insf_x_; \
    type* __rb_insf_y_; \
    __rb_insert_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        __rb_insf_x_, \
        __rb_insf_y_ \
    ); \
} \


#define _rb_insert_fix_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        rot_left, \
        rot_right, \
        tree, \
        x, \
        y \
) \
{ \
    y = right(parent(parent(x))); \
    /* Case 1: z’s uncle y is red. */ \
    if(rb_is_red_m(color(y))) { \
        rb_make_black_m(color(parent(x))); \
        rb_make_black_m(color(y)); \
        rb_make_red_m(color(parent(parent(x)))); \
        /* Locally property 3 is fixed, but changing the color of the \
         * grandparent might have created a new violation. We continue with the \
         * grandparent. */ \
        x = parent(parent(x)); \
    } else { \
        /* Case 2: z’s uncle y is black and z is a right child. */ \
        if(x == right(parent(x))) { \
            x = parent(x); \
            rot_left( \
                type, \
                nil, \
                color, \
                parent, \
                left, \
                right, \
                tree, \
                x \
            ); \
        } \
        /* Case 3: z’s uncle y is black and z is a left child. */ \
        rb_make_black_m(color(parent(x))); \
        rb_make_red_m(color(parent(parent(x)))); \
        rot_right( \
            type, \
            nil, \
            color, \
            parent, \
            left, \
            right, \
            tree, \
            parent(parent(x)) \
        ); \
    } \
} \


// _rb_delete_fix_m
// ----------------
//
// Internal: not bound
//
// After deleting a black node, the blackness is pushed down to the child. If
// it is black, it is now double (extra) black. Property 1 has to be restored.
//
// tree
//    The root node of the tree. A pointer to nil represents an empty tree.
//
// node
//    The start-node to fix.
//
// .. code-block:: cpp
//
#define __rb_delete_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        x, \
        y \
) \
{ \
    x = node; \
    /* Move up fix extra blackness till x is red. */ \
    while( \
            (x != tree) && \
            rb_is_black_m(color(x)) \
    ) { \
        if(x == left(parent(x))) { \
            _rb_delete_fix_node_m( \
                type, \
                nil, \
                color, \
                parent, \
                left, \
                right, \
                _rb_rotate_left_m, \
                _rb_rotate_right_m, \
                tree, \
                x, \
                y \
            ); \
        } else { \
            _rb_delete_fix_node_m( \
                type, \
                nil, \
                color, \
                parent, \
                right, /* Switched */ \
                left, /* Switched */ \
                _rb_rotate_left_m, \
                _rb_rotate_right_m, \
                tree, \
                x, \
                y \
            ); \
        } \
    } \
    /* If x is red we can introduce a real black node. */ \
    rb_make_black_m(color(x)); \
} \


#define _rb_delete_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node \
) \
{ \
    type* __rb_delf_x_; \
    type* __rb_delf_y_; \
    __rb_delete_fix_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        tree, \
        node, \
        __rb_delf_x_, \
        __rb_delf_y_ \
    ); \
} \


#define _rb_delete_fix_node_m( \
        type, \
        nil, \
        color, \
        parent, \
        left, \
        right, \
        rot_left, \
        rot_right, \
        tree, \
        x, \
        w \
) \
{ \
    /* X is double (extra) black. Goal: introduce a real black node. */ \
    w = right(parent(x)); \
    /* Case 1: x’s sibling w is red. */ \
    if(rb_is_red_m(color(w))) { \
        rb_make_black_m(color(w)); \
        rb_make_red_m(color(parent(x))); \
        rot_left( \
            type, \
            nil, \
            color, \
            parent, \
            left, \
            right, \
            tree, \
            parent(x) \
        ); \
        /* Transforms into case 2, 3 or 4 */ \
        w = right(parent(x)); \
    } \
    if( \
            rb_is_black_m(color(left(w))) && \
            rb_is_black_m(color(right(w))) \
    ) { \
        /* Case 2: x’s sibling w is black, and both of w’s children are black. */ \
        rb_make_red_m(color(w)); \
        /* Double blackness move up. Reenter loop. */ \
        x = parent(x); \
    } else { \
        /* Case 3: x’s sibling w is black, w’s left child is red, and w’s right \
         * child is black. */ \
        if(rb_is_black_m(color(right(w)))) { \
            rb_make_black_m(color(left(w))); \
            rb_make_red_m(color(w)); \
            rot_right( \
                type, \
                nil, \
                color, \
                parent, \
                left, \
                right, \
                tree, \
                w \
            ); \
            w = right(parent(x)); \
        } \
        /* Case 3: x’s sibling w is black, w’s left child is red, and w’s right \
         * child is black. */ \
        color(w) = color(parent(x)); \
        rb_make_black_m(color(parent(x))); \
        rb_make_black_m(color(right(w))); \
        rot_left( \
            type, \
            nil, \
            color, \
            parent, \
            left, \
            right, \
            tree, \
            parent(x) \
        ); \
        /* Terminate the loop. */ \
        x = tree; \
    } \
    /* When the loop ends x is red and will be colored black. */ \
} \


#endif // rb_tree_h

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
