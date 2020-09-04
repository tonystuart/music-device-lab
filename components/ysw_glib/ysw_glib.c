// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "glib.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "errno.h"
#include "pthread.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define TAG "YSW_GLIB"

static void g_thread_abort(gint status, const char *source)
{
    ESP_LOGE(TAG, "g_thread_abort status=%d, source=%s", status, source);
    abort();
}

static pthread_mutex_t *g_rec_mutex_impl_new(void)
{
  pthread_mutexattr_t attr;
  pthread_mutex_t *mutex;

  mutex = ysw_heap_allocate(sizeof(pthread_mutex_t));

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(mutex, &attr);
  pthread_mutexattr_destroy(&attr);

  return mutex;
}

static void g_rec_mutex_impl_free(pthread_mutex_t *mutex)
{
    pthread_mutex_destroy (mutex);
    ysw_heap_free(mutex);
}

static pthread_mutex_t *g_mutex_impl_new(void)
{
    pthread_mutexattr_t *pattr = NULL;
    pthread_mutex_t *mutex;
    gint status;

    mutex = ysw_heap_allocate(sizeof(pthread_mutex_t));

    // See https://stackoverflow.com/questions/19863734/what-is-pthread-mutex-adaptive-np
    if ((status = pthread_mutex_init(mutex, pattr)) != 0) {
        g_thread_abort(status, "pthread_mutex_init");
    }

    return mutex;
}

static void g_mutex_impl_free(pthread_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
    ysw_heap_free(mutex);
}

static inline pthread_mutex_t *g_mutex_get_impl(GMutex *mutex)
{
    pthread_mutex_t *impl = g_atomic_pointer_get(&mutex->p);

    if (impl == NULL) {
        impl = g_mutex_impl_new();
        if (!g_atomic_pointer_compare_and_exchange(&mutex->p, NULL, impl)) {
            g_mutex_impl_free(impl);
        }
        impl = mutex->p;
    }

    return impl;
}

static inline pthread_mutex_t *g_rec_mutex_get_impl(GRecMutex *rec_mutex)
{
    pthread_mutex_t *impl = g_atomic_pointer_get(&rec_mutex->p);

    if (impl == NULL) {
        impl = g_rec_mutex_impl_new();
        if (!g_atomic_pointer_compare_and_exchange(&rec_mutex->p, NULL, impl)) {
            g_rec_mutex_impl_free(impl);
        }
        impl = rec_mutex->p;
    }

    return impl;
}

