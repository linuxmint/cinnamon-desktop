/* gnome-bg.h - 

   Copyright 2007, Red Hat, Inc.

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

   Author: Soren Sandmann <sandmann@redhat.com>
*/

#ifndef __GNOME_BG_H__
#define __GNOME_BG_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeBG is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-bg.h
#endif

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gdesktop-enums.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <libgnome-desktop/gnome-bg-crossfade.h>

G_BEGIN_DECLS

#define GNOME_TYPE_BG            (gnome_bg_get_type ())
#define GNOME_BG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_BG, GnomeBG))
#define GNOME_BG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_BG, GnomeBGClass))
#define GNOME_IS_BG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_BG))
#define GNOME_IS_BG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_BG))
#define GNOME_BG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_BG, GnomeBGClass))

typedef struct _GnomeBG GnomeBG;
typedef struct _GnomeBGClass GnomeBGClass;

GType            gnome_bg_get_type              (void);
GnomeBG *        gnome_bg_new                   (void);
void             gnome_bg_load_from_preferences (GnomeBG               *bg,
						 GSettings             *settings);
void             gnome_bg_save_to_preferences   (GnomeBG               *bg,
						 GSettings             *settings);
/* Setters */
void             gnome_bg_set_filename          (GnomeBG               *bg,
						 const char            *filename);
void             gnome_bg_set_placement         (GnomeBG               *bg,
						 GDesktopBackgroundStyle placement);
void             gnome_bg_set_color             (GnomeBG               *bg,
						 GDesktopBackgroundShading type,
						 GdkColor              *primary,
						 GdkColor              *secondary);
void             gnome_bg_set_draw_background   (GnomeBG               *bg,
						 gboolean               draw_background);
/* Getters */
GDesktopBackgroundStyle gnome_bg_get_placement  (GnomeBG               *bg);
gboolean         gnome_bg_get_draw_background   (GnomeBG               *bg);
void		 gnome_bg_get_color             (GnomeBG               *bg,
						 GDesktopBackgroundShading *type,
						 GdkColor              *primary,
						 GdkColor              *secondary);
const gchar *    gnome_bg_get_filename          (GnomeBG               *bg);

/* Drawing and thumbnailing */
void             gnome_bg_draw                  (GnomeBG               *bg,
						 GdkPixbuf             *dest,
						 GdkScreen	       *screen,
                                                 gboolean               is_root);
cairo_surface_t *gnome_bg_create_surface        (GnomeBG               *bg,
						 GdkWindow             *window,
						 int                    width,
						 int                    height,
						 gboolean               root);
gboolean         gnome_bg_get_image_size        (GnomeBG               *bg,
						 GnomeDesktopThumbnailFactory *factory,
                                                 int                    best_width,
                                                 int                    best_height,
						 int                   *width,
						 int                   *height);
GdkPixbuf *      gnome_bg_create_thumbnail      (GnomeBG               *bg,
						 GnomeDesktopThumbnailFactory *factory,
						 GdkScreen             *screen,
						 int                    dest_width,
						 int                    dest_height);
gboolean         gnome_bg_is_dark               (GnomeBG               *bg,
                                                 int                    dest_width,
						 int                    dest_height);
gboolean         gnome_bg_has_multiple_sizes    (GnomeBG               *bg);
gboolean         gnome_bg_changes_with_time     (GnomeBG               *bg);
GdkPixbuf *      gnome_bg_create_frame_thumbnail (GnomeBG              *bg,
						 GnomeDesktopThumbnailFactory *factory,
						 GdkScreen             *screen,
						 int                    dest_width,
						 int                    dest_height,
						 int                    frame_num);

/* Set a surface as root - not a GnomeBG method. At some point
 * if we decide to stabilize the API then we may want to make
 * these object methods, drop gnome_bg_create_surface, etc.
 */
void             gnome_bg_set_surface_as_root   (GdkScreen             *screen,
						 cairo_surface_t       *surface);

GnomeBGCrossfade *gnome_bg_set_surface_as_root_with_crossfade (GdkScreen *screen,
                                                              cairo_surface_t *surface);
cairo_surface_t *gnome_bg_get_surface_from_root (GdkScreen *screen);

G_END_DECLS

#endif
