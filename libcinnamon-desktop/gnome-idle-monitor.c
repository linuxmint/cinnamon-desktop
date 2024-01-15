/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.c
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <time.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-idle-monitor.h"
#include "meta-dbus-idle-monitor.h"

G_STATIC_ASSERT(sizeof(unsigned long) == sizeof(gpointer));

struct _GnomeIdleMonitorPrivate
{
    GCancellable        *cancellable;
    MetaDBusIdleMonitor *proxy;
    MetaDBusObjectManagerClient *om;
    int                  name_watch_id;
    GHashTable          *watches;
    GHashTable          *watches_by_upstream_id;
};

typedef struct
{
    int                       ref_count;
    gboolean                  dead;
    GnomeIdleMonitor         *monitor;
    guint             id;
    guint                     upstream_id;
    GnomeIdleMonitorWatchFunc callback;
    gpointer          user_data;
    GDestroyNotify        notify;
    guint64                   timeout_msec;
} GnomeIdleMonitorWatch;

static void gnome_idle_monitor_initable_iface_init (GInitableIface *iface);
static void gnome_idle_monitor_remove_watch_internal (GnomeIdleMonitor *monitor,
                              guint             id);

static void add_idle_watch (GnomeIdleMonitor *, GnomeIdleMonitorWatch *);
static void add_active_watch (GnomeIdleMonitor *, GnomeIdleMonitorWatch *);

G_DEFINE_TYPE_WITH_CODE (GnomeIdleMonitor, gnome_idle_monitor, G_TYPE_OBJECT,
             G_ADD_PRIVATE (GnomeIdleMonitor)
             G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                        gnome_idle_monitor_initable_iface_init))

#define IDLE_MONITOR_PATH "/org/cinnamon/Muffin/IdleMonitor/Core"

static void
on_watch_fired (MetaDBusIdleMonitor *proxy,
        guint                upstream_id,
        GnomeIdleMonitor    *monitor)
{
    GnomeIdleMonitorWatch *watch;

    watch = g_hash_table_lookup (monitor->priv->watches_by_upstream_id, GINT_TO_POINTER (upstream_id));
    if (!watch)
        return;

    g_object_ref (monitor);

    if (watch->callback) {
        watch->callback (watch->monitor,
                 watch->id,
                 watch->user_data);
    }

    if (watch->timeout_msec == 0)
        gnome_idle_monitor_remove_watch_internal (monitor, watch->id);

    g_object_unref (monitor);
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;
  g_atomic_int_inc (&serial);
  return serial;
}

static void
idle_monitor_watch_unref (GnomeIdleMonitorWatch *watch)
{
    watch->ref_count--;
    if (watch->ref_count)
        return;

    if (watch->notify != NULL)
        watch->notify (watch->user_data);


    if (watch->upstream_id != 0)
        g_hash_table_remove (watch->monitor->priv->watches_by_upstream_id,
                     GINT_TO_POINTER (watch->upstream_id));

    g_slice_free (GnomeIdleMonitorWatch, watch);
}

static GnomeIdleMonitorWatch *
idle_monitor_watch_ref (GnomeIdleMonitorWatch *watch)
{
    g_assert (watch->ref_count > 0);

    watch->ref_count++;
    return watch;
}

static void
idle_monitor_watch_destroy (GnomeIdleMonitorWatch *watch)
{
    watch->dead = TRUE;
    idle_monitor_watch_unref (watch);
}

static void
gnome_idle_monitor_dispose (GObject *object)
{
    GnomeIdleMonitor *monitor;

    monitor = GNOME_IDLE_MONITOR (object);

    if (monitor->priv->cancellable)
        g_cancellable_cancel (monitor->priv->cancellable);
    g_clear_object (&monitor->priv->cancellable);

    if (monitor->priv->name_watch_id) {
        g_bus_unwatch_name (monitor->priv->name_watch_id);
        monitor->priv->name_watch_id = 0;
    }

    g_clear_object (&monitor->priv->proxy);
    g_clear_object (&monitor->priv->om);
    g_clear_pointer (&monitor->priv->watches, g_hash_table_destroy);
    g_clear_pointer (&monitor->priv->watches_by_upstream_id, g_hash_table_destroy);

    G_OBJECT_CLASS (gnome_idle_monitor_parent_class)->dispose (object);
}

