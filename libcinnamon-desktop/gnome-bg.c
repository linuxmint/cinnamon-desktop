/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

gnomebg.c: Object for the desktop background.

Copyright (C) 2000 Eazel, Inc.
Copyright (C) 2007-2008 Red Hat, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this program; if not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.

Derived from eel-background.c and eel-gdk-pixbuf-extensions.c by
Darin Adler <darin@eazel.com> and Ramiro Estrugo <ramiro@eazel.com>

Author: Soren Sandmann <sandmann@redhat.com>

*/

#include "config.h"

#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include "cdesktop-enums.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cairo.h>
#include <cairo-xlib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-bg.h"
#include "gnome-bg-crossfade.h"

#define BG_KEY_PRIMARY_COLOR      "primary-color"
#define BG_KEY_SECONDARY_COLOR    "secondary-color"
#define BG_KEY_COLOR_TYPE         "color-shading-type"
#define BG_KEY_PICTURE_PLACEMENT  "picture-options"
#define BG_KEY_PICTURE_OPACITY    "picture-opacity"
#define BG_KEY_PICTURE_URI        "picture-uri"

/* We keep the large pixbufs around if the next update
   in the slideshow is less than 60 seconds away */
#define KEEP_EXPENSIVE_CACHE_SECS 60

typedef struct _SlideShow SlideShow;
typedef struct _Slide Slide;

struct _Slide
{
	double   duration;		/* in seconds */
	gboolean fixed;

	GSList  *file1;
	GSList  *file2;		/* NULL if fixed is TRUE */
};

typedef struct _FileSize FileSize;
struct _FileSize
{
	gint width;
	gint height;

	char *file;
};

/* This is the size of the GdkRGB dither matrix, in order to avoid
 * bad dithering when tiling the gradient
 */
#define GRADIENT_PIXMAP_TILE_SIZE 128
#define THUMBNAIL_SIZE 256

typedef struct FileCacheEntry FileCacheEntry;
#define CACHE_SIZE 4

/*
 *   Implementation of the GnomeBG class
 */
struct _GnomeBG
{
	GObject                 parent_instance;
	char *			filename;
	CDesktopBackgroundStyle	placement;
	CDesktopBackgroundShading	color_type;
	GdkColor		primary;
	GdkColor		secondary;

	GFileMonitor *		file_monitor;

	guint                   changed_id;
	guint                   transitioned_id;
	guint                   blow_caches_id;

	/* Cached information, only access through cache accessor functions */
        SlideShow *		slideshow;
	time_t			file_mtime;
	GdkPixbuf *		pixbuf_cache;
	int			timeout_id;

	GList *		        file_cache;
};

struct _GnomeBGClass
{
	GObjectClass parent_class;
};

enum {
	CHANGED,
	TRANSITIONED,
	N_SIGNALS
};

static const cairo_user_data_key_t average_color_key;

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GnomeBG, gnome_bg, G_TYPE_OBJECT)

static cairo_surface_t *make_root_pixmap     (GdkScreen  *screen,
                                              gint        width,
                                              gint        height);

/* Pixbuf utils */
static void       pixbuf_average_value (GdkPixbuf  *pixbuf,
                                        GdkRGBA    *result);
static GdkPixbuf *pixbuf_scale_to_fit  (GdkPixbuf  *src,
					int         max_width,
					int         max_height);
static GdkPixbuf *pixbuf_scale_to_min  (GdkPixbuf  *src,
					int         min_width,
					int         min_height);
static void       pixbuf_draw_gradient (GdkPixbuf    *pixbuf,
					gboolean      horizontal,
					GdkColor     *c1,
					GdkColor     *c2,
					GdkRectangle *rect);
static void       pixbuf_tile          (GdkPixbuf  *src,
					GdkPixbuf  *dest);
static void       pixbuf_blend         (GdkPixbuf  *src,
					GdkPixbuf  *dest,
					int         src_x,
					int         src_y,
					int         width,
					int         height,
					int         dest_x,
					int         dest_y,
					double      alpha);

/* Thumbnail utilities */
static GdkPixbuf *create_thumbnail_for_filename (GnomeDesktopThumbnailFactory *factory,
						 const char            *filename);
static gboolean   get_thumb_annotations (GdkPixbuf             *thumb,
					 int                   *orig_width,
					 int                   *orig_height);

/* Cache */
static GdkPixbuf *get_pixbuf_for_size  (GnomeBG               *bg,
					gint                   num_monitor,
					int                    width,
					int                    height);
static void       clear_cache          (GnomeBG               *bg);
static gboolean   is_different         (GnomeBG               *bg,
					const char            *filename);
static time_t     get_mtime            (const char            *filename);
static GdkPixbuf *create_img_thumbnail (GnomeBG               *bg,
					GnomeDesktopThumbnailFactory *factory,
					GdkScreen             *screen,
					int                    dest_width,
					int                    dest_height,
					int		       frame_num);
static SlideShow * get_as_slideshow    (GnomeBG               *bg,
					const char 	      *filename);
static Slide *     get_current_slide   (SlideShow 	      *show,
		   			double    	      *alpha);
static gboolean    slideshow_has_multiple_sizes (SlideShow *show);

static SlideShow *read_slideshow_file (const char *filename,
				       GError     **err);
static SlideShow *slideshow_ref       (SlideShow  *show);
static void       slideshow_unref     (SlideShow  *show);

static FileSize   *find_best_size      (GSList                *sizes,
					gint                   width,
					gint                   height);

static void
color_from_string (const char *string,
		   GdkColor   *colorp)
{
	/* If all else fails use black */
	gdk_color_parse ("black", colorp);

	if (!string)
		return;

	gdk_color_parse (string, colorp);
}

static char *
color_to_string (const GdkColor *color)
{
	return g_strdup_printf ("#%02x%02x%02x",
				color->red >> 8,
				color->green >> 8,
				color->blue >> 8);
}

static gboolean
do_changed (GnomeBG *bg)
{
	gboolean ignore_pending_change;
	bg->changed_id = 0;

	ignore_pending_change =
		GPOINTER_TO_INT (g_object_get_data (G_OBJECT (bg),
						    "ignore-pending-change"));

	if (!ignore_pending_change) {
		g_signal_emit (G_OBJECT (bg), signals[CHANGED], 0);
	}

	return FALSE;
}

static void
queue_changed (GnomeBG *bg)
{
	if (bg->changed_id > 0) {
		g_source_remove (bg->changed_id);
		bg->changed_id = 0;
	}

	/* We unset this here to allow apps to set it if they don't want
	   to get the change event. This is used by nautilus when it
	   gets the pixmap from the bg (due to a reason other than the changed
	   event). Because if there is no other change after this time the
	   pending changed event will just uselessly cause us to recreate
	   the pixmap. */
	g_object_set_data (G_OBJECT (bg), "ignore-pending-change",
			   GINT_TO_POINTER (FALSE));
	bg->changed_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_changed,
					     bg,
					     NULL);
}

static gboolean
do_transitioned (GnomeBG *bg)
{
	bg->transitioned_id = 0;

	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		bg->pixbuf_cache = NULL;
	}

	g_signal_emit (G_OBJECT (bg), signals[TRANSITIONED], 0);

	return FALSE;
}

static void
queue_transitioned (GnomeBG *bg)
{
	if (bg->transitioned_id > 0) {
		g_source_remove (bg->transitioned_id);
		bg->transitioned_id = 0;
	}

	bg->transitioned_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_transitioned,
					     bg,
					     NULL);
}

