// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "glib.h"

// https://developer.gnome.org/glib/stable/glib-Memory-Allocation.html

gpointer g_malloc (gsize n_bytes) { return 0; }

void g_free (gpointer mem) { }

// https://developer.gnome.org/glib/stable/glib-Threads.html

GThread * g_thread_try_new (const gchar *name, GThreadFunc func, gpointer data, GError **error) { return 0; }

gpointer g_thread_join (GThread *thread) { return 0; }

void g_thread_unref (GThread *thread) { }

void g_cond_broadcast (GCond *cond) { }

void g_cond_clear (GCond *cond) { }

void g_cond_init (GCond *cond) { }

void g_cond_signal (GCond *cond) { }

void g_cond_wait (GCond *cond, GMutex *mutex) { }

void g_mutex_clear (GMutex *mutex) { }

void g_mutex_init (GMutex *mutex) { }

void g_mutex_lock (GMutex *mutex) { }

void g_mutex_unlock (GMutex *mutex) { }

void g_rec_mutex_clear (GRecMutex *rec_mutex) { }

void g_rec_mutex_init (GRecMutex *rec_mutex) { }

void g_rec_mutex_lock (GRecMutex *rec_mutex) { }

void g_rec_mutex_unlock (GRecMutex *rec_mutex) { }

gpointer g_private_get (GPrivate *key) { return 0; }

void g_private_set (GPrivate *key, gpointer value) { }

// https://developer.gnome.org/glib/stable/glib-File-Utilities.html

gboolean g_file_test (const gchar *filename, GFileTest test) { return 0; }

int g_stat (const gchar *filename, GStatBuf *buf) { return 0; }

// https://developer.gnome.org/glib/stable/glib-Atomic-Operations.html

gboolean g_atomic_int_compare_and_exchange (volatile gint *atomic, gint oldval, gint newval) { return 0; }

gint g_atomic_int_exchange_and_add (volatile gint *atomic, gint val) { return 0; }

gint g_atomic_int_get (const volatile gint *atomic) { return 0; }

void g_atomic_int_set (volatile gint *atomic, gint newval) { }

void g_atomic_int_inc (gint *atomic) { }

// https://developer.gnome.org/glib/stable/glib-Error-Reporting.html

void g_clear_error (GError **err) { }

// https://developer.gnome.org/glib/stable/glib-Date-and-Time-Functions.html

void g_get_current_time (GTimeVal *result) { }

void g_usleep (gulong microseconds) { }

// https://developer.gnome.org/glib/stable/glib-Shell-related-Utilities.html

gboolean g_shell_parse_argv (const gchar *command_line, gint *argcp, gchar ***argvp, GError **error) { return 0; }

// https://developer.gnome.org/glib/stable/glib-String-Utility-Functions.html

void g_strfreev (gchar **str_array) { }