static void
add_known_watch (gpointer key,
         gpointer value,
         gpointer user_data)
{
    GnomeIdleMonitor *monitor = user_data;
    GnomeIdleMonitorWatch *watch = value;

    if (watch->timeout_msec == 0)
        add_active_watch (monitor, watch);
    else
        add_idle_watch (monitor, watch);
}

static void
connect_proxy (GDBusObject  *object,
           GnomeIdleMonitor *monitor)
{
    MetaDBusIdleMonitor *proxy;

    proxy = meta_dbus_object_get_idle_monitor (META_DBUS_OBJECT (object));
    if (!proxy) {
        g_critical ("Unable to get idle monitor from object at %s",
                g_dbus_object_get_object_path (object));
        return;
    }

    monitor->priv->proxy = proxy;
    g_signal_connect_object (proxy, "watch-fired", G_CALLBACK (on_watch_fired), monitor, 0);
    g_hash_table_foreach (monitor->priv->watches, add_known_watch, monitor);
}

static void
on_object_added (GDBusObjectManager *manager,
         GDBusObject        *object,
         gpointer        user_data)
{
    GnomeIdleMonitor *monitor = user_data;

    if (!g_str_equal (IDLE_MONITOR_PATH, g_dbus_object_get_object_path (object)))
        return;

    connect_proxy (object, monitor);

    g_signal_handlers_disconnect_by_func (manager, on_object_added, user_data);
}

static void
get_proxy (GnomeIdleMonitor *monitor)
{
    GDBusObject *object;

    object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (monitor->priv->om),
                           IDLE_MONITOR_PATH);
    if (object) {
        connect_proxy (object, monitor);
        g_object_unref (object);
        return;
    }

    g_signal_connect_object (monitor->priv->om, "object-added",
                 G_CALLBACK (on_object_added), monitor, 0);
}

static void
on_object_manager_ready (GObject    *source,
             GAsyncResult   *res,
             gpointer    user_data)
{
    GnomeIdleMonitor *monitor = user_data;
    GDBusObjectManager *om;
    GError *error = NULL;

    om = meta_dbus_object_manager_client_new_finish (res, &error);
    if (!om) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("Failed to acquire idle monitor object manager: %s", error->message);
        g_error_free (error);
        return;
    }

    monitor->priv->om = META_DBUS_OBJECT_MANAGER_CLIENT (om);
    get_proxy (monitor);
}

static void
on_name_appeared (GDBusConnection *connection,
          const char      *name,
          const char      *name_owner,
          gpointer         user_data)
{
    GnomeIdleMonitor *monitor = user_data;

    meta_dbus_object_manager_client_new (connection,
                         G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                         name_owner,
                         "/org/cinnamon/Muffin/IdleMonitor",
                         monitor->priv->cancellable,
                         on_object_manager_ready,
                         monitor);
}

static void
clear_watch (gpointer key,
         gpointer value,
         gpointer user_data)
{
    GnomeIdleMonitorWatch *watch = value;
    GnomeIdleMonitor *monitor = user_data;

    g_hash_table_remove (monitor->priv->watches_by_upstream_id, GINT_TO_POINTER (watch->upstream_id));
    watch->upstream_id = 0;
}

static void
on_name_vanished (GDBusConnection *connection,
          const char      *name,
          gpointer         user_data)
{
    GnomeIdleMonitor *monitor = user_data;

    g_hash_table_foreach (monitor->priv->watches, clear_watch, monitor);
    g_clear_object (&monitor->priv->proxy);
    g_clear_object (&monitor->priv->om);
}

static gboolean
gnome_idle_monitor_initable_init (GInitable     *initable,
                  GCancellable  *cancellable,
                  GError       **error)
{
    GnomeIdleMonitor *monitor;

    monitor = GNOME_IDLE_MONITOR (initable);

    monitor->priv->name_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                             "org.cinnamon.Muffin.IdleMonitor",
                             G_BUS_NAME_WATCHER_FLAGS_NONE,
                             on_name_appeared,
                             on_name_vanished,
                             monitor, NULL);

    return TRUE;
}