static gboolean 
bg_gsettings_mapping (GVariant *value,
			gpointer *result,
			gpointer user_data)
{
	const gchar *bg_key_value;
	char *filename = NULL;

	/* The final fallback if nothing matches is with a NULL value. */
	if (value == NULL) {
		*result = NULL;
		return TRUE;
	}

	bg_key_value = g_variant_get_string (value, NULL);

	if (bg_key_value && *bg_key_value != '\0') {
		filename = g_filename_from_uri (bg_key_value, NULL, NULL);

		if (filename != NULL && g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE) {
			g_free (filename);
			return FALSE;
		}

		if (filename != NULL) {
			*result = filename;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
set_user_bg_with_display_manager (const gchar *obj_path,
                                  const gchar *bg_path)
{
  GDBusProxy *props;
  GVariant *ret;
  GError *error;

  error = NULL;

  props = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.freedesktop.Accounts",
                                         obj_path,
                                         "org.freedesktop.DBus.Properties",
                                         NULL,
                                         &error);

  if (error != NULL) {
    g_debug ("Could not create proxy for Accounts properties: '%s': %s\n",
             obj_path,
             error->message);

    g_clear_error (&error);

    return FALSE;
  }

  ret = g_dbus_proxy_call_sync (props,
                                "Set",
                                g_variant_new ("(ssv)",
                                               "org.freedesktop.DisplayManager.AccountsService",
                                               "BackgroundFile",
                                               g_variant_new_string (bg_path ? bg_path : "")),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

  g_clear_object (&props);

  if (error != NULL) {
    g_debug ("Failed to set the background for '%s' -> %s: %s",
             obj_path,
             bg_path,
             error->message);

    g_clear_error (&error);

    return FALSE;
  }

  g_variant_unref (ret);

  g_debug ("Background set via org.freedesktop.DisplayManager.AccountsService BackgroundFile");

  return TRUE;
}

static void
set_user_bg_with_accounts_service (const gchar *obj_path,
                                   const gchar *bg_path)
{
  GDBusProxy *user;
  GError *error;
  GVariant *ret;

  error = NULL;

  user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "org.freedesktop.Accounts",
                                        obj_path,
                                        "org.freedesktop.Accounts.User",
                                        NULL,
                                        &error);

  if (user == NULL) {
    g_debug ("Could not create User proxy for user '%s': %s",
             g_get_user_name (),
             error->message);

    g_clear_error (&error);
    return;
  }

  ret = g_dbus_proxy_call_sync (user,
                                "SetBackgroundFile",
                                g_variant_new ("(s)", bg_path ? bg_path : ""),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

  g_clear_object (&user);

  if (error != NULL) {
    g_debug ("Failed to set the background for '%s' -> %s': %s",
             obj_path,
             bg_path,
             error->message);

    g_clear_error (&error);
    return;
  }

  g_variant_unref (ret);

  g_debug ("Background set via org.freedesktop.AccountsService.User SetBackgroundFile");
}

void
gnome_bg_set_accountsservice_background (const gchar *background)
{
  GDBusProxy *proxy;
  GVariant *user_var;
  GError *error;
  gchar *object_path;

  g_debug ("Setting user AccountsService background: %s", background);

  error = NULL;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.freedesktop.Accounts",
                                         "/org/freedesktop/Accounts",
                                         "org.freedesktop.Accounts",
                                         NULL,
                                         &error);

  if (error != NULL) {
    g_debug ("Failed to contact accounts service: %s",
             error->message);

    g_clear_error (&error);
    return;
  }

  user_var = g_dbus_proxy_call_sync (proxy,
                                     "FindUserByName",
                                     g_variant_new ("(s)", g_get_user_name ()),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);

  g_clear_object (&proxy);

  if (error != NULL) {
    g_debug ("Could not contact org.freedesktop.Accounts service to look up '%s': %s",
             g_get_user_name (),
             error->message);

    g_clear_error (&error);
    return;
  }

  object_path = NULL;
  g_variant_get (user_var, "(o)", &object_path);
  g_variant_unref (user_var);

  if (!set_user_bg_with_display_manager (object_path, background)) {
    g_debug ("Could not set background via org.freedesktop.DisplayManager.AccountsService, "
             "trying org.freedesktop.Accounts.User");

    set_user_bg_with_accounts_service (object_path, background);
  }

  g_free (object_path);
}

void
gnome_bg_load_from_preferences (GnomeBG   *bg,
				GSettings *settings)
{
	char    *tmp;
	char    *filename;
	CDesktopBackgroundShading ctype;
	GdkColor c1, c2;
	CDesktopBackgroundStyle placement;

	g_return_if_fail (GNOME_IS_BG (bg));
	g_return_if_fail (G_IS_SETTINGS (settings));

	/* Filename */
	filename = g_settings_get_mapped (settings, BG_KEY_PICTURE_URI, bg_gsettings_mapping, NULL);

	/* Colors */
	tmp = g_settings_get_string (settings, BG_KEY_PRIMARY_COLOR);
	color_from_string (tmp, &c1);
	g_free (tmp);

	tmp = g_settings_get_string (settings, BG_KEY_SECONDARY_COLOR);
	color_from_string (tmp, &c2);
	g_free (tmp);

	/* Color type */
	ctype = g_settings_get_enum (settings, BG_KEY_COLOR_TYPE);

	/* Placement */
	placement = g_settings_get_enum (settings, BG_KEY_PICTURE_PLACEMENT);

	gnome_bg_set_color (bg, ctype, &c1, &c2);
	gnome_bg_set_placement (bg, placement);
	gnome_bg_set_filename (bg, filename);

	g_free (filename);
}

void
gnome_bg_save_to_preferences (GnomeBG   *bg,
			      GSettings *settings)
{
	gchar *primary;
	gchar *secondary;
	gchar *uri;

	g_return_if_fail (GNOME_IS_BG (bg));
	g_return_if_fail (G_IS_SETTINGS (settings));

	primary = color_to_string (&bg->primary);
	secondary = color_to_string (&bg->secondary);

	g_settings_delay (settings);

	uri = NULL;
	if (bg->filename != NULL)
		uri = g_filename_to_uri (bg->filename, NULL, NULL);
	if (uri == NULL)
		uri = g_strdup ("");
	g_settings_set_string (settings, BG_KEY_PICTURE_URI, uri);
	g_settings_set_string (settings, BG_KEY_PRIMARY_COLOR, primary);
	g_settings_set_string (settings, BG_KEY_SECONDARY_COLOR, secondary);
	g_settings_set_enum (settings, BG_KEY_COLOR_TYPE, bg->color_type);
	g_settings_set_enum (settings, BG_KEY_PICTURE_PLACEMENT, bg->placement);

	/* Apply changes atomically. */
	g_settings_apply (settings);

	g_free (primary);
	g_free (secondary);
	g_free (uri);
}


static void
gnome_bg_init (GnomeBG *bg)
{
}

static void
gnome_bg_dispose (GObject *object)
{
	GnomeBG *bg = GNOME_BG (object);

	if (bg->file_monitor) {
		g_object_unref (bg->file_monitor);
		bg->file_monitor = NULL;
	}

	clear_cache (bg);

	G_OBJECT_CLASS (gnome_bg_parent_class)->dispose (object);
}

static void
gnome_bg_finalize (GObject *object)
{
	GnomeBG *bg = GNOME_BG (object);

	if (bg->changed_id != 0) {
		g_source_remove (bg->changed_id);
		bg->changed_id = 0;
	}

	if (bg->transitioned_id != 0) {
		g_source_remove (bg->transitioned_id);
		bg->transitioned_id = 0;
	}
	
	if (bg->blow_caches_id != 0) {
		g_source_remove (bg->blow_caches_id);
		bg->blow_caches_id = 0;
	}
	
	g_free (bg->filename);
	bg->filename = NULL;

	G_OBJECT_CLASS (gnome_bg_parent_class)->finalize (object);
}

static void
gnome_bg_class_init (GnomeBGClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gnome_bg_dispose;
	object_class->finalize = gnome_bg_finalize;

	signals[CHANGED] = g_signal_new ("changed",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

	signals[TRANSITIONED] = g_signal_new ("transitioned",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
}

GnomeBG *
gnome_bg_new (void)
{
	return g_object_new (GNOME_TYPE_BG, NULL);
}

void
gnome_bg_set_color (GnomeBG *bg,
		    CDesktopBackgroundShading type,
		    GdkColor *primary,
		    GdkColor *secondary)
{
	g_return_if_fail (bg != NULL);
	g_return_if_fail (primary != NULL);

	if (bg->color_type != type			||
	    !gdk_color_equal (&bg->primary, primary)			||
	    (secondary && !gdk_color_equal (&bg->secondary, secondary))) {

		bg->color_type = type;
		bg->primary = *primary;
		if (secondary) {
			bg->secondary = *secondary;
		}

		queue_changed (bg);
	}
}

void
gnome_bg_set_placement (GnomeBG                 *bg,
			CDesktopBackgroundStyle  placement)
{
	g_return_if_fail (bg != NULL);
	
	if (bg->placement != placement) {
		bg->placement = placement;
		
		queue_changed (bg);
	}
}

CDesktopBackgroundStyle
gnome_bg_get_placement (GnomeBG *bg)
{
	g_return_val_if_fail (bg != NULL, -1);

	return bg->placement;
}

void
gnome_bg_get_color (GnomeBG                   *bg,
		    CDesktopBackgroundShading *type,
		    GdkColor                  *primary,
		    GdkColor                  *secondary)
{
	g_return_if_fail (bg != NULL);

	if (type)
		*type = bg->color_type;

	if (primary)
		*primary = bg->primary;

	if (secondary)
		*secondary = bg->secondary;
}

const gchar *
gnome_bg_get_filename (GnomeBG *bg)
{
	g_return_val_if_fail (bg != NULL, NULL);

	return bg->filename;
}

static inline gchar *
get_wallpaper_cache_dir (void)
{
	return g_build_filename (g_get_user_cache_dir(), "wallpaper", NULL);
}

static inline gchar *
get_wallpaper_cache_prefix_name (gint                     num_monitor,
				 CDesktopBackgroundStyle  placement,
				 gint                     width,
				 gint                     height)
{
	return g_strdup_printf ("%i_%i_%i_%i", num_monitor, (gint) placement, width, height);
}

static char *
get_wallpaper_cache_filename (const char              *filename,
			      gint                     num_monitor,
			      CDesktopBackgroundStyle  placement,
			      gint                     width,
			      gint                     height)
{
	gchar *cache_filename;
	gchar *cache_prefix_name;
	gchar *md5_filename;
	gchar *cache_basename;
	gchar *cache_dir;

	md5_filename = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) filename, strlen (filename));
	cache_prefix_name = get_wallpaper_cache_prefix_name (num_monitor, placement, width, height);
	cache_basename = g_strdup_printf ("%s_%s", cache_prefix_name, md5_filename);
	cache_dir = get_wallpaper_cache_dir ();
	cache_filename = g_build_filename (cache_dir, cache_basename, NULL);

	g_free (cache_prefix_name);
	g_free (md5_filename);
	g_free (cache_basename);
	g_free (cache_dir);

	return cache_filename;
}

static void
cleanup_cache_for_monitor (gchar *cache_dir,
			   gint   num_monitor)
{
	GDir            *g_cache_dir;
	gchar           *monitor_prefix;
	const gchar     *file;

	g_cache_dir = g_dir_open (cache_dir, 0, NULL);
	monitor_prefix = g_strdup_printf ("%i_", num_monitor);

	file = g_dir_read_name (g_cache_dir);
	while (file != NULL) {
		gchar *path;

		path = g_build_filename (cache_dir, file, NULL);
		/* purge files with same monitor id */
		if (g_str_has_prefix (file, monitor_prefix) &&
		    g_file_test (path, G_FILE_TEST_IS_REGULAR))
			g_unlink (path);

		g_free (path);

		file = g_dir_read_name (g_cache_dir);
	}

	g_free (monitor_prefix);
	g_dir_close (g_cache_dir);
}

static gboolean
cache_file_is_valid (const char *filename,
		     const char *cache_filename)
{
	time_t mtime;
	time_t cache_mtime;

	if (!g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR))
		return FALSE;

	mtime = get_mtime (filename);
	cache_mtime = get_mtime (cache_filename);

	return (mtime < cache_mtime);
}

static void
refresh_cache_file (GnomeBG     *bg,
		    GdkPixbuf   *new_pixbuf,
		    gint         num_monitor,
		    gint         width,
		    gint         height)
{
	gchar           *cache_filename;
	gchar           *cache_dir;
	GdkPixbufFormat *format;
	gchar           *format_name;

	if ((num_monitor == -1) || (width <= 300) || (height <= 300))
		return;

	cache_filename = get_wallpaper_cache_filename (bg->filename, num_monitor, bg->placement, width, height);
	cache_dir = get_wallpaper_cache_dir ();

	/* Only refresh scaled file on disk if useful (and don't cache slideshow) */
	if (!cache_file_is_valid (bg->filename, cache_filename)) {
		format = gdk_pixbuf_get_file_info (bg->filename, NULL, NULL);

		if (format != NULL) {
			if (!g_file_test (cache_dir, G_FILE_TEST_IS_DIR)) {
				g_mkdir_with_parents (cache_dir, 0700);
			} else {
				cleanup_cache_for_monitor (cache_dir, num_monitor);
			}

			format_name = gdk_pixbuf_format_get_name (format);

			if (strcmp (format_name, "jpeg") == 0)
				gdk_pixbuf_save (new_pixbuf, cache_filename, format_name, NULL, "quality", "100", NULL);
			else
				gdk_pixbuf_save (new_pixbuf, cache_filename, format_name, NULL, NULL);

			g_free (format_name);
		}
	}

	g_free (cache_filename);
	g_free (cache_dir);
}

static void
file_changed (GFileMonitor *file_monitor,
	      GFile *child,
	      GFile *other_file,
	      GFileMonitorEvent event_type,
	      gpointer user_data)
{
	GnomeBG *bg = GNOME_BG (user_data);

	clear_cache (bg);
	queue_changed (bg);
}

void
gnome_bg_set_filename (GnomeBG     *bg,
		       const char  *filename)
{
	g_return_if_fail (bg != NULL);
	
	if (is_different (bg, filename)) {
		g_free (bg->filename);
		
		bg->filename = g_strdup (filename);
		bg->file_mtime = get_mtime (bg->filename);

		if (bg->file_monitor) {
			g_object_unref (bg->file_monitor);
			bg->file_monitor = NULL;
		}

		if (bg->filename) {
			GFile *f = g_file_new_for_path (bg->filename);
			
			bg->file_monitor = g_file_monitor_file (f, 0, NULL, NULL);
			g_signal_connect (bg->file_monitor, "changed",
					  G_CALLBACK (file_changed), bg);

			g_object_unref (f);
		}
		
		clear_cache (bg);
		
		queue_changed (bg);
	}
}

static void
draw_color_area (GnomeBG *bg,
		 GdkPixbuf *dest,
		 GdkRectangle *rect)
{
	guint32 pixel;
        GdkRectangle extent;

        extent.x = 0;
        extent.y = 0;
        extent.width = gdk_pixbuf_get_width (dest);
        extent.height = gdk_pixbuf_get_height (dest);

        gdk_rectangle_intersect (rect, &extent, rect);
	
	switch (bg->color_type) {
	case C_DESKTOP_BACKGROUND_SHADING_SOLID:
		/* not really a big deal to ignore the area of interest */
		pixel = ((bg->primary.red >> 8) << 24)      |
			((bg->primary.green >> 8) << 16)    |
			((bg->primary.blue >> 8) << 8)      |
			(0xff);
		
		gdk_pixbuf_fill (dest, pixel);
		break;
		
	case C_DESKTOP_BACKGROUND_SHADING_HORIZONTAL:
		pixbuf_draw_gradient (dest, TRUE, &(bg->primary), &(bg->secondary), rect);
		break;
		
	case C_DESKTOP_BACKGROUND_SHADING_VERTICAL:
		pixbuf_draw_gradient (dest, FALSE, &(bg->primary), &(bg->secondary), rect);
		break;
		
	default:
		break;
	}
}

static void
draw_color (GnomeBG *bg,
	    GdkPixbuf *dest)
{
	GdkRectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_pixbuf_get_width (dest);
	rect.height = gdk_pixbuf_get_height (dest);
	draw_color_area (bg, dest, &rect);
}

static void
draw_color_each_monitor (GnomeBG *bg,
			 GdkPixbuf *dest,
			 GdkScreen *screen)
{
	GdkRectangle rect;
	gint num_monitors;
	int monitor;

	num_monitors = gdk_screen_get_n_monitors (screen);
	for (monitor = 0; monitor < num_monitors; monitor++) {
		gdk_screen_get_monitor_geometry (screen, monitor, &rect);
		draw_color_area (bg, dest, &rect);
	}
}

static GdkPixbuf *
pixbuf_clip_to_fit (GdkPixbuf *src,
		    int        max_width,
		    int        max_height)
{
	int src_width, src_height;
	int w, h;
	int src_x, src_y;
	GdkPixbuf *pixbuf;

	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);

	if (src_width < max_width && src_height < max_height)
		return g_object_ref (src);

	w = MIN(src_width, max_width);
	h = MIN(src_height, max_height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 gdk_pixbuf_get_has_alpha (src),
				 8, w, h);

	src_x = (src_width - w) / 2;
	src_y = (src_height - h) / 2;
	gdk_pixbuf_copy_area (src,
			      src_x, src_y,
			      w, h,
			      pixbuf,
			      0, 0);
	return pixbuf;
}

