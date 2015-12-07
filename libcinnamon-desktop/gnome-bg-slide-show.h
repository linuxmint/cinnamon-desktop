/* gnome-bg-slide_show.h - fade window background between two surfaces

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

#ifndef __GNOME_BG_SLIDE_SHOW_H__
#define __GNOME_BG_SLIDE_SHOW_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeBGSlideShow is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-bg-slide_show.h
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GNOME_BG_TYPE_SLIDE_SHOW            (gnome_bg_slide_show_get_type ())
#define GNOME_BG_SLIDE_SHOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_BG_TYPE_SLIDE_SHOW, GnomeBGSlideShow))
#define GNOME_BG_SLIDE_SHOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_BG_TYPE_SLIDE_SHOW, GnomeBGSlideShowClass))
#define GNOME_BG_IS_SLIDE_SHOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_BG_TYPE_SLIDE_SHOW))
#define GNOME_BG_IS_SLIDE_SHOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_BG_TYPE_SLIDE_SHOW))
#define GNOME_BG_SLIDE_SHOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_BG_TYPE_SLIDE_SHOW, GnomeBGSlideShowClass))

typedef struct _GnomeBGSlideShowPrivate GnomeBGSlideShowPrivate;
typedef struct _GnomeBGSlideShow GnomeBGSlideShow;
typedef struct _GnomeBGSlideShowClass GnomeBGSlideShowClass;

struct _GnomeBGSlideShow
{
    GObject parent_object;

    GnomeBGSlideShowPrivate *priv;
};

struct _GnomeBGSlideShowClass
{
    GObjectClass parent_class;
};

GType             gnome_bg_slide_show_get_type (void);
GnomeBGSlideShow *gnome_bg_slide_show_new (const char *filename);
gboolean          gnome_bg_slide_show_load (GnomeBGSlideShow  *self,
                                            GError           **error);
void              gnome_bg_slide_show_load_async (GnomeBGSlideShow    *self,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean          gnome_bg_slide_show_get_slide (GnomeBGSlideShow *self,
                                                 int               frame_number,
                                                 int               width,
                                                 int               height,
                                                 gdouble          *progress,
                                                 double           *duration,
                                                 gboolean         *is_fixed,
                                                 const char      **file1,
                                                 const char      **file2);

void              gnome_bg_slide_show_get_current_slide (GnomeBGSlideShow  *self,
                                                         int                width,
                                                         int                height,
                                                         gdouble           *progress,
                                                         double            *duration,
                                                         gboolean          *is_fixed,
                                                         const char       **file1,
                                                         const char       **file2);


double gnome_bg_slide_show_get_start_time (GnomeBGSlideShow *self);
double gnome_bg_slide_show_get_total_duration (GnomeBGSlideShow *self);
gboolean gnome_bg_slide_show_get_has_multiple_sizes (GnomeBGSlideShow *self);
int  gnome_bg_slide_show_get_num_slides (GnomeBGSlideShow *self);
G_END_DECLS

#endif
