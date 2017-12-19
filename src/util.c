// ====
// Util
// ====
//
// Common utility functions.
//

// Project includes
// ================
//
// .. code-block:: cpp

#include "util.h"
#include "chirp.h"
#include "rbtree.h"

// System includes
// ===============
//
// .. code-block:: cpp

#include <stdarg.h>

// Declarations
// ============
//
// .. code-block:: cpp
//

static int             _ch_always_encrypt = 0;
static ch_free_cb_t    _ch_free_cb        = free;
static ch_alloc_cb_t   _ch_alloc_cb       = malloc;
static ch_realloc_cb_t _ch_realloc_cb     = realloc;

// Logging and assert macros
// =========================
//
// Colors
// ------
//
// .. code-block:: cpp

static char* const _ch_lg_reset     = "\x1B[0m";
static char* const _ch_lg_err       = "\x1B[1;31m";
static char* const _ch_lg_colors[8] = {
        "\x1B[0;34m",
        "\x1B[0;32m",
        "\x1B[0;36m",
        "\x1B[0;33m",
        "\x1B[1;34m",
        "\x1B[1;32m",
        "\x1B[1;36m",
        "\x1B[1;33m",
};

#ifdef CH_ENABLE_ASSERTS

// Debug alloc tracking
// ====================
//
// Since we can't use valgrind and rr at the same time and I needed to debug a
// leak with rr, I added this memory leak debugging code. Since we use rr, the
// pointer to the allocation is enough, we don't need any meta information.

// .. c:var:: uv_mutex_t _ch_at_lock
//
//    Lock for alloc tracking
//
// .. code-block:: cpp
//
static uv_mutex_t _ch_at_lock;

struct ch_alloc_track_s;
typedef struct ch_alloc_track_s ch_alloc_track_t;
struct ch_alloc_track_s {
    void*             buf;
    char              color;
    ch_alloc_track_t* parent;
    ch_alloc_track_t* left;
    ch_alloc_track_t* right;
};

#define _ch_at_cmp_m(x, y) rb_safe_cmp_m(x->buf, y->buf)

rb_bind_m(_ch_at, ch_alloc_track_t) CH_ALLOW_NL;

ch_alloc_track_t* _ch_alloc_tree;

// .. c:function::
int
ch_at_allocated(void* buf)
//    :noindex:
//
//    see: :c:func:`ch_at_allocated`
//
// .. code-block:: cpp
//
{
    ch_alloc_track_t  key;
    ch_alloc_track_t* value;
    key.buf = buf;
    return _ch_at_find(_ch_alloc_tree, &key, &value) == CH_SUCCESS;
}

// .. c:function::
static void*
_ch_at_alloc(void* buf)
//
//    Track a memory allocation.
//
//    :param void* buf: Pointer to the buffer to track.
//
// .. code-block:: cpp
//
{
    /* We do not track failed allocations */
    if (buf == NULL) {
        return buf;
    }
    ch_alloc_track_t* track = _ch_alloc_cb(sizeof(*track));
    assert(track);
    /* We treat a failure to track as an alloc failure. This code is here
     * if we ever need to use this in release mode (or just for
     * correctness). */
    if (track == NULL) {
        return NULL;
    }
    _ch_at_node_init(track);
    track->buf = buf;
    uv_mutex_lock(&_ch_at_lock);
    int ret = _ch_at_insert(&_ch_alloc_tree, track);
    uv_mutex_unlock(&_ch_at_lock);
    assert(ret == 0);
    return buf;
}

// .. c:function::
void
ch_at_cleanup(void)
//    :noindex:
//
//    see: :c:func:`ch_at_cleanup`
//
// .. code-block:: cpp
//
{
    if (_ch_alloc_tree != _ch_at_nil_ptr) {
        fprintf(stderr, "Leaked allocations: \n");
        while (_ch_alloc_tree != _ch_at_nil_ptr) {
            ch_alloc_track_t* item;
            item = _ch_alloc_tree;
            fprintf(stderr, "%p ", item->buf);
            _ch_at_delete_node(&_ch_alloc_tree, item);
            _ch_free_cb(item);
        }
        fprintf(stderr, "\n");
        A(0, "There is a memory leak")
    }
    uv_mutex_destroy(&_ch_at_lock);
}

// .. c:function::
void
ch_at_init(void)
//    :noindex:
//
//    see: :c:func:`ch_at_init`
//
// .. code-block:: cpp
//
{
    uv_mutex_init(&_ch_at_lock);
    _ch_at_tree_init(&_ch_alloc_tree);
}