static GdkPixbuf *
get_scaled_pixbuf (CDesktopBackgroundStyle placement,
		   GdkPixbuf *pixbuf,
		   int width, int height,
		   int *x, int *y,
		   int *w, int *h)
{
	GdkPixbuf *new;

#if 0
	g_print ("original_width: %d %d\n",
		 gdk_pixbuf_get_width (pixbuf),
		 gdk_pixbuf_get_height (pixbuf));
#endif
	
	switch (placement) {
	case C_DESKTOP_BACKGROUND_STYLE_SPANNED:
                new = pixbuf_scale_to_fit (pixbuf, width, height);
		break;
	case C_DESKTOP_BACKGROUND_STYLE_ZOOM:
		new = pixbuf_scale_to_min (pixbuf, width, height);
		break;
		
	case C_DESKTOP_BACKGROUND_STYLE_STRETCHED:
		new = gdk_pixbuf_scale_simple (pixbuf, width, height,
					       GDK_INTERP_BILINEAR);
		break;
		
	case C_DESKTOP_BACKGROUND_STYLE_SCALED:
		new = pixbuf_scale_to_fit (pixbuf, width, height);
		break;
		
	case C_DESKTOP_BACKGROUND_STYLE_CENTERED:
	case C_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
	default:
		new = pixbuf_clip_to_fit (pixbuf, width, height);
		break;
	}
	
	*w = gdk_pixbuf_get_width (new);
	*h = gdk_pixbuf_get_height (new);
	*x = (width - *w) / 2;
	*y = (height - *h) / 2;
	
	return new;
}

static void
draw_image_area (GnomeBG         *bg,
		 gint             num_monitor,
		 GdkPixbuf       *pixbuf,
		 GdkPixbuf       *dest,
		 GdkRectangle    *area)
{
	int dest_width = area->width;
	int dest_height = area->height;
	int x, y, w, h;
	GdkPixbuf *scaled;

	if (!pixbuf)
		return;

	scaled = get_scaled_pixbuf (bg->placement, pixbuf, dest_width, dest_height, &x, &y, &w, &h);

	switch (bg->placement) {
	case C_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
		pixbuf_tile (scaled, dest);
		break;
	case C_DESKTOP_BACKGROUND_STYLE_ZOOM:
	case C_DESKTOP_BACKGROUND_STYLE_CENTERED:
	case C_DESKTOP_BACKGROUND_STYLE_STRETCHED:
	case C_DESKTOP_BACKGROUND_STYLE_SCALED:
		pixbuf_blend (scaled, dest, 0, 0, w, h, x + area->x, y + area->y, 1.0);
		break;
	case C_DESKTOP_BACKGROUND_STYLE_SPANNED:
		pixbuf_blend (scaled, dest, 0, 0, w, h, x, y, 1.0);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	refresh_cache_file (bg, scaled, num_monitor, dest_width, dest_height);

	g_object_unref (scaled);
}

static void
draw_image_for_thumb (GnomeBG       *bg,
	    GdkPixbuf               *pixbuf,
	    GdkPixbuf               *dest)
{
	GdkRectangle rect;

	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_pixbuf_get_width (dest);
	rect.height = gdk_pixbuf_get_height (dest);

	draw_image_area (bg, -1, pixbuf, dest, &rect);
}

static void
draw_once (GnomeBG   *bg,
	   GdkPixbuf *dest)
{
	GdkRectangle rect;
	GdkPixbuf   *pixbuf;
	gint         num_monitor;

	/* we just draw on the whole screen */
	num_monitor = 0;

	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_pixbuf_get_width (dest);
	rect.height = gdk_pixbuf_get_height (dest);

	pixbuf = get_pixbuf_for_size (bg, num_monitor, rect.width, rect.height);
	if (pixbuf) {
		draw_image_area (bg,
				 num_monitor,
				 pixbuf,
				 dest,
				 &rect);
		g_object_unref (pixbuf);
	}
}

static void
draw_each_monitor (GnomeBG   *bg,
		   GdkPixbuf *dest,
		   GdkScreen *screen)
{
	GdkRectangle rect;
	gint num_monitors;
	int monitor;

	num_monitors = gdk_screen_get_n_monitors (screen);
	for (monitor = 0; monitor < num_monitors; monitor++) {
		GdkPixbuf *pixbuf;
		gdk_screen_get_monitor_geometry (screen, monitor, &rect);
		pixbuf = get_pixbuf_for_size (bg, monitor, rect.width, rect.height);
		if (pixbuf) {
			draw_image_area (bg,
					 monitor,
					 pixbuf,
					 dest, &rect);
			g_object_unref (pixbuf);
		}
	}
}

void
gnome_bg_draw (GnomeBG *bg,
	       GdkPixbuf *dest,
	       GdkScreen *screen,
	       gboolean is_root)
{
	if (!bg)
		return;

	if (is_root && (bg->placement != C_DESKTOP_BACKGROUND_STYLE_SPANNED)) {
		draw_color_each_monitor (bg, dest, screen);
		if (bg->placement != C_DESKTOP_BACKGROUND_STYLE_NONE) {
			draw_each_monitor (bg, dest, screen);
		}
	} else {
		draw_color (bg, dest);
		if (bg->placement != C_DESKTOP_BACKGROUND_STYLE_NONE) {
			draw_once (bg, dest);
		}
	}
}

gboolean
gnome_bg_has_multiple_sizes (GnomeBG *bg)
{
	SlideShow *show;
	gboolean ret;

	g_return_val_if_fail (bg != NULL, FALSE);

	ret = FALSE;

	show = get_as_slideshow (bg, bg->filename);
	if (show) {
		ret = slideshow_has_multiple_sizes (show);
		slideshow_unref (show);
	}

	return ret;
}

static void
gnome_bg_get_pixmap_size (GnomeBG   *bg,
			  int        width,
			  int        height,
			  int       *pixmap_width,
			  int       *pixmap_height)
{
	int dummy;
	
	if (!pixmap_width)
		pixmap_width = &dummy;
	if (!pixmap_height)
		pixmap_height = &dummy;
	
	*pixmap_width = width;
	*pixmap_height = height;

	if (!bg->filename) {
		switch (bg->color_type) {
		case C_DESKTOP_BACKGROUND_SHADING_SOLID:
			*pixmap_width = 1;
			*pixmap_height = 1;
			break;
			
		case C_DESKTOP_BACKGROUND_SHADING_HORIZONTAL:
		case C_DESKTOP_BACKGROUND_SHADING_VERTICAL:
			break;
		}
		
		return;
	}
}

/**
 * gnome_bg_create_surface:
 * @bg: GnomeBG 
 * @window: 
 * @width: 
 * @height:
 * @root:
 *
 * Create a surface that can be set as background for @window. If @is_root is
 * TRUE, the surface created will be created by a temporary X server connection
 * so that if someone calls XKillClient on it, it won't affect the application
 * who created it.
 *
 * Returns: %NULL on error (e.g. out of X connections)
 **/
cairo_surface_t *
gnome_bg_create_surface (GnomeBG	    *bg,
		 	 GdkWindow   *window,
			 int	     width,
			 int	     height,
			 gboolean     root)
{
	int pm_width, pm_height;
	cairo_surface_t *surface;
	GdkRGBA average;
	cairo_t *cr;
	
	g_return_val_if_fail (bg != NULL, NULL);
	g_return_val_if_fail (window != NULL, NULL);

        if (bg->pixbuf_cache &&
            gdk_pixbuf_get_width (bg->pixbuf_cache) != width &&
            gdk_pixbuf_get_height (bg->pixbuf_cache) != height) {
                g_object_unref (bg->pixbuf_cache);
                bg->pixbuf_cache = NULL;
        }

	/* has the side effect of loading and caching pixbuf only when in tile mode */
	gnome_bg_get_pixmap_size (bg, width, height, &pm_width, &pm_height);

	if (root) {
		surface = make_root_pixmap (gdk_window_get_screen (window),
					   pm_width, pm_height);
	}
	else {
		surface = gdk_window_create_similar_image_surface (window,
                                                           CAIRO_FORMAT_ARGB32,
                                                           pm_width, pm_height, 0);
	}

	if (surface == NULL)
		return NULL;

	cr = cairo_create (surface);
	if (!bg->filename && bg->color_type == C_DESKTOP_BACKGROUND_SHADING_SOLID) {
		gdk_cairo_set_source_color (cr, &(bg->primary));
		average.red = bg->primary.red / 65535.0;
		average.green = bg->primary.green / 65535.0;
		average.blue = bg->primary.blue / 65535.0;
		average.alpha = 1.0;
	}
	else {
		GdkPixbuf *pixbuf;
		
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
					 width, height);
		gnome_bg_draw (bg, pixbuf, gdk_window_get_screen (window), root);
		gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
		pixbuf_average_value (pixbuf, &average);
		g_object_unref (pixbuf);
	}

	cairo_paint (cr);
	
	cairo_destroy (cr);

	cairo_surface_set_user_data (surface, &average_color_key,
	                             gdk_rgba_copy (&average),
	                             (cairo_destroy_func_t) gdk_rgba_free);

	return surface;
}