static pthread_cond_t *g_cond_impl_new(void)
{
    pthread_condattr_t attr;
    pthread_cond_t *cond;
    gint status;

    // NB: just a stub in ~/esp/esp-idf-v4.0/components/pthread/pthread_cond_var.c
    pthread_condattr_init(&attr);

    // NB: just a stub in ~/esp/esp-idf-v4.0/components/newlib/pthread.c
    if ((status = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0) {
        g_thread_abort(status, "pthread_condattr_setclock");
    }

    cond = ysw_heap_allocate(sizeof(pthread_cond_t));

    if ((status = pthread_cond_init(cond, &attr)) != 0) {
        g_thread_abort(status, "pthread_cond_init");
    }

    // NB: just a stub in ~/git/player/components/ysw_glib/ysw_pthread.c
    pthread_condattr_destroy(&attr);

    return cond;
}

// https://developer.gnome.org/glib/stable/glib-Memory-Allocation.html

gpointer g_malloc(gsize n_bytes)
{
    ESP_LOGV(TAG, "g_malloc entered");

    gpointer mem;

    mem = ysw_heap_allocate(n_bytes);

    return mem;
}

gpointer g_realloc(gpointer mem, gsize n_bytes)
{
    ESP_LOGV(TAG, "g_realloc entered");

    gpointer new_mem;
    
    new_mem = ysw_heap_reallocate(mem, n_bytes);

    return new_mem;
}

void g_free(gpointer mem)
{
    ESP_LOGV(TAG, "g_free entered");

    ysw_heap_free(mem);
}

// https://developer.gnome.org/glib/stable/glib-Threads.html

GThread * g_thread_try_new(const gchar *name, GThreadFunc func, gpointer data, GError **error)
{
    ESP_LOGD(TAG, "g_thread_try_new entered");

    GThread *thread = ysw_heap_allocate(sizeof(GThread));
    int rc = pthread_create(&thread->id, NULL, func, data);
    if (rc == -1) {
        ESP_LOGE(TAG, "pthread_create failed");
        abort();
    }
    return thread;
}

gpointer g_thread_join(GThread *thread)
{
    ESP_LOGE(TAG, "g_thread_join entered");
    return 0;
}

void g_thread_unref(GThread *thread)
{
    ESP_LOGE(TAG, "g_thread_unref entered");
}

void g_cond_broadcast(GCond *cond)
{
    ESP_LOGE(TAG, "g_cond_broadcast entered");
}

void g_cond_clear(GCond *cond)
{
    ESP_LOGE(TAG, "g_cond_clear entered");
}

void g_cond_init(GCond *cond)
{
    ESP_LOGV(TAG, "g_cond_init entered");

    cond->p = g_cond_impl_new();
}

void g_cond_signal(GCond *cond)
{
    ESP_LOGE(TAG, "g_cond_signal entered");
}

void g_cond_wait(GCond *cond, GMutex *mutex)
{
    ESP_LOGE(TAG, "g_cond_wait entered");
}

void g_mutex_clear(GMutex *mutex)
{
    ESP_LOGE(TAG, "g_mutex_clear entered");
}

void g_mutex_init(GMutex *mutex)
{
    ESP_LOGV(TAG, "g_mutex_init entered");

    mutex->p = g_mutex_impl_new();
}

void g_mutex_lock(GMutex *mutex)
{
    ESP_LOGV(TAG, "g_mutex_lock entered");

    gint status;

    if ((status = pthread_mutex_lock(g_mutex_get_impl(mutex))) != 0) {
        g_thread_abort(status, "pthread_mutex_lock");
    }
}

void g_mutex_unlock(GMutex *mutex)
{
    ESP_LOGV(TAG, "g_mutex_unlock entered");

    gint status;

    if ((status = pthread_mutex_unlock(g_mutex_get_impl(mutex))) != 0) {
        g_thread_abort(status, "pthread_mutex_unlock");
    }
}

void g_rec_mutex_clear(GRecMutex *rec_mutex)
{
    ESP_LOGE(TAG, "g_rec_mutex_clear entered");
}

void g_rec_mutex_init(GRecMutex *rec_mutex)
{
    ESP_LOGV(TAG, "g_rec_mutex_init entered");

    rec_mutex->p = g_rec_mutex_impl_new();
}

void g_rec_mutex_lock(GRecMutex *rec_mutex)
{
    ESP_LOGV(TAG, "g_rec_mutex_lock entered");

    pthread_mutex_lock(g_rec_mutex_get_impl(rec_mutex));
}

void g_rec_mutex_unlock(GRecMutex *rec_mutex)
{
    ESP_LOGV(TAG, "g_rec_mutex_unlock entered");

    pthread_mutex_unlock(rec_mutex->p);
}

gpointer g_private_get(GPrivate *key)
{
    ESP_LOGE(TAG, "g_private_get entered");
    return 0;
}

void g_private_set(GPrivate *key, gpointer value)
{
    ESP_LOGE(TAG, "g_private_set entered");
}

// https://developer.gnome.org/glib/stable/glib-File-Utilities.html

gboolean g_file_test(const gchar *filename, GFileTest test)
{
    ESP_LOGD(TAG, "g_file_test entered, filename=%s, test=%#x", filename, test);

    struct stat sb;
    int rc = stat(filename, &sb);
    if (rc == -1) {
        ESP_LOGD(TAG, "g_file_test stat errno=%d", errno);
        return false;
    }

    if ((test & G_FILE_TEST_IS_REGULAR) && !(S_ISREG(sb.st_mode))) {
        ESP_LOGD(TAG, "g_file_test not regular");
        return false;
    }

    if ((test & G_FILE_TEST_IS_SYMLINK) && !(S_ISLNK(sb.st_mode))) {
        ESP_LOGD(TAG, "g_file_test not symlink");
        return false;
    }

    if ((test & G_FILE_TEST_IS_DIR) && !(S_ISDIR(sb.st_mode))) {
        ESP_LOGD(TAG, "g_file_test not dir");
        return false;
    }

    if ((test & G_FILE_TEST_IS_EXECUTABLE) && !(sb.st_mode & S_IEXEC)) {
        ESP_LOGD(TAG, "g_file_test not exec");
        return false;
    }

    return TRUE;
}

int g_stat(const gchar *filename, GStatBuf *buf)
{
    ESP_LOGV(TAG, "g_stat entered");

    return stat(filename, buf);
}

// https://developer.gnome.org/glib/stable/glib-Atomic-Operations.html

static pthread_mutex_t g_atomic_lock = PTHREAD_MUTEX_INITIALIZER;

gint g_atomic_int_add(volatile gint *atomic, gint val)
{
    ESP_LOGV(TAG, "g_atomic_int_add entered");

    gint oldval;

    pthread_mutex_lock(&g_atomic_lock);
    oldval = *atomic;
    *atomic = oldval + val;
    pthread_mutex_unlock(&g_atomic_lock);

    return oldval;
}

gboolean g_atomic_int_compare_and_exchange(volatile gint *atomic, gint oldval, gint newval)
{
    ESP_LOGV(TAG, "g_atomic_int_compare_and_exchange entered");

    gboolean success;

    pthread_mutex_lock(&g_atomic_lock);

    if ((success = (*atomic == oldval))) {
        *atomic = newval;
    }

    pthread_mutex_unlock(&g_atomic_lock);

    return success;
}

gint g_atomic_int_exchange_and_add(volatile gint *atomic, gint val)
{
    ESP_LOGV(TAG, "g_atomic_int_exchange_and_add entered");

    return g_atomic_int_add(atomic, val);
}

gint g_atomic_int_get(const volatile gint *atomic)
{
    ESP_LOGV(TAG, "g_atomic_int_get entered");

    gint value;

    pthread_mutex_lock(&g_atomic_lock);
    value = *atomic;
    pthread_mutex_unlock(&g_atomic_lock);

    return value;
}

void g_atomic_int_set(volatile gint *atomic, gint newval)
{
    ESP_LOGV(TAG, "g_atomic_int_set entered");

    pthread_mutex_lock(&g_atomic_lock);
    *atomic = newval;
    pthread_mutex_unlock(&g_atomic_lock);
}

void g_atomic_int_inc(gint *atomic)
{
    ESP_LOGV(TAG, "g_atomic_int_inc entered");

    pthread_mutex_lock(&g_atomic_lock);
    (*atomic)++;
    pthread_mutex_unlock(&g_atomic_lock);
}
 
gpointer g_atomic_pointer_get(const volatile void *atomic)
{
    const volatile gpointer *ptr = atomic;
    gpointer value;

    pthread_mutex_lock(&g_atomic_lock);
    value = *ptr;
    pthread_mutex_unlock(&g_atomic_lock);

    return value;
}

gboolean g_atomic_pointer_compare_and_exchange(volatile void *atomic, gpointer oldval, gpointer newval)
{
    volatile gpointer *ptr = atomic;
    gboolean success;

    pthread_mutex_lock(&g_atomic_lock);

    if ((success = (*ptr == oldval))) {
        *ptr = newval;
    }

    pthread_mutex_unlock(&g_atomic_lock);

    return success;
}

// https://developer.gnome.org/glib/stable/glib-Error-Reporting.html

void g_clear_error(GError **err)
{
    ESP_LOGE(TAG, "g_clear_error entered");
}

// https://developer.gnome.org/glib/stable/glib-Date-and-Time-Functions.html

gint64 g_get_real_time(void)
{
    struct timeval r;

    gettimeofday(&r, NULL);

    return (((gint64) r.tv_sec) * 1000000) + r.tv_usec;
}

void g_get_current_time(GTimeVal *result)
{
    ESP_LOGV(TAG, "g_get_current_time entered");

    gint64 tv;

    assert(result != NULL);

    tv = g_get_real_time();

    result->tv_sec = tv / 1000000;
    result->tv_usec = tv % 1000000;
}

void g_usleep(gulong microseconds)
{
    ESP_LOGE(TAG, "g_usleep entered");
}

// https://developer.gnome.org/glib/stable/glib-Shell-related-Utilities.html

gboolean g_shell_parse_argv(const gchar *command_line, gint *argcp, gchar ***argvp, GError **error)
{
    ESP_LOGE(TAG, "g_shell_parse_argv entered");
    return 0;
}

// https://developer.gnome.org/glib/stable/glib-String-Utility-Functions.html

void g_strfreev(gchar **str_array)
{
    ESP_LOGE(TAG, "g_strfreev entered");
}
