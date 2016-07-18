/* gnome-tz-monitor.h - fade window background between two surfaces

   Copyright 2008, Red Hat, Inc.
   Copyright 2011, Red Hat, Inc.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Colin Walters <walters@verbum.org>
*/

#ifndef __GNOME_WALL_CLOCK_H__
#define __GNOME_WALL_CLOCK_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-wall-clock.h
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define GNOME_TYPE_WALL_CLOCK            (gnome_wall_clock_get_type ())
#define GNOME_WALL_CLOCK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_WALL_CLOCK, GnomeWallClock))
#define GNOME_WALL_CLOCK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_WALL_CLOCK, GnomeWallClockClass))
#define GNOME_IS_WALL_CLOCK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_WALL_CLOCK))
#define GNOME_IS_WALL_CLOCK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_WALL_CLOCK))
#define GNOME_WALL_CLOCK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_WALL_CLOCK, GnomeWallClockClass))

typedef struct _GnomeWallClockPrivate GnomeWallClockPrivate;
typedef struct _GnomeWallClock GnomeWallClock;
typedef struct _GnomeWallClockClass GnomeWallClockClass;

struct _GnomeWallClock
{
	GObject parent_object;

	GnomeWallClockPrivate *priv;
};

struct _GnomeWallClockClass
{
	GObjectClass parent_class;
};

GType             gnome_wall_clock_get_type      (void);

const char *      gnome_wall_clock_get_clock     (GnomeWallClock *clock);
GnomeWallClock *  gnome_wall_clock_new           (void);

G_END_DECLS

#endif