/* determine if a background is darker or lighter than average, to help
 * clients know what colors to draw on top with
 */
gboolean
gnome_bg_is_dark (GnomeBG *bg,
		  int      width,
		  int      height)
{
	GdkColor color;
	int intensity;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (bg != NULL, FALSE);
	
	if (bg->color_type == C_DESKTOP_BACKGROUND_SHADING_SOLID) {
		color = bg->primary;
	} else {
		color.red = (bg->primary.red + bg->secondary.red) / 2;
		color.green = (bg->primary.green + bg->secondary.green) / 2;
		color.blue = (bg->primary.blue + bg->secondary.blue) / 2;
	}
	pixbuf = get_pixbuf_for_size (bg, -1, width, height);
	if (pixbuf) {
		GdkRGBA argb;
		guchar a, r, g, b;

		pixbuf_average_value (pixbuf, &argb);
		a = argb.alpha * 0xff;
		r = argb.red * 0xff;
		g = argb.green * 0xff;
		b = argb.blue * 0xff;
		
		color.red = (color.red * (0xFF - a) + r * 0x101 * a) / 0xFF;
		color.green = (color.green * (0xFF - a) + g * 0x101 * a) / 0xFF;
		color.blue = (color.blue * (0xFF - a) + b * 0x101 * a) / 0xFF;
		g_object_unref (pixbuf);
	}
	
	intensity = (color.red * 77 +
		     color.green * 150 +
		     color.blue * 28) >> 16;
	
	return intensity < 160; /* biased slightly to be dark */
}

/* 
 * Create a persistent pixmap. We create a separate display
 * and set the closedown mode on it to RetainPermanent.
 */
static cairo_surface_t *
make_root_pixmap (GdkScreen *screen, gint width, gint height)
{
	Display *display;
        const char *display_name;
	Pixmap result;
        cairo_surface_t *surface;
	int screen_num;
	int depth;
	
	screen_num = gdk_screen_get_number (screen);
	
	gdk_flush ();
	
	display_name = gdk_display_get_name (gdk_screen_get_display (screen));
	display = XOpenDisplay (display_name);
	
        if (display == NULL) {
                g_warning ("Unable to open display '%s' when setting "
			   "background pixmap\n",
                           (display_name) ? display_name : "NULL");
                return NULL;
        }
	
	/* Desktop background pixmap should be created from 
	 * dummy X client since most applications will try to
	 * kill it with XKillClient later when changing pixmap
	 */
	
	XSetCloseDownMode (display, RetainPermanent);
	
	depth = DefaultDepth (display, screen_num);

	result = XCreatePixmap (display,
				RootWindow (display, screen_num),
				width, height, depth);
	
	XCloseDisplay (display);
	
	surface = cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (screen),
                                             result,
                                             GDK_VISUAL_XVISUAL (gdk_screen_get_system_visual (screen)),
					     width, height);

	return surface;
}

static gboolean
get_original_size (const char *filename,
		   int        *orig_width,
		   int        *orig_height)
{
	gboolean result;

        if (gdk_pixbuf_get_file_info (filename, orig_width, orig_height))
		result = TRUE;
	else
		result = FALSE;

	return result;
}

static const char *
get_filename_for_size (GnomeBG *bg, gint best_width, gint best_height)
{
	SlideShow *show;
	Slide *slide;
	FileSize *size;

	if (!bg->filename)
		return NULL;

	show = get_as_slideshow (bg, bg->filename);
	if (!show) {
		return bg->filename;
	}

	slide = get_current_slide (show, NULL);
	slideshow_unref (show);
	size = find_best_size (slide->file1, best_width, best_height);
	return size->file;
}

gboolean
gnome_bg_get_image_size (GnomeBG	       *bg,
			 GnomeDesktopThumbnailFactory *factory,
			 int                    best_width,
			 int                    best_height,
			 int		       *width,
			 int		       *height)
{
	GdkPixbuf *thumb;
	gboolean result = FALSE;
	const gchar *filename;
	
	g_return_val_if_fail (bg != NULL, FALSE);
	g_return_val_if_fail (factory != NULL, FALSE);
	
	if (!bg->filename)
		return FALSE;
	
	filename = get_filename_for_size (bg, best_width, best_height);
	thumb = create_thumbnail_for_filename (factory, filename);
	if (thumb) {
		if (get_thumb_annotations (thumb, width, height))
			result = TRUE;
		
		g_object_unref (thumb);
	}

	if (!result) {
		if (get_original_size (filename, width, height))
			result = TRUE;
	}

	return result;
}

static double
fit_factor (int from_width, int from_height,
	    int to_width,   int to_height)
{
	return MIN (to_width  / (double) from_width, to_height / (double) from_height);
}

/**
 * gnome_bg_create_thumbnail:
 *
 * Returns: (transfer full): a #GdkPixbuf showing the background as a thumbnail
 */
GdkPixbuf *
gnome_bg_create_thumbnail (GnomeBG               *bg,
		           GnomeDesktopThumbnailFactory *factory,
			   GdkScreen             *screen,
			   int                    dest_width,
			   int                    dest_height)
{
	GdkPixbuf *result;
	GdkPixbuf *thumb;
	
	g_return_val_if_fail (bg != NULL, NULL);
	
	result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, dest_width, dest_height);
	
	draw_color (bg, result);
	
	if (bg->placement != C_DESKTOP_BACKGROUND_STYLE_NONE) {
		thumb = create_img_thumbnail (bg, factory, screen, dest_width, dest_height, -1);
		
		if (thumb) {
			draw_image_for_thumb (bg, thumb, result);
			g_object_unref (thumb);
		}
	}
	
	return result;
}

/**
 * gnome_bg_get_surface_from_root:
 * @screen: a #GdkScreen
 *
 * This function queries the _XROOTPMAP_ID property from
 * the root window associated with @screen to determine
 * the current root window background pixmap and returns
 * a copy of it. If the _XROOTPMAP_ID is not set, then
 * a black surface is returned.
 *
 * Return value: a #cairo_surface_t if successful or %NULL
 **/
cairo_surface_t *
gnome_bg_get_surface_from_root (GdkScreen *screen)
{
	int result;
	gint format;
	gulong nitems;
	gulong bytes_after;
	guchar *data;
	Atom type;
	Display *display;
	int screen_num;
	cairo_surface_t *surface;
	cairo_surface_t *source_pixmap;
	int width, height;
	cairo_t *cr;

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
	screen_num = gdk_screen_get_number (screen);

	result = XGetWindowProperty (display,
				     RootWindow (display, screen_num),
				     gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"),
				     0L, 1L, False, XA_PIXMAP,
				     &type, &format, &nitems, &bytes_after,
				     &data);
	surface = NULL;
	source_pixmap = NULL;

	if (result != Success || type != XA_PIXMAP ||
	    format != 32 || nitems != 1) {
		XFree (data);
		data = NULL;
	}

	if (data != NULL) {
                Pixmap xpixmap = *(Pixmap *) data;
                Window root_return;
                int x_ret, y_ret;
                unsigned int w_ret, h_ret, bw_ret, depth_ret;

		gdk_error_trap_push ();
                if (XGetGeometry (GDK_SCREEN_XDISPLAY (screen),
                                  xpixmap,
                                  &root_return,
                                  &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret)) {
                        source_pixmap = cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (screen),
                                                                   xpixmap,
                                                                   GDK_VISUAL_XVISUAL (gdk_screen_get_system_visual (screen)),
                                                                   w_ret, h_ret);
                }

                gdk_error_trap_pop_ignored ();
	}

	width = gdk_screen_get_width (screen);
	height = gdk_screen_get_height (screen);

        if (source_pixmap) {
                surface = cairo_surface_create_similar (source_pixmap,
                                                        CAIRO_CONTENT_COLOR,
                                                        width, height);

                cr = cairo_create (surface);
                cairo_set_source_surface (cr, source_pixmap, 0, 0);
                cairo_paint (cr);

                if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
                        cairo_surface_destroy (surface);
                        surface = NULL;
                }

                cairo_destroy (cr);
        }

        if (surface == NULL) {
	        surface = gdk_window_create_similar_surface (gdk_screen_get_root_window (screen),
                                                             CAIRO_CONTENT_COLOR,
                                                             width, height);
        }

	if (source_pixmap != NULL)
		cairo_surface_destroy (source_pixmap);

	if (data != NULL)
		XFree (data);

	return surface;
}

static void
gnome_bg_set_root_pixmap_id (GdkScreen       *screen,
			     cairo_surface_t *surface)
{
	int      result;
	gint     format;
	gulong   nitems;
	gulong   bytes_after;
	guchar  *data_esetroot;
	Pixmap   pixmap_id;
	Atom     type;
	Display *display;
	int      screen_num;
	GdkRGBA *average;

	screen_num = gdk_screen_get_number (screen);
	data_esetroot = NULL;

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

	result = XGetWindowProperty (display,
				     RootWindow (display, screen_num),
				     gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
				     0L, 1L, False, XA_PIXMAP,
				     &type, &format, &nitems,
				     &bytes_after,
				     &data_esetroot);

	if (data_esetroot != NULL) {
		if (result == Success && type == XA_PIXMAP &&
		    format == 32 &&
		    nitems == 1) {
			gdk_error_trap_push ();
			XKillClient (display, *(Pixmap *)data_esetroot);
                        gdk_error_trap_pop_ignored ();
		}
		XFree (data_esetroot);
	}
	
	pixmap_id = cairo_xlib_surface_get_drawable (surface);
	
	XChangeProperty (display, RootWindow (display, screen_num),
			 gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
			 XA_PIXMAP, 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);
	XChangeProperty (display, RootWindow (display, screen_num),
			 gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);

	average = cairo_surface_get_user_data (surface, &average_color_key);
	if (average != NULL) {
		gchar *string;

		string = gdk_rgba_to_string (average);

		/* X encodes string lists as one big string with a nul
		 * terminator after each item in the list.  That's why
		 * the strlen has to be given; scanning for nul would
		 * only find the first item.
		 *
		 * For now, we only want to set a single string.
		 * Fortunately, since this is C, it comes with its own
		 * nul and we can just give strlen + 1 for the size of
		 * our "list".
		 */
		XChangeProperty (display, RootWindow (display, screen_num),
		                 gdk_x11_get_xatom_by_name ("_GNOME_BACKGROUND_REPRESENTATIVE_COLORS"),
		                 XA_STRING, 8, PropModeReplace,
		                 (guchar *) string, strlen (string) + 1);
		g_free (string);
	} else {
		/* Could happen if we didn't create the surface... */
		XDeleteProperty (display, RootWindow (display, screen_num),
		                 gdk_x11_get_xatom_by_name ("_GNOME_BACKGROUND_REPRESENTATIVE_COLORS"));
	}
}

/**
 * gnome_bg_set_surface_as_root:
 * @screen: the #GdkScreen to change root background on
 * @surface: the #cairo_surface_t to set root background from.
 *   Must be an xlib surface backing a pixmap.
 *
 * Set the root pixmap, and properties pointing to it. We
 * do this atomically with a server grab to make sure that
 * we won't leak the pixmap if somebody else it setting
 * it at the same time. (This assumes that they follow the
 * same conventions we do).  @surface should come from a call
 * to gnome_bg_create_surface().
 **/