// .. c:function::
static void
_ch_at_free(void* buf)
//
//    Track a freed memory allocation.
//
//    :param void* buf: Pointer to the buffer to track.
//
// .. code-block:: cpp
//
{
    ch_alloc_track_t  key;
    ch_alloc_track_t* track;
    key.buf = buf;
    uv_mutex_lock(&_ch_at_lock);
    int ret = _ch_at_delete(&_ch_alloc_tree, &key, &track);
    uv_mutex_unlock(&_ch_at_lock);
    assert(ret == 0);
    _ch_free_cb(track);
}

// .. c:function::
static void*
_ch_at_realloc(void* buf, void* rbuf)
//
//    Track a memory reallocation.
//
//    :param void* buf: Pointer to the buffer that has been reallocated.
//    :param void* rbuf: Pointer to the new buffer.
//
// .. code-block:: cpp
//
{
    /* We do not track failed reallocations */
    if (rbuf == NULL) {
        return rbuf;
    }
    ch_alloc_track_t  key;
    ch_alloc_track_t* track;
    /* Shortcut if the allocator was able to extend the allocation */
    if (buf == rbuf) {
        return rbuf;
    }
    key.buf = buf;
    uv_mutex_lock(&_ch_at_lock);
    int ret = _ch_at_delete(&_ch_alloc_tree, &key, &track);
    uv_mutex_unlock(&_ch_at_lock);
    assert(ret == 0);
    track->buf = rbuf;
    ret        = _ch_at_insert(&_ch_alloc_tree, track);
    assert(ret == 0);
    return rbuf;
}
#endif

// Definitions
// ===========

// .. c:function::
void*
ch_alloc(size_t size)
//    :noindex:
//
//    see: :c:func:`ch_alloc`
//
// .. code-block:: cpp
//
{
    void* buf = _ch_alloc_cb(size);
    /* Assert memory (do not rely on this, implement it robust: be graceful and
     * return error to user) */
    A(buf, "Allocation failure");
#ifdef CH_ENABLE_ASSERTS
    return _ch_at_alloc(buf);
#else
    return buf;
#endif
}

// .. c:function::
void
ch_bytes_to_hex(uint8_t* bytes, size_t bytes_size, char* str, size_t str_size)
//    :noindex:
//
//    see: :c:func:`ch_bytes_to_hex`
//
// .. code-block:: cpp
//
{
    size_t i;
    A(bytes_size * 2 + 1 <= str_size, "Not enough space for string");
    for (i = 0; i < bytes_size; i++) {
        snprintf(str, 3, "%02X", bytes[i]);
        str += 2;
    }
    *str = 0;
}

// .. c:function::
void
ch_chirp_set_always_encrypt()
//    :noindex:
//
//    see: :c:func:`ch_chirp_set_always_encrypt`
//
// .. code-block:: cpp
//
{
    _ch_always_encrypt = 1;
}

// .. c:function::
void
ch_free(void* buf)
//    :noindex:
//
//    see: :c:func:`ch_free`
//
// .. code-block:: cpp
//
{
#ifdef CH_ENABLE_ASSERTS
    _ch_at_free(buf);
#endif
    _ch_free_cb(buf);
}

// .. c:function::
int
ch_is_local_addr(ch_text_address_t* addr)
//    :noindex:
//
//    see: :c:func:`ch_is_local_addr`
//
// .. code-block:: cpp
//
{
    if (_ch_always_encrypt) {
        return 0;
    } else {
        return (strncmp("::1", addr->data, sizeof(addr->data)) == 0 ||
                strncmp("127.0.0.1", addr->data, sizeof(addr->data)) == 0);
    }
}

