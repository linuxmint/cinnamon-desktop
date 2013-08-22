/* gnome-bg-crossfade.h - fade window background between two surfaces

   Copyright 2008, Red Hat, Inc.

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

   Author: Ray Strode <rstrode@redhat.com>
*/

#ifndef __GNOME_BG_CROSSFADE_H__
#define __GNOME_BG_CROSSFADE_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeBGCrossfade is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-bg-crossfade.h
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GNOME_TYPE_BG_CROSSFADE            (gnome_bg_crossfade_get_type ())
#define GNOME_BG_CROSSFADE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_BG_CROSSFADE, GnomeBGCrossfade))
#define GNOME_BG_CROSSFADE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_BG_CROSSFADE, GnomeBGCrossfadeClass))
#define GNOME_IS_BG_CROSSFADE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_BG_CROSSFADE))
#define GNOME_IS_BG_CROSSFADE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_BG_CROSSFADE))
#define GNOME_BG_CROSSFADE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_BG_CROSSFADE, GnomeBGCrossfadeClass))

typedef struct _GnomeBGCrossfadePrivate GnomeBGCrossfadePrivate;
typedef struct _GnomeBGCrossfade GnomeBGCrossfade;
typedef struct _GnomeBGCrossfadeClass GnomeBGCrossfadeClass;

struct _GnomeBGCrossfade
{
	GObject parent_object;

	GnomeBGCrossfadePrivate *priv;
};

struct _GnomeBGCrossfadeClass
{
	GObjectClass parent_class;

	void (* finished) (GnomeBGCrossfade *fade, GdkWindow *window);
};

GType             gnome_bg_crossfade_get_type              (void);
GnomeBGCrossfade *gnome_bg_crossfade_new (int width, int height);
gboolean          gnome_bg_crossfade_set_start_surface (GnomeBGCrossfade *fade,
                                                        cairo_surface_t *surface);
gboolean          gnome_bg_crossfade_set_end_surface (GnomeBGCrossfade *fade,
                                                      cairo_surface_t *surface);
void              gnome_bg_crossfade_start (GnomeBGCrossfade *fade,
                                            GdkWindow        *window);
gboolean          gnome_bg_crossfade_is_started (GnomeBGCrossfade *fade);
void              gnome_bg_crossfade_stop (GnomeBGCrossfade *fade);

G_END_DECLS

#endif