void
gnome_bg_set_surface_as_root (GdkScreen *screen, cairo_surface_t *surface)
{
	Display *display;
	int      screen_num;

	g_return_if_fail (screen != NULL);
	g_return_if_fail (surface != NULL);
	g_return_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_XLIB);

	screen_num = gdk_screen_get_number (screen);

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

	gdk_x11_display_grab (gdk_screen_get_display (screen));

	gnome_bg_set_root_pixmap_id (screen, surface);

	XSetWindowBackgroundPixmap (display, RootWindow (display, screen_num),
				    cairo_xlib_surface_get_drawable (surface));
	XClearWindow (display, RootWindow (display, screen_num));

	gdk_display_flush (gdk_screen_get_display (screen));
	gdk_x11_display_ungrab (gdk_screen_get_display (screen));
}

/**
 * gnome_bg_set_surface_as_root_with_crossfade:
 * @screen: the #GdkScreen to change root background on
 * @surface: the cairo xlib surface to set root background from
 *
 * Set the root pixmap, and properties pointing to it.
 * This function differs from gnome_bg_set_surface_as_root()
 * in that it adds a subtle crossfade animation from the
 * current root pixmap to the new one.
 *
 * Return value: (transfer full): a #GnomeBGCrossfade object
 **/
GnomeBGCrossfade *
gnome_bg_set_surface_as_root_with_crossfade (GdkScreen       *screen,
		 			     cairo_surface_t *surface)
{
	GdkDisplay *display;
	GdkWindow *root_window;
	cairo_surface_t *old_surface;
	int      width, height;
	GnomeBGCrossfade *fade;

	g_return_val_if_fail (screen != NULL, NULL);
	g_return_val_if_fail (surface != NULL, NULL);

	root_window = gdk_screen_get_root_window (screen);

	width = gdk_screen_get_width (screen);
	height = gdk_screen_get_height (screen);

	fade = gnome_bg_crossfade_new (width, height);

	display = gdk_screen_get_display (screen);
	gdk_x11_display_grab (display);
	old_surface = gnome_bg_get_surface_from_root (screen);
	gnome_bg_set_root_pixmap_id (screen, surface);
	gnome_bg_crossfade_set_start_surface (fade, old_surface);
	cairo_surface_destroy (old_surface);
	gnome_bg_crossfade_set_end_surface (fade, surface);
	gdk_display_flush (display);
	gdk_x11_display_ungrab (display);

	gnome_bg_crossfade_start (fade, root_window);

	return fade;
}

/**
 * gnome_bg_create_and_set_surface_as_root:
 * @root_window: the #GdkWindow
 * @screen: the #GdkScreen
 **/
void
gnome_bg_create_and_set_surface_as_root (GnomeBG *bg, GdkWindow *root_window, GdkScreen *screen)
{
    int width, height;
    cairo_surface_t *surface;

    width = gdk_screen_get_width (screen);
    height = gdk_screen_get_height (screen);

    surface = gnome_bg_create_surface (bg, root_window, width, height, TRUE);

    gnome_bg_set_surface_as_root (screen, surface);

    cairo_surface_destroy (surface);
}

/**
 * gnome_bg_create_and_set_gtk_image:
 **/
void
gnome_bg_create_and_set_gtk_image (GnomeBG *bg, GtkImage *image, gint width, gint height)
{
    cairo_surface_t *surface;

    int pm_width, pm_height;
    GdkRGBA average;
    GdkPixbuf *pixbuf;

    g_return_if_fail (bg != NULL);
    g_return_if_fail (image != NULL);

    g_object_ref (image);

    if (bg->pixbuf_cache &&
        gdk_pixbuf_get_width (bg->pixbuf_cache) != width &&
        gdk_pixbuf_get_height (bg->pixbuf_cache) != height) {
            g_object_unref (bg->pixbuf_cache);
            bg->pixbuf_cache = NULL;
    }

    GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (image));

    /* has the side effect of loading and caching pixbuf only when in tile mode */
    gnome_bg_get_pixmap_size (bg, width, height, &pm_width, &pm_height);

    if (!bg->filename && bg->color_type == C_DESKTOP_BACKGROUND_SHADING_SOLID) {
        cairo_t *cr;

        surface = gdk_window_create_similar_image_surface (window,
                                                           CAIRO_FORMAT_ARGB32,
                                                           pm_width, pm_height, 0);

        if (surface == NULL)
            return;

        cr = cairo_create (surface);

        gdk_cairo_set_source_color (cr, &(bg->primary));
        average.red = bg->primary.red / 65535.0;
        average.green = bg->primary.green / 65535.0;
        average.blue = bg->primary.blue / 65535.0;
        average.alpha = 1.0;

        cairo_paint (cr);
        cairo_destroy (cr);
    } else {
        gint scale = gtk_widget_get_scale_factor (GTK_WIDGET (image));

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
                                 width * scale, height * scale);
        gnome_bg_draw (bg, pixbuf, window ? gdk_window_get_screen (window) : gdk_screen_get_default (), FALSE);
        surface = gdk_cairo_surface_create_from_pixbuf (pixbuf,
                                                        scale,
                                                        window);
        pixbuf_average_value (pixbuf, &average);

        g_object_unref (pixbuf);
    }

    cairo_surface_set_user_data (surface, &average_color_key,
                                 gdk_rgba_copy (&average),
                                 (cairo_destroy_func_t) gdk_rgba_free);

    gtk_image_set_from_surface (image, surface);

    cairo_surface_destroy (surface);
    g_object_unref (image);
}

 // Implementation of the pixbuf cache 
struct _SlideShow
{
	gint ref_count;
	double start_time;
	double total_duration;

	GQueue *slides;
	
	gboolean has_multiple_sizes;

	/* used during parsing */
	struct tm start_tm;
	GQueue *stack;
};


static double
now (void)
{
	GTimeVal tv;

	g_get_current_time (&tv);

	return (double)tv.tv_sec + (tv.tv_usec / 1000000.0);
}

static Slide *
get_current_slide (SlideShow *show,
		   double    *alpha)
{
	double delta = fmod (now() - show->start_time, show->total_duration);
	GList *list;
	double elapsed;
	int i;

	if (delta < 0)
		delta += show->total_duration;

	elapsed = 0;
	i = 0;
	for (list = show->slides->head; list != NULL; list = list->next) {
		Slide *slide = list->data;

		if (elapsed + slide->duration > delta) {
			if (alpha)
				*alpha = (delta - elapsed) / (double)slide->duration;
			return slide;
		}

		i++;
		elapsed += slide->duration;
	}

	/* this should never happen since we have slides and we should always
	 * find a current slide for the elapsed time since beginning -- we're
	 * looping with fmod() */
	g_assert_not_reached ();

	return NULL;
}

static GdkPixbuf *
blend (GdkPixbuf *p1,
       GdkPixbuf *p2,
       double alpha)
{
	GdkPixbuf *result = gdk_pixbuf_copy (p1);
	GdkPixbuf *tmp;

	if (gdk_pixbuf_get_width (p2) != gdk_pixbuf_get_width (p1) ||
            gdk_pixbuf_get_height (p2) != gdk_pixbuf_get_height (p1)) {
		tmp = gdk_pixbuf_scale_simple (p2, 
					       gdk_pixbuf_get_width (p1),
					       gdk_pixbuf_get_height (p1),
					       GDK_INTERP_BILINEAR);
	}
        else {
		tmp = g_object_ref (p2);
	}
	
	pixbuf_blend (tmp, result, 0, 0, -1, -1, 0, 0, alpha);
        
        g_object_unref (tmp);	

	return result;
}

typedef	enum {
	PIXBUF,
	SLIDESHOW,
	THUMBNAIL
} FileType;

struct FileCacheEntry
{
	FileType type;
	char *filename;
	union {
		GdkPixbuf *pixbuf;
		SlideShow *slideshow;
		GdkPixbuf *thumbnail;
	} u;
};

static void
file_cache_entry_delete (FileCacheEntry *ent)
{
	g_free (ent->filename);
	
	switch (ent->type) {
	case PIXBUF:
		g_object_unref (ent->u.pixbuf);
		break;
	case SLIDESHOW:
		slideshow_unref (ent->u.slideshow);
		break;
	case THUMBNAIL:
		g_object_unref (ent->u.thumbnail);
		break;
	}

	g_free (ent);
}

static void
bound_cache (GnomeBG *bg)
{
      while (g_list_length (bg->file_cache) >= CACHE_SIZE) {
	      GList *last_link = g_list_last (bg->file_cache);
	      FileCacheEntry *ent = last_link->data;

	      file_cache_entry_delete (ent);

	      bg->file_cache = g_list_delete_link (bg->file_cache, last_link);
      }
}

static const FileCacheEntry *
file_cache_lookup (GnomeBG *bg, FileType type, const char *filename)
{
	GList *list;

	for (list = bg->file_cache; list != NULL; list = list->next) {
		FileCacheEntry *ent = list->data;

		if (ent && ent->type == type &&
		    strcmp (ent->filename, filename) == 0) {
			return ent;
		}
	}

	return NULL;
}

static FileCacheEntry *
file_cache_entry_new (GnomeBG *bg,
		      FileType type,
		      const char *filename)
{
	FileCacheEntry *ent = g_new0 (FileCacheEntry, 1);

	g_assert (!file_cache_lookup (bg, type, filename));
	
	ent->type = type;
	ent->filename = g_strdup (filename);

	bg->file_cache = g_list_prepend (bg->file_cache, ent);

	bound_cache (bg);
	
	return ent;
}

static void
file_cache_add_pixbuf (GnomeBG *bg,
		       const char *filename,
		       GdkPixbuf *pixbuf)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, PIXBUF, filename);
	ent->u.pixbuf = g_object_ref (pixbuf);
}

static void
file_cache_add_thumbnail (GnomeBG *bg,
			  const char *filename,
			  GdkPixbuf *pixbuf)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, THUMBNAIL, filename);
	ent->u.thumbnail = g_object_ref (pixbuf);
}

static void
file_cache_add_slide_show (GnomeBG *bg,
			   const char *filename,
			   SlideShow *show)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, SLIDESHOW, filename);
	ent->u.slideshow = slideshow_ref (show);
}

static GdkPixbuf *
load_from_cache_file (GnomeBG    *bg,
		      const char *filename,
		      gint        num_monitor,
		      gint        best_width,
		      gint        best_height)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *cache_filename;

	cache_filename = get_wallpaper_cache_filename (filename, num_monitor, bg->placement, best_width, best_height);
	if (cache_file_is_valid (filename, cache_filename))
		pixbuf = gdk_pixbuf_new_from_file (cache_filename, NULL);
	g_free (cache_filename);

	return pixbuf;
}

static GdkPixbuf *
get_as_pixbuf_for_size (GnomeBG    *bg,
			const char *filename,
			gint        num_monitor,
			gint        best_width,
			gint        best_height)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, PIXBUF, filename))) {
		return g_object_ref (ent->u.pixbuf);
	}
	else {
		GdkPixbufFormat *format;
		GdkPixbuf *pixbuf;
		GdkPixbuf *tmp_pixbuf;
                gchar *tmp;
		pixbuf = NULL;

		/* Try to hit local cache first if relevant */
		if (num_monitor != -1)
			pixbuf = load_from_cache_file (bg, filename, num_monitor, best_width, best_height);

		if (!pixbuf) {
			/* If scalable choose maximum size */
			format = gdk_pixbuf_get_file_info (filename, NULL, NULL);

			if (format != NULL) {
				tmp = gdk_pixbuf_format_get_name (format);
			} else {
				tmp = NULL;
			}

			if (tmp != NULL &&
			    strcmp (tmp, "svg") == 0 &&
			    (best_width > 0 && best_height > 0) &&
			    (bg->placement == C_DESKTOP_BACKGROUND_STYLE_STRETCHED ||
			     bg->placement == C_DESKTOP_BACKGROUND_STYLE_SCALED ||
			     bg->placement == C_DESKTOP_BACKGROUND_STYLE_ZOOM))
				pixbuf = gdk_pixbuf_new_from_file_at_size (filename, best_width, best_height, NULL);
			else
				pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			g_free (tmp);
		}

		if (pixbuf) {
			tmp_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
			g_object_unref (pixbuf);
			pixbuf = tmp_pixbuf;
			file_cache_add_pixbuf (bg, filename, pixbuf);
		}

		return pixbuf;
	}
}

