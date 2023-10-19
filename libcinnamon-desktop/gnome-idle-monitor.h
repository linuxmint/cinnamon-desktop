/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.h
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
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 */

#ifndef __GNOME_IDLE_MONITOR_H__
#define __GNOME_IDLE_MONITOR_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-idle-monitor.h
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_IDLE_MONITOR         (gnome_idle_monitor_get_type ())
#define GNOME_IDLE_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_IDLE_MONITOR, GnomeIdleMonitor))
#define GNOME_IDLE_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_IDLE_MONITOR, GnomeIdleMonitorClass))
#define GNOME_IS_IDLE_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_IDLE_MONITOR))
#define GNOME_IS_IDLE_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_IDLE_MONITOR))
#define GNOME_IDLE_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_IDLE_MONITOR, GnomeIdleMonitorClass))

typedef struct _GnomeIdleMonitor GnomeIdleMonitor;
typedef struct _GnomeIdleMonitorClass GnomeIdleMonitorClass;
typedef struct _GnomeIdleMonitorPrivate GnomeIdleMonitorPrivate;

struct _GnomeIdleMonitor
{
        GObject                  parent;
        GnomeIdleMonitorPrivate *priv;
};

struct _GnomeIdleMonitorClass
{
        GObjectClass          parent_class;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GnomeIdleMonitor, g_object_unref)

typedef void (*GnomeIdleMonitorWatchFunc) (GnomeIdleMonitor      *monitor,
                                           guint                  id,
                                           gpointer               user_data);

GType              gnome_idle_monitor_get_type     (void);

GnomeIdleMonitor * gnome_idle_monitor_new          (void);

guint              gnome_idle_monitor_add_idle_watch    (GnomeIdleMonitor         *monitor,
                             guint64                   interval_msec,
                             GnomeIdleMonitorWatchFunc callback,
                             gpointer                  user_data,
                             GDestroyNotify            notify);

guint              gnome_idle_monitor_add_user_active_watch (GnomeIdleMonitor          *monitor,
                                 GnomeIdleMonitorWatchFunc  callback,
                                 gpointer            user_data,
                                 GDestroyNotify      notify);

void               gnome_idle_monitor_remove_watch (GnomeIdleMonitor         *monitor,
                                                    guint                     id);

guint64            gnome_idle_monitor_get_idletime (GnomeIdleMonitor         *monitor);

G_END_DECLS

#endif /* __GNOME_IDLE_MONITOR_H__ */