// .. c:function::
void
ch_random_ints_as_bytes(uint8_t* bytes, size_t len)
//    :noindex:
//
//    see: :c:func:`ch_random_ints_as_bytes`
//
// .. code-block:: cpp
//
{
    size_t i;
    int    tmp_rand;
    A(len % 4 == 0, "len must be multiple of four");
#ifdef _WIN32
#if RAND_MAX < 16384 || INT_MAX < 16384 // 2**14
#error Seriously broken compiler or platform
#else  // RAND_MAX < 16384 || INT_MAX < 16384
    for (i = 0; i < len; i += 2) {
        tmp_rand = rand();
        memcpy(bytes + i, &tmp_rand, 2);
    }
#endif // RAND_MAX < 16384 || INT_MAX < 16384
#else  // _WIN32
#if RAND_MAX < 1073741824 || INT_MAX < 1073741824 // 2**30
#ifdef CH_ACCEPT_STRANGE_PLATFORM
    /* WTF, fallback platform */
    (void) (tmp_rand);
    for (i = 0; i < len; i++) {
        bytes[i] = ((unsigned int) rand()) % 256;
    }
#else // ACCEPT_STRANGE_PLATFORM
/* cppcheck-suppress preprocessorErrorDirective */
#error Unexpected RAND_MAX / INT_MAX, define CH_ACCEPT_STRANGE_PLATFORM
#endif // ACCEPT_STRANGE_PLATFORM
#else  // RAND_MAX < 1073741824 || INT_MAX < 1073741824
    /* Tested: this is 4 times faster*/
    for (i = 0; i < len; i += 4) {
        tmp_rand = rand();
        memcpy(bytes + i, &tmp_rand, 4);
    }
#endif // RAND_MAX < 1073741824 || INT_MAX < 1073741824
#endif // _WIN32
}

// .. c:function::
void*
ch_realloc(void* buf, size_t size)
//    :noindex:
//
//    see: :c:func:`ch_realloc`
//
// .. code-block:: cpp
//
{
    void* rbuf = _ch_realloc_cb(buf, size);
    /* Assert memory (do not rely on this, implement it robust: be graceful and
     * return error to user) */
    A(rbuf, "Reallocation failure");
#ifdef CH_ENABLE_ASSERTS
    return _ch_at_realloc(buf, rbuf);
#else
    return rbuf;
#endif
}

// .. c:function::
CH_EXPORT
void
ch_set_alloc_funcs(
        ch_alloc_cb_t alloc, ch_realloc_cb_t realloc, ch_free_cb_t free)
//    :noindex:
//
//    see: :c:func:`ch_set_alloc_funcs`
//
// .. code-block:: cpp
//
{
    _ch_alloc_cb   = alloc;
    _ch_realloc_cb = realloc;
    _ch_free_cb    = free;
}

// .. c:function::
ch_error_t
ch_textaddr_to_sockaddr(
        int                      af,
        ch_text_address_t*       text,
        uint16_t                 port,
        struct sockaddr_storage* addr)
//    :noindex:
//
//    see: :c:func:`ch_textaddr_to_sockaddr`
//
// .. code-block:: cpp
//
{
    if (af == AF_INET6) {
        return uv_ip6_addr(text->data, port, (struct sockaddr_in6*) addr);
    } else {
        A(af, "Unknown IP protocol");
        return uv_ip4_addr(text->data, port, (struct sockaddr_in*) addr);
    }
}

// .. c:function::
CH_EXPORT
void
ch_write_log(
        ch_chirp_t* chirp,
        char*       file,
        int         line,
        char*       message,
        char*       clear,
        int         error,
        ...)
//    :noindex:
//
//    see: :c:func:`ch_write_log`
//
// .. code-block:: cpp
//
{
    va_list args;
    va_start(args, error);
    char* tfile = strrchr(file, '/');
    if (tfile != NULL) {
        file = tfile + 1;
    }
    char buf1[1024];
    if (chirp->_log != NULL) {
        char buf2[1024];
        snprintf(buf1, 1024, "%s:%5d %s %s", file, line, message, clear);
        vsnprintf(buf2, 1024, buf1, args);
        chirp->_log(buf2, error);
    } else {
        uint8_t log_id = ((uint8_t) chirp->_->identity[0]) % 8;
        char*   tmpl;
        char*   first;
        char*   second;
        if (error) {
            tmpl   = "%s%02X%02X%s %17s:%5d Error: %s%s %s%s\n";
            first  = _ch_lg_err;
            second = _ch_lg_err;
        } else {
            tmpl   = "%s%02X%02X%s %17s:%5d %s%s %s%s\n";
            first  = _ch_lg_colors[log_id];
            second = _ch_lg_reset;
        }
        /* From doc: If the format is exhausted while arguments remain, the
         * excess arguments shall be evaluated but are otherwise ignored. */
        snprintf(
                buf1,
                1024,
                tmpl,
                first,
                chirp->_->identity[0],
                chirp->_->identity[1],
                second,
                file,
                line,
                _ch_lg_colors[log_id],
                message,
                _ch_lg_reset,
                clear);
        vfprintf(stderr, buf1, args);
        fflush(stderr);
    }
    va_end(args);
}