static SlideShow *
get_as_slideshow (GnomeBG *bg, const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, SLIDESHOW, filename))) {
		return slideshow_ref (ent->u.slideshow);
	}
	else {
		SlideShow *show = read_slideshow_file (filename, NULL);

		if (show)
			file_cache_add_slide_show (bg, filename, show);

		return show;
	}
}

static GdkPixbuf *
get_as_thumbnail (GnomeBG *bg, GnomeDesktopThumbnailFactory *factory, const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, THUMBNAIL, filename))) {
		return g_object_ref (ent->u.thumbnail);
	}
	else {
		GdkPixbuf *thumb = create_thumbnail_for_filename (factory, filename);

		if (thumb)
			file_cache_add_thumbnail (bg, filename, thumb);

		return thumb;
	}
}

static gboolean
blow_expensive_caches (gpointer data)
{
	GnomeBG *bg = data;
	GList *list, *next;
	
	bg->blow_caches_id = 0;
	
	for (list = bg->file_cache; list != NULL; list = next) {
		FileCacheEntry *ent = list->data;
		next = list->next;
		
		if (ent->type == PIXBUF) {
			file_cache_entry_delete (ent);
			bg->file_cache = g_list_delete_link (bg->file_cache,
							     list);
		}
	}

	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		bg->pixbuf_cache = NULL;
	}

	return FALSE;
}

static void
blow_expensive_caches_in_idle (GnomeBG *bg)
{
	if (bg->blow_caches_id == 0) {
		bg->blow_caches_id =
			g_idle_add (blow_expensive_caches,
				    bg);
	}
}


static gboolean
on_timeout (gpointer data)
{
	GnomeBG *bg = data;

	bg->timeout_id = 0;
	
	queue_transitioned (bg);

	return FALSE;
}

static double
get_slide_timeout (Slide   *slide)
{
	double timeout;
	if (slide->fixed) {
		timeout = slide->duration;
	} else {
		/* Maybe the number of steps should be configurable? */
		
		/* In the worst case we will do a fade from 0 to 256, which mean
		 * we will never use more than 255 steps, however in most cases
		 * the first and last value are similar and users can't perceive
		 * changes in pixel values as small as 1/255th. So, lets not waste
		 * CPU cycles on transitioning to often.
		 *
		 * 64 steps is enough for each step to be just detectable in a 16bit
		 * color mode in the worst case, so we'll use this as an approximation
		 * of what's detectable.
		 */
		timeout = slide->duration / 64.0;
	}
	return timeout;
}

static void
ensure_timeout (GnomeBG *bg,
		Slide   *slide)
{
	if (!bg->timeout_id) {
		double timeout = get_slide_timeout (slide);

		/* G_MAXUINT means "only one slide" */
		if (timeout < G_MAXUINT) {
			bg->timeout_id = g_timeout_add_full (
				G_PRIORITY_LOW,
				timeout * 1000, on_timeout, bg, NULL);
		}
	}
}

static time_t
get_mtime (const char *filename)
{
	GFile     *file;
	GFileInfo *info;
	time_t     mtime;
	
	mtime = (time_t)-1;
	
	if (filename) {
		file = g_file_new_for_path (filename);
		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (info) {
			mtime = g_file_info_get_attribute_uint64 (info,
								  G_FILE_ATTRIBUTE_TIME_MODIFIED);
			g_object_unref (info);
		}
		g_object_unref (file);
	}
	
	return mtime;
}

static GdkPixbuf *
scale_thumbnail (CDesktopBackgroundStyle placement,
		 const char *filename,
		 GdkPixbuf *thumb,
		 GdkScreen *screen,
		 int	    dest_width,
		 int	    dest_height)
{
	int o_width;
	int o_height;
	
	if (placement != C_DESKTOP_BACKGROUND_STYLE_WALLPAPER &&
	    placement != C_DESKTOP_BACKGROUND_STYLE_CENTERED) {
		
		/* In this case, the pixbuf will be scaled to fit the screen anyway,
		 * so just return the pixbuf here
		 */
		return g_object_ref (thumb);
	}
	
	if (get_thumb_annotations (thumb, &o_width, &o_height)		||
	    (filename && get_original_size (filename, &o_width, &o_height))) {
		
		int scr_height = gdk_screen_get_height (screen);
		int scr_width = gdk_screen_get_width (screen);
		int thumb_width = gdk_pixbuf_get_width (thumb);
		int thumb_height = gdk_pixbuf_get_height (thumb);
		double screen_to_dest = fit_factor (scr_width, scr_height,
						    dest_width, dest_height);
		double thumb_to_orig  = fit_factor (thumb_width, thumb_height,
						    o_width, o_height);
		double f = thumb_to_orig * screen_to_dest;
		int new_width, new_height;
		
		new_width = floor (thumb_width * f + 0.5);
		new_height = floor (thumb_height * f + 0.5);

		if (placement == C_DESKTOP_BACKGROUND_STYLE_WALLPAPER) {
			/* Heuristic to make sure tiles don't become so small that
			 * they turn into a blur.
			 *
			 * This is strictly speaking incorrect, but the resulting
			 * thumbnail gives a much better idea what the background
			 * will actually look like.
			 */
			
			if ((new_width < 32 || new_height < 32) &&
			    (new_width < o_width / 4 || new_height < o_height / 4)) {
				new_width = o_width / 4;
				new_height = o_height / 4;
			}
		}
			
		thumb = gdk_pixbuf_scale_simple (thumb, new_width, new_height,
						 GDK_INTERP_BILINEAR);
	}
	else
		g_object_ref (thumb);
	
	return thumb;
}

/* frame_num determines which slide to thumbnail.
 * -1 means 'current slide'.
 */
static GdkPixbuf *
create_img_thumbnail (GnomeBG                      *bg,
		      GnomeDesktopThumbnailFactory *factory,
		      GdkScreen                    *screen,
		      int                           dest_width,
		      int                           dest_height,
		      int                           frame_num)
{
	if (bg->filename) {
		GdkPixbuf *thumb;

		thumb = get_as_thumbnail (bg, factory, bg->filename);

		if (thumb) {
			GdkPixbuf *result;
			result = scale_thumbnail (bg->placement,
						  bg->filename,
						  thumb,
						  screen,
						  dest_width,
						  dest_height);
			g_object_unref (thumb);
			return result;
		}
		else {
			SlideShow *show = get_as_slideshow (bg, bg->filename);

			if (show) {
				double alpha=255;
				Slide *slide;

				if (frame_num == -1)
					slide = get_current_slide (show, &alpha);
				else
					slide = g_queue_peek_nth (show->slides, frame_num);

				if (slide->fixed) {
					GdkPixbuf *tmp;
					FileSize *fs;
					fs = find_best_size (slide->file1, dest_width, dest_height);
					tmp = get_as_thumbnail (bg, factory, fs->file);
					if (tmp) {
						thumb = scale_thumbnail (bg->placement,
									 fs->file,
									 tmp,
									 screen,
									 dest_width,
									 dest_height);
						g_object_unref (tmp);
					}
				}
				else {
					FileSize *fs1, *fs2;
					GdkPixbuf *p1, *p2;
					fs1 = find_best_size (slide->file1, dest_width, dest_height);
					p1 = get_as_thumbnail (bg, factory, fs1->file);

					fs2 = find_best_size (slide->file2, dest_width, dest_height);
					p2 = get_as_thumbnail (bg, factory, fs2->file);

					if (p1 && p2) {
						GdkPixbuf *thumb1, *thumb2;

						thumb1 = scale_thumbnail (bg->placement,
									  fs1->file,
									  p1,
									  screen,
									  dest_width,
									  dest_height);

						thumb2 = scale_thumbnail (bg->placement,
									  fs2->file,
									  p2,
									  screen,
									  dest_width,
									  dest_height);

						thumb = blend (thumb1, thumb2, alpha);

						g_object_unref (thumb1);
						g_object_unref (thumb2);
					}
					if (p1)
						g_object_unref (p1);
					if (p2)
						g_object_unref (p2);
				}

				ensure_timeout (bg, slide);

				slideshow_unref (show);
			}
		}

		return thumb;
	}

	return NULL;
}

/*
 * Find the FileSize that best matches the given size.
 * Do two passes; the first pass only considers FileSizes
 * that are larger than the given size.
 * We are looking for the image that best matches the aspect ratio.
 * When two images have the same aspect ratio, prefer the one whose
 * width is closer to the given width.
 */
static FileSize *
find_best_size (GSList *sizes, gint width, gint height)
{
	GSList *s;
	gdouble a, d, distance;
	FileSize *best = NULL;
	gint pass;

	a = width/(gdouble)height;
	distance = 10000.0;

	for (pass = 0; pass < 2; pass++) {
		for (s = sizes; s; s = s->next) {
			FileSize *size = s->data;

			if (pass == 0 && (size->width < width || size->height < height))
				continue;       

			d = fabs (a - size->width/(gdouble)size->height);
			if (d < distance) {
				distance = d;
				best = size;
			} 
			else if (d == distance) {
				if (best == NULL)
				{
					if (abs (size->width - width) < abs (width)) {
						best = size;
					}
				}
				else
				{
					if (abs (size->width - width) < abs (best->width - width)) {
						best = size;
					}
				}
			}
		}

		if (best)
			break;
	}

	return best;
}