static void
gnome_idle_monitor_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_idle_monitor_initable_init;
}

static void
gnome_idle_monitor_class_init (GnomeIdleMonitorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gnome_idle_monitor_dispose;
}

static void
gnome_idle_monitor_init (GnomeIdleMonitor *monitor)
{
    monitor->priv = gnome_idle_monitor_get_instance_private (monitor);

    monitor->priv->watches = g_hash_table_new_full (NULL,
                            NULL,
                            NULL,
                            (GDestroyNotify)idle_monitor_watch_destroy);
    monitor->priv->watches_by_upstream_id = g_hash_table_new (NULL, NULL);

    monitor->priv->cancellable = g_cancellable_new ();
}

/**
 * gnome_idle_monitor_new:
 *
 * Returns: a new #GnomeIdleMonitor that tracks the server-global
 * idletime for all devices.
 */
GnomeIdleMonitor *
gnome_idle_monitor_new (void)
{
    return GNOME_IDLE_MONITOR (g_initable_new (GNOME_TYPE_IDLE_MONITOR, NULL, NULL, NULL));
}

static GnomeIdleMonitorWatch *
make_watch (GnomeIdleMonitor          *monitor,
        guint64                    timeout_msec,
        GnomeIdleMonitorWatchFunc  callback,
        gpointer                   user_data,
        GDestroyNotify             notify)
{
    GnomeIdleMonitorWatch *watch;

    watch = g_slice_new0 (GnomeIdleMonitorWatch);
    watch->ref_count = 1;
    watch->id = get_next_watch_serial ();
    watch->monitor = monitor;
    watch->callback = callback;
    watch->user_data = user_data;
    watch->notify = notify;
    watch->timeout_msec = timeout_msec;

    return watch;
}

static void
on_watch_added (GObject      *object,
        GAsyncResult *result,
        gpointer      user_data)
{
    GnomeIdleMonitorWatch *watch = user_data;
    GnomeIdleMonitor *monitor;
    GError *error;
    GVariant *res;

    error = NULL;
    res = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), result, &error);
    if (!res) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_error_free (error);
            idle_monitor_watch_unref (watch);
            return;
        }

        g_warning ("Failed to acquire idle monitor proxy: %s", error->message);
        g_error_free (error);
        idle_monitor_watch_unref (watch);
        return;
    }

    if (watch->dead) {
        idle_monitor_watch_unref (watch);
        return;
    }

    monitor = watch->monitor;
    g_variant_get (res, "(u)", &watch->upstream_id);
    g_variant_unref (res);

    g_hash_table_insert (monitor->priv->watches_by_upstream_id,
                 GINT_TO_POINTER (watch->upstream_id), watch);
    idle_monitor_watch_unref (watch);
}

static void
add_idle_watch (GnomeIdleMonitor      *monitor,
        GnomeIdleMonitorWatch *watch)
{
    meta_dbus_idle_monitor_call_add_idle_watch (monitor->priv->proxy,
                            watch->timeout_msec,
                            monitor->priv->cancellable,
                            on_watch_added, idle_monitor_watch_ref (watch));
}

static void
add_active_watch (GnomeIdleMonitor      *monitor,
          GnomeIdleMonitorWatch *watch)
{
    meta_dbus_idle_monitor_call_add_user_active_watch (monitor->priv->proxy,
                               monitor->priv->cancellable,
                               on_watch_added, idle_monitor_watch_ref (watch));
}

/**
 * gnome_idle_monitor_add_idle_watch:
 * @monitor: A #GnomeIdleMonitor
 * @interval_msec: The idletime interval, in milliseconds. It must be
 *     a strictly positive value (> 0).
 * @callback: (allow-none): The callback to call when the user has
 *     accumulated @interval_msec milliseconds of idle time.
 * @user_data: (allow-none): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Adds a watch for a specific idle time. The callback will be called
 * when the user has accumulated @interval_msec milliseconds of idle time.
 * This function will return an ID that can either be passed to
 * gnome_idle_monitor_remove_watch(), or can be used to tell idle time
 * watches apart if you have more than one.
 *
 * Also note that this function will only care about positive transitions
 * (user's idle time exceeding a certain time). If you want to know about
 * when the user has become active, use
 * gnome_idle_monitor_add_user_active_watch().
 */
