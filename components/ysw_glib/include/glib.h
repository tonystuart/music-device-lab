// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define FALSE 0
#define TRUE (!FALSE)
#define GLIB_CHECK_VERSION(major,minor,patch) 1
#define G_BIG_ENDIAN 4321
#define G_LITTLE_ENDIAN 1234
#define G_BYTE_ORDER G_LITTLE_ENDIAN

#define GINT16_TO_LE(val)((gint16) (val))
#define GINT32_TO_LE(val)((gint32) (val))

#define GINT16_FROM_LE(val)	(GINT16_TO_LE(val))
#define GINT32_FROM_LE(val)	(GINT32_TO_LE(val))

//#define G_LIKELY(expr)(__builtin_expect (_G_BOOLEAN_EXPR(expr), 1))
//#define G_UNLIKELY(expr)(__builtin_expect (_G_BOOLEAN_EXPR(expr), 0))

#define G_LIKELY(expr)(expr)
#define G_UNLIKELY(expr)(expr)

typedef void * GStaticMutex;
typedef void * GStaticRecMutex;
typedef void * GPrivate;
typedef void * gpointer;

typedef int gint;
typedef char gchar;
typedef bool gboolean;
typedef unsigned int guint;
typedef unsigned long gsize;
typedef unsigned long gulong;

typedef int16_t gint16;
typedef int32_t gint32;
typedef int64_t gint64;

typedef struct stat GStatBuf;
typedef struct timeval GTimeVal;

typedef gpointer (*GThreadFunc) (gpointer data);

typedef struct {
    gchar *message;
} GError;

typedef enum {
    G_FILE_TEST_IS_REGULAR    = 1 << 0,
    G_FILE_TEST_IS_SYMLINK    = 1 << 1,
    G_FILE_TEST_IS_DIR        = 1 << 2,
    G_FILE_TEST_IS_EXECUTABLE = 1 << 3,
    G_FILE_TEST_EXISTS        = 1 << 4
} GFileTest;

typedef union _GMutex
{
    gpointer p;
    guint i[2];
} GMutex;

typedef struct _GRecMutex
{
    gpointer p;
    guint i[2];
} GRecMutex;

typedef struct _GCond
{
    gpointer p;
    guint i[2];
} GCond;

typedef enum
{
    G_THREAD_PRIORITY_LOW,
    G_THREAD_PRIORITY_NORMAL,
    G_THREAD_PRIORITY_HIGH,
    G_THREAD_PRIORITY_URGENT
} GThreadPriority;

typedef struct _GThread
{
    unsigned long int id;
} GThread;

// https://developer.gnome.org/glib/stable/glib-Memory-Allocation.html

#define g_new(struct_type, n_structs)(struct_type *)g_malloc(sizeof(struct_type) * n_structs)

gpointer g_malloc(gsize n_bytes);

void g_free(gpointer mem);

// https://developer.gnome.org/glib/stable/glib-Threads.html

GThread * g_thread_try_new(const gchar *name, GThreadFunc func, gpointer data, GError **error);

gpointer g_thread_join(GThread *thread);

void g_thread_unref(GThread *thread);

void g_cond_broadcast(GCond *cond);

void g_cond_clear(GCond *cond);

void g_cond_init(GCond *cond);

void g_cond_signal(GCond *cond);

void g_cond_wait(GCond *cond, GMutex *mutex);

void g_mutex_clear(GMutex *mutex);

void g_mutex_init(GMutex *mutex);

void g_mutex_lock(GMutex *mutex);

void g_mutex_unlock(GMutex *mutex);

void g_rec_mutex_clear(GRecMutex *rec_mutex);

void g_rec_mutex_init(GRecMutex *rec_mutex);

void g_rec_mutex_lock(GRecMutex *rec_mutex);

void g_rec_mutex_unlock(GRecMutex *rec_mutex);

gpointer g_private_get(GPrivate *key);

void g_private_set(GPrivate *key, gpointer value);

// https://developer.gnome.org/glib/stable/glib-File-Utilities.html

gboolean g_file_test(const gchar *filename, GFileTest test);

int g_stat(const gchar *filename, GStatBuf *buf);

// https://developer.gnome.org/glib/stable/glib-Atomic-Operations.html

gint g_atomic_int_add(volatile gint *atomic, gint val);

gboolean g_atomic_int_compare_and_exchange(volatile gint *atomic, gint oldval, gint newval);

gint g_atomic_int_exchange_and_add(volatile gint *atomic, gint val);

gint g_atomic_int_get(const volatile gint *atomic);

void g_atomic_int_set(volatile gint *atomic, gint newval);

void g_atomic_int_inc(gint *atomic);

gpointer g_atomic_pointer_get(const volatile void *atomic);

gboolean g_atomic_pointer_compare_and_exchange(volatile void *atomic, gpointer oldval, gpointer newval);

// https://developer.gnome.org/glib/stable/glib-Error-Reporting.html

void g_clear_error(GError **err);

// https://developer.gnome.org/glib/stable/glib-Date-and-Time-Functions.html

gint64 g_get_real_time(void);

void g_get_current_time(GTimeVal *result);

void g_usleep(gulong microseconds);

// https://developer.gnome.org/glib/stable/glib-Warnings-and-Assertions.html

#define g_return_val_if_fail(expr,val) assert(expr)

// https://developer.gnome.org/glib/stable/glib-Shell-related-Utilities.html

gboolean g_shell_parse_argv(const gchar *command_line, gint *argcp, gchar ***argvp, GError **error);

// https://developer.gnome.org/glib/stable/glib-String-Utility-Functions.html

void g_strfreev(gchar **str_array);