static GdkPixbuf *
get_pixbuf_for_size (GnomeBG *bg,
		     gint num_monitor,
		     gint best_width,
		     gint best_height)
{
	guint time_until_next_change;
	gboolean hit_cache = FALSE;

	/* only hit the cache if the aspect ratio matches */
	if (bg->pixbuf_cache) {
		int width, height;
		width = gdk_pixbuf_get_width (bg->pixbuf_cache);
		height = gdk_pixbuf_get_height (bg->pixbuf_cache);
		hit_cache = 0.2 > fabs ((best_width / (double)best_height) - (width / (double)height));
		if (!hit_cache) {
			g_object_unref (bg->pixbuf_cache);
			bg->pixbuf_cache = NULL;
		}
	}

	if (!hit_cache && bg->filename) {
		bg->file_mtime = get_mtime (bg->filename);

		bg->pixbuf_cache = get_as_pixbuf_for_size (bg, bg->filename, num_monitor, best_width, best_height);
		time_until_next_change = G_MAXUINT;
		if (!bg->pixbuf_cache) {
			SlideShow *show = get_as_slideshow (bg, bg->filename);

			if (show) {
				double alpha;
				Slide *slide;

				slideshow_ref (show);

				slide = get_current_slide (show, &alpha);
				time_until_next_change = (guint)get_slide_timeout (slide);
				if (slide->fixed) {
					FileSize *size;
					size = find_best_size (slide->file1, best_width, best_height);
					bg->pixbuf_cache = get_as_pixbuf_for_size (bg, size->file, num_monitor, best_width, best_height);
				}
				else {
					FileSize *size;
					GdkPixbuf *p1, *p2;
					size = find_best_size (slide->file1, best_width, best_height);
					p1 = get_as_pixbuf_for_size (bg, size->file, num_monitor, best_width, best_height);
					size = find_best_size (slide->file2, best_width, best_height);
					p2 = get_as_pixbuf_for_size (bg, size->file, num_monitor, best_width, best_height);

					if (p1 && p2) {
						bg->pixbuf_cache = blend (p1, p2, alpha);
					}
					if (p1)
						g_object_unref (p1);
					if (p2)
						g_object_unref (p2);
				}

				ensure_timeout (bg, slide);

				slideshow_unref (show);
			}
		}

		/* If the next slideshow step is a long time away then
		   we blow away the expensive stuff (large pixbufs) from
		   the cache */
		if (time_until_next_change > KEEP_EXPENSIVE_CACHE_SECS)
		    blow_expensive_caches_in_idle (bg);
	}

	if (bg->pixbuf_cache)
		g_object_ref (bg->pixbuf_cache);

	return bg->pixbuf_cache;
}

static gboolean
is_different (GnomeBG    *bg,
	      const char *filename)
{
	if (!filename && bg->filename) {
		return TRUE;
	}
	else if (filename && !bg->filename) {
		return TRUE;
	}
	else if (!filename && !bg->filename) {
		return FALSE;
	}
	else {
		time_t mtime = get_mtime (filename);
		
		if (mtime != bg->file_mtime)
			return TRUE;
		
		if (strcmp (filename, bg->filename) != 0)
			return TRUE;
		
		return FALSE;
	}
}

static void
clear_cache (GnomeBG *bg)
{
	GList *list;

	if (bg->file_cache) {
		for (list = bg->file_cache; list != NULL; list = list->next) {
			FileCacheEntry *ent = list->data;
			
			file_cache_entry_delete (ent);
		}
		g_list_free (bg->file_cache);
		bg->file_cache = NULL;
	}
	
	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		
		bg->pixbuf_cache = NULL;
	}

	if (bg->timeout_id) {
		g_source_remove (bg->timeout_id);
		bg->timeout_id = 0;
	}
}

/* Pixbuf utilities */
static void
pixbuf_average_value (GdkPixbuf *pixbuf,
                      GdkRGBA   *result)
{
	guint64 a_total, r_total, g_total, b_total;
	guint row, column;
	int row_stride;
	const guchar *pixels, *p;
	int r, g, b, a;
	guint64 dividend;
	guint width, height;
	gdouble dd;
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	/* iterate through the pixbuf, counting up each component */
	a_total = 0;
	r_total = 0;
	g_total = 0;
	b_total = 0;
	
	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				a = *p++;
				
				a_total += a;
				r_total += r * a;
				g_total += g * a;
				b_total += b * a;
			}
		}
		dividend = height * width * 0xFF;
		a_total *= 0xFF;
	} else {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				
				r_total += r;
				g_total += g;
				b_total += b;
			}
		}
		dividend = height * width;
		a_total = dividend * 0xFF;
	}

	dd = dividend * 0xFF;
	result->alpha = a_total / dd;
	result->red = r_total / dd;
	result->green = g_total / dd;
	result->blue = b_total / dd;
}

static GdkPixbuf *
pixbuf_scale_to_fit (GdkPixbuf *src, int max_width, int max_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	
	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);
	
	factor = MIN (max_width  / (double) src_width, max_height / (double) src_height);
	
	new_width  = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);
	
	return gdk_pixbuf_scale_simple (src, new_width, new_height, GDK_INTERP_BILINEAR);	
}

static GdkPixbuf *
pixbuf_scale_to_min (GdkPixbuf *src, int min_width, int min_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	GdkPixbuf *dest;

	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);

	factor = MAX (min_width / (double) src_width, min_height / (double) src_height);

	new_width = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);

	dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       gdk_pixbuf_get_has_alpha (src),
			       8, min_width, min_height);
	if (!dest)
		return NULL;

	/* crop the result */
	gdk_pixbuf_scale (src, dest,
			  0, 0,
			  min_width, min_height,
			  (new_width - min_width) / -2,
			  (new_height - min_height) / -2,
			  factor,
			  factor,
			  GDK_INTERP_BILINEAR);
	return dest;
}

static guchar *
create_gradient (const GdkColor *primary,
		 const GdkColor *secondary,
		 int	         n_pixels)
{
	guchar *result = g_malloc (n_pixels * 3);
	int i;
	
	for (i = 0; i < n_pixels; ++i) {
		double ratio = (i + 0.5) / n_pixels;
		
		result[3 * i + 0] = ((guint16) (primary->red * (1 - ratio) + secondary->red * ratio)) >> 8;
		result[3 * i + 1] = ((guint16) (primary->green * (1 - ratio) + secondary->green * ratio)) >> 8;
		result[3 * i + 2] = ((guint16) (primary->blue * (1 - ratio) + secondary->blue * ratio)) >> 8;
	}
	
	return result;
}	

static void
pixbuf_draw_gradient (GdkPixbuf    *pixbuf,
		      gboolean      horizontal,
		      GdkColor     *primary,
		      GdkColor     *secondary,
		      GdkRectangle *rect)
{
	int width;
	int height;
	int rowstride;
	guchar *dst;
	int n_channels = 3;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	width = rect->width;
	height = rect->height;
	dst = gdk_pixbuf_get_pixels (pixbuf) + rect->x * n_channels + rowstride * rect->y;

	if (horizontal) {
		guchar *gradient = create_gradient (primary, secondary, width);
		int copy_bytes_per_row = width * n_channels;
		int i;

		for (i = 0; i < height; i++) {
			guchar *d;
			d = dst + rowstride * i;
			memcpy (d, gradient, copy_bytes_per_row);
		}
		g_free (gradient);
	} else {
		guchar *gb, *gradient;
		int i;

		gradient = create_gradient (primary, secondary, height);
		for (i = 0; i < height; i++) {
			int j;
			guchar *d;

			d = dst + rowstride * i;
			gb = gradient + n_channels * i;
			for (j = width; j > 0; j--) {
				int k;

				for (k = 0; k < n_channels; k++) {
					*(d++) = gb[k];
				}
			}
		}

		g_free (gradient);
	}
}

static void
pixbuf_blend (GdkPixbuf *src,
	      GdkPixbuf *dest,
	      int	 src_x,
	      int	 src_y,
	      int	 src_width,
	      int        src_height,
	      int	 dest_x,
	      int	 dest_y,
	      double	 alpha)
{
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	int offset_x = dest_x - src_x;
	int offset_y = dest_y - src_y;

	if (src_width < 0)
		src_width = gdk_pixbuf_get_width (src);

	if (src_height < 0)
		src_height = gdk_pixbuf_get_height (src);
	
	if (dest_x < 0)
		dest_x = 0;
	
	if (dest_y < 0)
		dest_y = 0;
	
	if (dest_x + src_width > dest_width) {
		src_width = dest_width - dest_x;
	}
	
	if (dest_y + src_height > dest_height) {
		src_height = dest_height - dest_y;
	}

	gdk_pixbuf_composite (src, dest,
			      dest_x, dest_y,
			      src_width, src_height,
			      offset_x, offset_y,
			      1, 1, GDK_INTERP_NEAREST,
			      alpha * 0xFF + 0.5);
}

static void
pixbuf_tile (GdkPixbuf *src, GdkPixbuf *dest)
{
	int x, y;
	int tile_width, tile_height;
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	tile_width = gdk_pixbuf_get_width (src);
	tile_height = gdk_pixbuf_get_height (src);
	
	for (y = 0; y < dest_height; y += tile_height) {
		for (x = 0; x < dest_width; x += tile_width) {
			pixbuf_blend (src, dest, 0, 0,
				      tile_width, tile_height, x, y, 1.0);
		}
	}
}

static gboolean stack_is (SlideShow *parser, const char *s1, ...);

/* Parser for fading background */
static void
handle_start_element (GMarkupParseContext *context,
		      const gchar         *name,
		      const gchar        **attr_names,
		      const gchar        **attr_values,
		      gpointer             user_data,
		      GError             **err)
{
	SlideShow *parser = user_data;
	gint i;
	
	if (strcmp (name, "static") == 0 || strcmp (name, "transition") == 0) {
		Slide *slide = g_new0 (Slide, 1);
		
		if (strcmp (name, "static") == 0)
			slide->fixed = TRUE;
		
		g_queue_push_tail (parser->slides, slide);
	}
	else if (strcmp (name, "size") == 0) {
		Slide *slide = parser->slides->tail->data;
		FileSize *size = g_new0 (FileSize, 1);
		for (i = 0; attr_names[i]; i++) {
			if (strcmp (attr_names[i], "width") == 0)
				size->width = atoi (attr_values[i]);
			else if (strcmp (attr_names[i], "height") == 0)
				size->height = atoi (attr_values[i]);
		}
		if (parser->stack->tail &&
		    (strcmp (parser->stack->tail->data, "file") == 0 ||
		     strcmp (parser->stack->tail->data, "from") == 0)) {
			slide->file1 = g_slist_prepend (slide->file1, size);
		}
		else if (parser->stack->tail &&
			 strcmp (parser->stack->tail->data, "to") == 0) { 
			slide->file2 = g_slist_prepend (slide->file2, size);
		}
	}
	g_queue_push_tail (parser->stack, g_strdup (name));
}

static void
handle_end_element (GMarkupParseContext *context,
		    const gchar         *name,
		    gpointer             user_data,
		    GError             **err)
{
	SlideShow *parser = user_data;
	
	g_free (g_queue_pop_tail (parser->stack));
}

static gboolean
stack_is (SlideShow *parser,
	  const char *s1,
	  ...)
{
	GList *stack = NULL;
	const char *s;
	GList *l1, *l2;
	va_list args;
	
	stack = g_list_prepend (stack, (gpointer)s1);
	
	va_start (args, s1);
	
	s = va_arg (args, const char *);
	while (s) {
		stack = g_list_prepend (stack, (gpointer)s);
		s = va_arg (args, const char *);
	}

	va_end (args);
	
	l1 = stack;
	l2 = parser->stack->head;
	
	while (l1 && l2) {
		if (strcmp (l1->data, l2->data) != 0) {
			g_list_free (stack);
			return FALSE;
		}
		
		l1 = l1->next;
		l2 = l2->next;
	}

	g_list_free (stack);

	return (!l1 && !l2);
}

static int
parse_int (const char *text)
{
	return strtol (text, NULL, 10);
}