guint
gnome_idle_monitor_add_idle_watch (GnomeIdleMonitor        *monitor,
                   guint64                  interval_msec,
                   GnomeIdleMonitorWatchFunc    callback,
                   gpointer         user_data,
                   GDestroyNotify       notify)
{
    GnomeIdleMonitorWatch *watch;

    g_return_val_if_fail (GNOME_IS_IDLE_MONITOR (monitor), 0);
    g_return_val_if_fail (interval_msec > 0, 0);

    watch = make_watch (monitor,
                interval_msec,
                callback,
                user_data,
                notify);

    g_hash_table_insert (monitor->priv->watches,
                 GUINT_TO_POINTER (watch->id),
                 watch);

    if (monitor->priv->proxy)
        add_idle_watch (monitor, watch);

    return watch->id;
}

/**
 * gnome_idle_monitor_add_user_active_watch:
 * @monitor: A #GnomeIdleMonitor
 * @callback: (allow-none): The callback to call when the user is
 *     active again.
 * @user_data: (allow-none): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Add a one-time watch to know when the user is active again.
 * Note that this watch is one-time and will de-activate after the
 * function is called, for efficiency purposes. It's most convenient
 * to call this when an idle watch, as added by
 * gnome_idle_monitor_add_idle_watch(), has triggered.
 */
guint
gnome_idle_monitor_add_user_active_watch (GnomeIdleMonitor          *monitor,
                      GnomeIdleMonitorWatchFunc  callback,
                      gpointer           user_data,
                      GDestroyNotify         notify)
{
    GnomeIdleMonitorWatch *watch;

    g_return_val_if_fail (GNOME_IS_IDLE_MONITOR (monitor), 0);

    watch = make_watch (monitor,
                0,
                callback,
                user_data,
                notify);

    g_hash_table_insert (monitor->priv->watches,
                 GUINT_TO_POINTER (watch->id),
                 watch);

    if (monitor->priv->proxy)
        add_active_watch (monitor, watch);

    return watch->id;
}

/**
 * gnome_idle_monitor_remove_watch:
 * @monitor: A #GnomeIdleMonitor
 * @id: A watch ID
 *
 * Removes an idle time watcher, previously added by
 * gnome_idle_monitor_add_idle_watch() or
 * gnome_idle_monitor_add_user_active_watch().
 */
void
gnome_idle_monitor_remove_watch (GnomeIdleMonitor *monitor,
                 guint         id)
{
    GnomeIdleMonitorWatch *watch;

    g_return_if_fail (GNOME_IS_IDLE_MONITOR (monitor));

    watch = g_hash_table_lookup (monitor->priv->watches, GINT_TO_POINTER (id));
    if (!watch)
        return;

    if (watch->upstream_id)
        meta_dbus_idle_monitor_call_remove_watch (monitor->priv->proxy,
                              watch->upstream_id,
                              NULL, NULL, NULL);

    gnome_idle_monitor_remove_watch_internal (monitor, id);
}

static void
gnome_idle_monitor_remove_watch_internal (GnomeIdleMonitor *monitor,
                      guint             id)
{
    g_hash_table_remove (monitor->priv->watches,
                 GUINT_TO_POINTER (id));
}

/**
 * gnome_idle_monitor_get_idletime:
 * @monitor: A #GnomeIdleMonitor
 *
 * Returns: The current idle time, in milliseconds
 */
guint64
gnome_idle_monitor_get_idletime (GnomeIdleMonitor *monitor)
{
    guint64 value;

    value = 0;
    if (monitor->priv->proxy)
        meta_dbus_idle_monitor_call_get_idletime_sync (monitor->priv->proxy, &value,
                                   NULL, NULL);

    return value;
}