static void
handle_text (GMarkupParseContext *context,
	     const gchar         *text,
	     gsize                text_len,
	     gpointer             user_data,
	     GError             **err)
{
	SlideShow *parser = user_data;
	FileSize *fs;
	gint i;

	g_return_if_fail (parser != NULL);
	g_return_if_fail (parser->slides != NULL);

	Slide *slide = parser->slides->tail ? parser->slides->tail->data : NULL;

	if (stack_is (parser, "year", "starttime", "background", NULL)) {
		parser->start_tm.tm_year = parse_int (text) - 1900;
	}
	else if (stack_is (parser, "month", "starttime", "background", NULL)) {
		parser->start_tm.tm_mon = parse_int (text) - 1;
	}
	else if (stack_is (parser, "day", "starttime", "background", NULL)) {
		parser->start_tm.tm_mday = parse_int (text);
	}
	else if (stack_is (parser, "hour", "starttime", "background", NULL)) {
		parser->start_tm.tm_hour = parse_int (text);
	}
	else if (stack_is (parser, "minute", "starttime", "background", NULL)) {
		parser->start_tm.tm_min = parse_int (text);
	}
	else if (stack_is (parser, "second", "starttime", "background", NULL)) {
		parser->start_tm.tm_sec = parse_int (text);
	}
	else if (stack_is (parser, "duration", "static", "background", NULL) ||
		 stack_is (parser, "duration", "transition", "background", NULL)) {
		g_return_if_fail (slide != NULL);

		slide->duration = g_strtod (text, NULL);
		parser->total_duration += slide->duration;
	}
	else if (stack_is (parser, "file", "static", "background", NULL) ||
		 stack_is (parser, "from", "transition", "background", NULL)) {
		g_return_if_fail (slide != NULL);

		for (i = 0; text[i]; i++) {
			if (!g_ascii_isspace (text[i]))
				break;
		}
		if (text[i] == 0)
			return;
		fs = g_new (FileSize, 1);
		fs->width = -1;
		fs->height = -1;
		fs->file = g_strdup (text);
		slide->file1 = g_slist_prepend (slide->file1, fs);
		if (slide->file1->next != NULL)
			parser->has_multiple_sizes = TRUE;                       
	}
	else if (stack_is (parser, "size", "file", "static", "background", NULL) ||
		 stack_is (parser, "size", "from", "transition", "background", NULL)) {
		g_return_if_fail (slide != NULL);

		fs = slide->file1->data;
		fs->file = g_strdup (text);
		if (slide->file1->next != NULL)
			parser->has_multiple_sizes = TRUE; 
	}
	else if (stack_is (parser, "to", "transition", "background", NULL)) {
		g_return_if_fail (slide != NULL);

		for (i = 0; text[i]; i++) {
			if (!g_ascii_isspace (text[i]))
				break;
		}
		if (text[i] == 0)
			return;
		fs = g_new (FileSize, 1);
		fs->width = -1;
		fs->height = -1;
		fs->file = g_strdup (text);
		slide->file2 = g_slist_prepend (slide->file2, fs);
		if (slide->file2->next != NULL)
			parser->has_multiple_sizes = TRUE;                       
	}
	else if (stack_is (parser, "size", "to", "transition", "background", NULL)) {
		g_return_if_fail (slide != NULL);

		fs = slide->file2->data;
		fs->file = g_strdup (text);
		if (slide->file2->next != NULL)
			parser->has_multiple_sizes = TRUE;
	}
}

static SlideShow *
slideshow_ref (SlideShow *show)
{
	show->ref_count++;
	return show;
}

static void
slideshow_unref (SlideShow *show)
{
	GList *list;
	GSList *slist;
	FileSize *size;

	show->ref_count--;
	if (show->ref_count > 0)
		return;

	for (list = show->slides->head; list != NULL; list = list->next) {
		Slide *slide = list->data;

		for (slist = slide->file1; slist != NULL; slist = slist->next) {
			size = slist->data;
			g_free (size->file);
			g_free (size);
		}
		g_slist_free (slide->file1);

		for (slist = slide->file2; slist != NULL; slist = slist->next) {
			size = slist->data;
			g_free (size->file);
			g_free (size);
		}
		g_slist_free (slide->file2);

		g_free (slide);
	}

	g_queue_free (show->slides);
	
	g_list_foreach (show->stack->head, (GFunc) g_free, NULL);
	g_queue_free (show->stack);
	
	g_free (show);
}

static void
dump_bg (SlideShow *show)
{
#if 0
	GList *list;
	GSList *slist;
	
	for (list = show->slides->head; list != NULL; list = list->next)
	{
		Slide *slide = list->data;
		
		g_print ("\nSlide: %s\n", slide->fixed? "fixed" : "transition");
		g_print ("duration: %f\n", slide->duration);
		g_print ("File1:\n");
		for (slist = slide->file1; slist != NULL; slist = slist->next) {
			FileSize *size = slist->data;
			g_print ("\t%s (%dx%d)\n", 
				 size->file, size->width, size->height);
		}
		g_print ("File2:\n");
		for (slist = slide->file2; slist != NULL; slist = slist->next) {
			FileSize *size = slist->data;
			g_print ("\t%s (%dx%d)\n", 
				 size->file, size->width, size->height);
		}
	}
#endif
}

static void
threadsafe_localtime (time_t time, struct tm *tm)
{
	struct tm *res;
	
	G_LOCK_DEFINE_STATIC (localtime_mutex);

	G_LOCK (localtime_mutex);

	res = localtime (&time);
	if (tm) {
		*tm = *res;
	}
	
	G_UNLOCK (localtime_mutex);
}

static SlideShow *
read_slideshow_file (const char *filename,
		     GError     **err)
{
	GMarkupParser parser = {
		handle_start_element,
		handle_end_element,
		handle_text,
		NULL, /* passthrough */
		NULL, /* error */
	};
	
	GFile *file;
	char *contents = NULL;
	gsize len;
	SlideShow *show = NULL;
	GMarkupParseContext *context = NULL;
	time_t t;

	if (!filename)
		return NULL;

	file = g_file_new_for_path (filename);
	if (!g_file_load_contents (file, NULL, &contents, &len, NULL, NULL)) {
		g_object_unref (file);
		return NULL;
	}
	g_object_unref (file);
	
	show = g_new0 (SlideShow, 1);
	show->ref_count = 1;
	threadsafe_localtime ((time_t)0, &show->start_tm);
	show->stack = g_queue_new ();
	show->slides = g_queue_new ();
	
	context = g_markup_parse_context_new (&parser, 0, show, NULL);
	
	if (!g_markup_parse_context_parse (context, contents, len, err)) {
		slideshow_unref (show);
		show = NULL;
	}
	

	if (show) {
		if (!g_markup_parse_context_end_parse (context, err)) {
			slideshow_unref (show);
			show = NULL;
		}
	}
	
	g_markup_parse_context_free (context);

	if (show) {
		int len;

		t = mktime (&show->start_tm);

		show->start_time = (double)t;
			
		dump_bg (show);

		len = g_queue_get_length (show->slides);

		/* no slides, that's not a slideshow */
		if (len == 0) {
			slideshow_unref (show);
			show = NULL;
		/* one slide, there's no transition */
		} else if (len == 1) {
			Slide *slide = show->slides->head->data;
			slide->duration = show->total_duration = G_MAXUINT;
		}
	}

	g_free (contents);
	
	return show;
}

/* Thumbnail utilities */
static GdkPixbuf *
create_thumbnail_for_filename (GnomeDesktopThumbnailFactory *factory,
			       const char            *filename)
{
	char *thumb;
	time_t mtime;
	GdkPixbuf *orig, *result = NULL;
	char *uri;
	
	mtime = get_mtime (filename);
	
	if (mtime == (time_t)-1)
		return NULL;
	
	uri = g_filename_to_uri (filename, NULL, NULL);
	
	if (uri == NULL)
		return NULL;
	
	thumb = gnome_desktop_thumbnail_factory_lookup (factory, uri, mtime);
	
	if (thumb) {
		result = gdk_pixbuf_new_from_file (thumb, NULL);
		g_free (thumb);
	}
	else {
		orig = gdk_pixbuf_new_from_file (filename, NULL);
		if (orig) {
			int orig_width = gdk_pixbuf_get_width (orig);
			int orig_height = gdk_pixbuf_get_height (orig);
			
			result = pixbuf_scale_to_fit (orig, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
			
			g_object_set_data_full (G_OBJECT (result), "gnome-thumbnail-height",
						g_strdup_printf ("%d", orig_height), g_free);
			g_object_set_data_full (G_OBJECT (result), "gnome-thumbnail-width",
						g_strdup_printf ("%d", orig_width), g_free);
			
			g_object_unref (orig);
			
			gnome_desktop_thumbnail_factory_save_thumbnail (factory, result, uri, mtime);
		}
		else {
			gnome_desktop_thumbnail_factory_create_failed_thumbnail (factory, uri, mtime);
		}
	}

	g_free (uri);

	return result;
}

static gboolean
get_thumb_annotations (GdkPixbuf *thumb,
		       int	 *orig_width,
		       int	 *orig_height)
{
	char *end;
	const char *wstr, *hstr;
	
	wstr = gdk_pixbuf_get_option (thumb, "tEXt::Thumb::Image::Width");
	hstr = gdk_pixbuf_get_option (thumb, "tEXt::Thumb::Image::Height");
	
	if (hstr && wstr) {
		*orig_width = strtol (wstr, &end, 10);
		if (*end != 0)
			return FALSE;
		
		*orig_height = strtol (hstr, &end, 10);
		if (*end != 0)
			return FALSE;
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
slideshow_has_multiple_sizes (SlideShow *show)
{
	return show->has_multiple_sizes;
}

/*
 * Returns whether the background is a slideshow.
 */
gboolean
gnome_bg_changes_with_time (GnomeBG *bg)
{
	SlideShow *show;
	gboolean ret = FALSE;

	g_return_val_if_fail (bg != NULL, FALSE);

	if (!bg->filename)
		return FALSE;

	show = get_as_slideshow (bg, bg->filename);
	if (show) {
                ret = g_queue_get_length (show->slides) > 1;
                g_object_unref (show);
        }

	return ret;
}

/**
 * gnome_bg_create_frame_thumbnail:
 *
 * Creates a thumbnail for a certain frame, where 'frame' is somewhat
 * vaguely defined as 'suitable point to show while single-stepping
 * through the slideshow'.
 *
 * Returns: (transfer full): the newly created thumbnail or
 * or NULL if frame_num is out of bounds.
 */
GdkPixbuf *
gnome_bg_create_frame_thumbnail (GnomeBG			*bg,
				 GnomeDesktopThumbnailFactory	*factory,
				 GdkScreen			*screen,
				 int				 dest_width,
				 int				 dest_height,
				 int				 frame_num)
{
	SlideShow *show;
	GdkPixbuf *result;
	GdkPixbuf *thumb;
        GList *l;
        int i, skipped;
        gboolean found;

	g_return_val_if_fail (bg != NULL, FALSE);

	show = get_as_slideshow (bg, bg->filename);

	if (!show)
		return NULL;


        if (frame_num < 0 || frame_num >= g_queue_get_length (show->slides)) {
                g_object_unref (show);
		return NULL;
        }

	i = 0;
	skipped = 0;
	found = FALSE;
	for (l = show->slides->head; l; l = l->next) {
		Slide *slide = l->data;
		if (!slide->fixed) {
			skipped++;
			continue;
		}
		if (i == frame_num) {
			found = TRUE;
			break;
		}
		i++;
	}
        g_object_unref (show);
	if (!found)
		return NULL;


	result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, dest_width, dest_height);

	draw_color (bg, result);

	if (bg->placement != C_DESKTOP_BACKGROUND_STYLE_NONE) {
		thumb = create_img_thumbnail (bg, factory, screen, dest_width, dest_height, frame_num + skipped);

		if (thumb) {
			draw_image_for_thumb (bg, thumb, result);
			g_object_unref (thumb);
		}
	}

	return result;
}

