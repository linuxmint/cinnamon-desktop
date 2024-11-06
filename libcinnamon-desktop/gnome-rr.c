/* gnome-rr.c
 *
 * Copyright 2007, 2008, 2013 Red Hat, Inc.
 * 
 * This file is part of the Gnome Library.
 * 
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 *         Giovanni Campagna <gcampagn@redhat.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gtk/gtk.h>

#undef GNOME_DISABLE_DEPRECATED
#include "gnome-rr.h"
#include "gnome-rr-config.h"

#include "gnome-rr-private.h"

/* From xf86drmMode.h: it's ABI so it won't change */
#define DRM_MODE_FLAG_INTERLACE			(1<<4)

enum {
    SCREEN_PROP_0,
    SCREEN_PROP_GDK_SCREEN,
    SCREEN_PROP_DPMS_MODE,
    SCREEN_PROP_LAST,
};

enum {
    SCREEN_CHANGED,
    SCREEN_OUTPUT_CONNECTED,
    SCREEN_OUTPUT_DISCONNECTED,
    SCREEN_SIGNAL_LAST,
};

static gint screen_signals[SCREEN_SIGNAL_LAST];

struct GnomeRROutput
{
    ScreenInfo *	info;
    guint		id;
    glong               winsys_id;
    
    char *		name;
    char *		display_name;
    char *		connector_type;
    GnomeRRCrtc *	current_crtc;
    GnomeRRCrtc **	possible_crtcs;
    GnomeRROutput **	clones;
    GnomeRRMode **	modes;

    char *              vendor;
    char *              product;
    char *              serial;
    int                 width_mm;
    int                 height_mm;
    GBytes *            edid;
    char *              edid_file;

    int                 backlight;
    int                 min_backlight_step;

    gboolean            is_primary;
    gboolean            is_presentation;
    gboolean            is_underscanning;
    gboolean            supports_underscanning;

    GnomeRRTile         tile_info;
};

struct GnomeRRCrtc
{
    ScreenInfo *	info;
    guint		id;
    glong               winsys_id;
    
    GnomeRRMode *	current_mode;
    GnomeRROutput **	current_outputs;
    GnomeRROutput **	possible_outputs;
    int			x;
    int			y;
    
    enum wl_output_transform transform;
    int                 all_transforms;
    int			gamma_size;
};

#define UNDEFINED_MODE_ID 0
struct GnomeRRMode
{
    ScreenInfo *	info;
    guint		id;
    glong               winsys_id;
    int			width;
    int			height;
    int			freq;		/* in mHz */
    gboolean		tiled;
    guint32             flags;
};

/* GnomeRRCrtc */
static GnomeRRCrtc *  crtc_new          (ScreenInfo         *info,
					 guint               id);
static GnomeRRCrtc *  crtc_copy         (const GnomeRRCrtc  *from);
static void           crtc_free         (GnomeRRCrtc        *crtc);

static void           crtc_initialize   (GnomeRRCrtc        *crtc,
					 GVariant           *res);

/* GnomeRROutput */
static GnomeRROutput *output_new        (ScreenInfo         *info,
					 guint               id);

static void           output_initialize (GnomeRROutput      *output,
					 GVariant           *res);

static GnomeRROutput *output_copy       (const GnomeRROutput *from);
static void           output_free       (GnomeRROutput      *output);

/* GnomeRRMode */
static GnomeRRMode *  mode_new          (ScreenInfo         *info,
					 guint               id);

static void           mode_initialize   (GnomeRRMode        *mode,
					 GVariant           *info);

static GnomeRRMode *  mode_copy         (const GnomeRRMode  *from);
static void           mode_free         (GnomeRRMode        *mode);

static void gnome_rr_screen_finalize (GObject*);
static void gnome_rr_screen_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void gnome_rr_screen_get_property (GObject*, guint, GValue*, GParamSpec*);
static gboolean gnome_rr_screen_initable_init (GInitable*, GCancellable*, GError**);
static void gnome_rr_screen_initable_iface_init (GInitableIface *iface);
static void gnome_rr_screen_async_initable_init (GAsyncInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GnomeRRScreen, gnome_rr_screen, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GnomeRRScreen)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gnome_rr_screen_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, gnome_rr_screen_async_initable_init))

G_DEFINE_BOXED_TYPE (GnomeRRCrtc, gnome_rr_crtc, crtc_copy, crtc_free)
G_DEFINE_BOXED_TYPE (GnomeRROutput, gnome_rr_output, output_copy, output_free)
G_DEFINE_BOXED_TYPE (GnomeRRMode, gnome_rr_mode, mode_copy, mode_free)

/* Errors */

/**
 * gnome_rr_error_quark:
 *
 * Returns the #GQuark that will be used for #GError values returned by the
 * GnomeRR API.
 *
 * Return value: a #GQuark used to identify errors coming from the GnomeRR API.
 */
GQuark
gnome_rr_error_quark (void)
{
    return g_quark_from_static_string ("gnome-rr-error-quark");
}

/* Screen */
static GnomeRROutput *
gnome_rr_output_by_id (ScreenInfo *info, guint id)
{
    GnomeRROutput **output;
    
    g_assert (info != NULL);
    
    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }
    
    return NULL;
}

static GnomeRRCrtc *
crtc_by_id (ScreenInfo *info, guint id)
{
    GnomeRRCrtc **crtc;
    
    if (!info)
        return NULL;
    
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }
    
    return NULL;
}

static GnomeRRMode *
mode_by_id (ScreenInfo *info, guint id)
{
    GnomeRRMode **mode;
    
    g_assert (info != NULL);
    
    for (mode = info->modes; *mode; ++mode)
    {
	if ((*mode)->id == id)
	    return *mode;
    }
    
    return NULL;
}

static void
screen_info_free (ScreenInfo *info)
{
    GnomeRROutput **output;
    GnomeRRCrtc **crtc;
    GnomeRRMode **mode;
    
    g_assert (info != NULL);

    if (info->outputs)
    {
	for (output = info->outputs; *output; ++output)
	    output_free (*output);
	g_free (info->outputs);
    }
    
    if (info->crtcs)
    {
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_free (*crtc);
	g_free (info->crtcs);
    }
    
    if (info->modes)
    {
	for (mode = info->modes; *mode; ++mode)
	    mode_free (*mode);
	g_free (info->modes);
    }

    if (info->clone_modes)
    {
	/* The modes themselves were freed above */
	g_free (info->clone_modes);
    }
    
    g_free (info);
}

static gboolean
has_similar_mode (GnomeRROutput *output, GnomeRRMode *mode)
{
    int i;
    GnomeRRMode **modes = gnome_rr_output_list_modes (output);
    guint width = gnome_rr_mode_get_width (mode);
    guint height = gnome_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	GnomeRRMode *m = modes[i];

	if (gnome_rr_mode_get_width (m) == width	&&
	    gnome_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

gboolean
_gnome_rr_output_get_tiled_display_size (GnomeRROutput *output,
					 int *tile_w, int *tile_h,
					 int *total_width, int *total_height)
{
    GnomeRRTile tile;
    guint ht, vt;
    int i, total_h = 0, total_w = 0;

    if (!_gnome_rr_output_get_tile_info (output, &tile))
	return FALSE;

    if (tile.loc_horiz != 0 ||
	tile.loc_vert != 0)
	return FALSE;

    if (tile_w)
	*tile_w = tile.width;
    if (tile_h)
	*tile_h = tile.height;

    for (ht = 0; ht < tile.max_horiz_tiles; ht++)
    {
	for (vt = 0; vt < tile.max_vert_tiles; vt++)
	{
	    for (i = 0; output->info->outputs[i]; i++)
	    {
		GnomeRRTile this_tile;

		if (!_gnome_rr_output_get_tile_info (output->info->outputs[i], &this_tile))
		    continue;

		if (this_tile.group_id != tile.group_id)
		    continue;

		if (this_tile.loc_horiz != ht ||
		    this_tile.loc_vert != vt)
		    continue;

		if (this_tile.loc_horiz == 0)
		    total_h += this_tile.height;

		if (this_tile.loc_vert == 0)
		    total_w += this_tile.width;
	    }
	}
    }

    *total_width = total_w;
    *total_height = total_h;
    return TRUE;
}

static void
gather_tile_modes_output (ScreenInfo *info, GnomeRROutput *output)
{
    GPtrArray *a;
    GnomeRRMode *mode;
    int width, height;
    int tile_w, tile_h;
    int i;

    if (!_gnome_rr_output_get_tiled_display_size (output, &tile_w, &tile_h,
						  &width, &height))
	return;

    /* now stick the mode into the modelist */
    a = g_ptr_array_new ();
    mode = mode_new (info, UNDEFINED_MODE_ID);
    mode->winsys_id = 0;
    mode->width = width;
    mode->height = height;
    mode->freq = 0;
    mode->tiled = TRUE;

    g_ptr_array_add (a, mode);
    for (i = 0; output->modes[i]; i++)
	g_ptr_array_add (a, output->modes[i]);

    g_ptr_array_add (a, NULL);
    output->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);
}

static void
gather_tile_modes (ScreenInfo *info)
{
    int i;

    for (i = 0; info->outputs[i]; i++)
	gather_tile_modes_output (info, info->outputs[i]);
}

static void
gather_clone_modes (ScreenInfo *info)
{
    int i;
    GPtrArray *result = g_ptr_array_new ();

    for (i = 0; info->outputs[i] != NULL; ++i)
    {
	int j;
	GnomeRROutput *output1, *output2;

	output1 = info->outputs[i];
	
	for (j = 0; output1->modes[j] != NULL; ++j)
	{
	    GnomeRRMode *mode = output1->modes[j];
	    gboolean valid;
	    int k;

	    valid = TRUE;
	    for (k = 0; info->outputs[k] != NULL; ++k)
	    {
		output2 = info->outputs[k];
		
		if (!has_similar_mode (output2, mode))
		{
		    valid = FALSE;
		    break;
		}
	    }

	    if (valid)
		g_ptr_array_add (result, mode);
	}
    }

    g_ptr_array_add (result, NULL);
    
    info->clone_modes = (GnomeRRMode **)g_ptr_array_free (result, FALSE);
}

static void
fill_screen_info_from_resources (ScreenInfo *info,
				 guint       serial,
				 GVariant   *crtcs,
				 GVariant   *outputs,
				 GVariant   *modes,
				 int         max_width,
				 int         max_height)
{
    guint i;
    GPtrArray *a;
    GnomeRRCrtc **crtc;
    GnomeRROutput **output;
    GnomeRRMode **mode;
    guint ncrtc, noutput, nmode;
    guint id;

    info->min_width = 312;
    info->min_height = 312;
    info->max_width = max_width;
    info->max_height = max_height;
    info->serial = serial;

    ncrtc = g_variant_n_children (crtcs);
    noutput = g_variant_n_children (outputs);
    nmode = g_variant_n_children (modes);

    /* We create all the structures before initializing them, so
     * that they can refer to each other.
     */
    a = g_ptr_array_new ();
    for (i = 0; i < ncrtc; ++i)
    {
	g_variant_get_child (crtcs, i, META_CRTC_STRUCT, &id,
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	g_ptr_array_add (a, crtc_new (info, id));
    }
    g_ptr_array_add (a, NULL);
    info->crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0; i < noutput; ++i)
    {
	g_variant_get_child (outputs, i, META_OUTPUT_STRUCT, &id,
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	g_ptr_array_add (a, output_new (info, id));
    }
    g_ptr_array_add (a, NULL);
    info->outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0;  i < nmode; ++i)
    {
	g_variant_get_child (modes, i, META_MONITOR_MODE_STRUCT, &id,
			     NULL, NULL, NULL, NULL, NULL);

	g_ptr_array_add (a, mode_new (info, id));
    }
    g_ptr_array_add (a, NULL);
    info->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);

    /* Initialize */
    for (i = 0, crtc = info->crtcs; *crtc; ++i, ++crtc)
    {
	GVariant *child = g_variant_get_child_value (crtcs, i);
	crtc_initialize (*crtc, child);
	g_variant_unref (child);
    }

    for (i = 0, output = info->outputs; *output; ++i, ++output)
    {
	GVariant *child = g_variant_get_child_value (outputs, i);
	output_initialize (*output, child);
	g_variant_unref (child);
    }

    for (i = 0, mode = info->modes; *mode; ++i, ++mode)
    {
	GVariant *child = g_variant_get_child_value (modes, i);
	mode_initialize (*mode, child);
	g_variant_unref (child);
    }

    gather_clone_modes (info);

    gather_tile_modes (info);
}

static gboolean
fill_out_screen_info (ScreenInfo  *info,
		      GError     **error)
{
    GnomeRRScreenPrivate *priv;
    guint serial;
    GVariant *crtcs, *outputs, *modes;
    int max_width, max_height;

    g_assert (info != NULL);

    priv = info->screen->priv;

    if (!meta_dbus_display_config_call_get_resources_sync (priv->proxy,
							   &serial,
							   &crtcs,
							   &outputs,
							   &modes,
							   &max_width,
							   &max_height,
							   NULL,
							   error))
	return FALSE;

    fill_screen_info_from_resources (info, serial, crtcs, outputs,
				     modes, max_width, max_height);

    g_variant_unref (crtcs);
    g_variant_unref (outputs);
    g_variant_unref (modes);

    return TRUE;
}

static ScreenInfo *
screen_info_new (GnomeRRScreen *screen, GError **error)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);

    g_assert (screen != NULL);

    info->outputs = NULL;
    info->crtcs = NULL;
    info->modes = NULL;
    info->screen = screen;
    
    if (fill_out_screen_info (info, error))
    {
	return info;
    }
    else
    {
	screen_info_free (info);
	return NULL;
    }
}

static GnomeRROutput *
find_output_by_winsys_id (GnomeRROutput **haystack, glong winsys_id)
{
    guint i;

    for (i = 0; haystack[i] != NULL; i++)
    {
	if (haystack[i]->winsys_id == winsys_id)
	    return haystack[i];
    }
    return NULL;
}

static void
diff_outputs_and_emit_signals (ScreenInfo *old, ScreenInfo *new)
{
    guint i;
    gulong winsys_id_old, winsys_id_new;
    GnomeRROutput *output_old;
    GnomeRROutput *output_new;

    /* have any outputs been removed/disconnected */
    for (i = 0; old->outputs[i] != NULL; i++)
    {
        winsys_id_old = old->outputs[i]->winsys_id;
        output_new = find_output_by_winsys_id (new->outputs, winsys_id_old);
	if (output_new == NULL)
	{
	    g_signal_emit (G_OBJECT (new->screen),
			   screen_signals[SCREEN_OUTPUT_DISCONNECTED], 0,
			   old->outputs[i]);
	}
    }

    /* have any outputs been created/connected */
    for (i = 0; new->outputs[i] != NULL; i++)
    {
        winsys_id_new = new->outputs[i]->winsys_id;
        output_old = find_output_by_winsys_id (old->outputs, winsys_id_new);
	if (output_old == NULL)
	{
	    g_signal_emit (G_OBJECT (new->screen),
			   screen_signals[SCREEN_OUTPUT_CONNECTED], 0,
			   new->outputs[i]);
	}
    }
}

typedef enum {
    REFRESH_NONE = 0,
    REFRESH_IGNORE_SERIAL = 1,
    REFRESH_FORCE_CALLBACK = 2
} RefreshFlags;

static gboolean
screen_update (GnomeRRScreen *screen, RefreshFlags flags, GError **error)
{
    ScreenInfo *info;
    gboolean changed = FALSE;
    
    g_assert (screen != NULL);

    info = screen_info_new (screen, error);
    if (!info)
	    return FALSE;

    if ((flags & REFRESH_IGNORE_SERIAL) || info->serial != screen->priv->info->serial)
	    changed = TRUE;

    /* work out if any outputs have changed connected state */
    diff_outputs_and_emit_signals (screen->priv->info, info);

    screen_info_free (screen->priv->info);
    screen->priv->info = info;

    if (changed || (flags & REFRESH_FORCE_CALLBACK))
        g_signal_emit (G_OBJECT (screen), screen_signals[SCREEN_CHANGED], 0);
    
    return changed;
}

static void
screen_on_monitors_changed (MetaDBusDisplayConfig *proxy,
			    gpointer data)
{
    GnomeRRScreen *screen = data;

    screen_update (screen, REFRESH_FORCE_CALLBACK, NULL);
}

static void
name_owner_changed (GObject       *object,
		    GParamSpec    *pspec,
		    GnomeRRScreen *self)
{
    GError *error;
    char *new_name_owner;

    new_name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (object));
    if (new_name_owner == NULL)
	return;

    error = NULL;
    if (!screen_update (self, REFRESH_IGNORE_SERIAL | REFRESH_FORCE_CALLBACK, &error))
	g_warning ("Failed to refresh screen configuration after mutter was restarted: %s",
		   error->message);

    g_clear_error (&error);
    g_free (new_name_owner);
}

static void
power_save_mode_changed (GObject       *object,
                         GParamSpec    *pspec,
                         GnomeRRScreen *self)
{
        g_object_notify (G_OBJECT (self), "dpms-mode");
}

static gboolean
gnome_rr_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (initable);
    GnomeRRScreenPrivate *priv = self->priv;
    MetaDBusDisplayConfig *proxy;

    proxy = meta_dbus_display_config_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							     G_DBUS_PROXY_FLAGS_NONE,
							     "org.cinnamon.Muffin.DisplayConfig",
							     "/org/cinnamon/Muffin/DisplayConfig",
							     NULL, error);
    if (!proxy)
	return FALSE;

    priv->proxy = META_DBUS_DISPLAY_CONFIG (proxy);

    priv->info = screen_info_new (self, error);
    if (!priv->info)
	return FALSE;

    g_signal_connect_object (priv->proxy, "notify::g-name-owner",
			     G_CALLBACK (name_owner_changed), self, 0);
    g_signal_connect_object (priv->proxy, "monitors-changed",
			     G_CALLBACK (screen_on_monitors_changed), self, 0);
    g_signal_connect_object (priv->proxy, "notify::power-save-mode",
                             G_CALLBACK (power_save_mode_changed), self, 0);
    return TRUE;
}

static void
on_proxy_acquired (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    GTask *task = user_data;
    GnomeRRScreen *self = g_task_get_source_object (task);
    GnomeRRScreenPrivate *priv = self->priv;
    MetaDBusDisplayConfig *proxy;
    GError *error;

    error = NULL;
    proxy = meta_dbus_display_config_proxy_new_for_bus_finish (result, &error);
    if (!proxy)
	return g_task_return_error (task, error);

    priv->proxy = META_DBUS_DISPLAY_CONFIG (proxy);

    priv->info = screen_info_new (self, &error);
    if (!priv->info)
	return g_task_return_error (task, error);

    g_signal_connect_object (priv->proxy, "notify::g-name-owner",
			     G_CALLBACK (name_owner_changed), self, 0);
    g_signal_connect_object (priv->proxy, "monitors-changed",
			     G_CALLBACK (screen_on_monitors_changed), self, 0);
    g_signal_connect_object (priv->proxy, "notify::power-save-mode",
                             G_CALLBACK (power_save_mode_changed), self, 0);
    g_task_return_boolean (task, TRUE);
}

static void
on_name_appeared (GDBusConnection *connection,
                  const char      *name,
                  const char      *name_owner,
                  gpointer         user_data)
{
    GTask *task = user_data;
    GnomeRRScreen *self = g_task_get_source_object (task);
    GnomeRRScreenPrivate *priv = self->priv;

    meta_dbus_display_config_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.cinnamon.Muffin.DisplayConfig",
                                                "/org/cinnamon/Muffin/DisplayConfig",
                                                g_task_get_cancellable (task),
                                                on_proxy_acquired, g_object_ref (task));

    g_bus_unwatch_name (priv->init_name_watch_id);
}

static void
gnome_rr_screen_async_initable_init_async (GAsyncInitable      *initable,
                                           int                  io_priority,
                                           GCancellable        *canc,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (initable);
    GnomeRRScreenPrivate *priv = self->priv;
    GTask *task;

    task = g_task_new (self, canc, callback, user_data);

    priv->init_name_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                 "org.cinnamon.Muffin.DisplayConfig",
                                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                 on_name_appeared,
                                                 NULL,
                                                 task, g_object_unref);
}

static gboolean
gnome_rr_screen_async_initable_init_finish (GAsyncInitable    *initable,
                                            GAsyncResult      *result,
                                            GError           **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gnome_rr_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_rr_screen_initable_init;
}

static void
gnome_rr_screen_async_initable_init (GAsyncInitableIface *iface)
{
    iface->init_async = gnome_rr_screen_async_initable_init_async;
    iface->init_finish = gnome_rr_screen_async_initable_init_finish;
}

void
gnome_rr_screen_finalize (GObject *gobject)
{
    GnomeRRScreen *screen = GNOME_RR_SCREEN (gobject);

    if (screen->priv->info)
      screen_info_free (screen->priv->info);

    g_clear_object (&screen->priv->proxy);

    G_OBJECT_CLASS (gnome_rr_screen_parent_class)->finalize (gobject);
}

void
gnome_rr_screen_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (gobject);
    GnomeRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        priv->gdk_screen = g_value_get_object (value);
        return;
    case SCREEN_PROP_DPMS_MODE:
        gnome_rr_screen_set_dpms_mode (self, g_value_get_enum (value), NULL);
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
gnome_rr_screen_get_property (GObject *gobject, guint property_id, GValue *value, GParamSpec *property)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (gobject);
    GnomeRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        g_value_set_object (value, priv->gdk_screen);
        return;
    case SCREEN_PROP_DPMS_MODE: {
        GnomeRRDpmsMode mode;
        if (gnome_rr_screen_get_dpms_mode (self, &mode, NULL))
                g_value_set_enum (value, mode);
        else
                g_value_set_enum (value, GNOME_RR_DPMS_UNKNOWN);
        }
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
gnome_rr_screen_class_init (GnomeRRScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    gobject_class->set_property = gnome_rr_screen_set_property;
    gobject_class->get_property = gnome_rr_screen_get_property;
    gobject_class->finalize = gnome_rr_screen_finalize;

    g_object_class_install_property(
            gobject_class,
            SCREEN_PROP_GDK_SCREEN,
            g_param_spec_object (
                    "gdk-screen",
                    "GDK Screen",
                    "The GDK Screen represented by this GnomeRRScreen",
                    GDK_TYPE_SCREEN,
                    G_PARAM_READWRITE |
		    G_PARAM_CONSTRUCT_ONLY |
		    G_PARAM_STATIC_STRINGS)
            );

    g_object_class_install_property(
            gobject_class,
            SCREEN_PROP_DPMS_MODE,
            g_param_spec_enum (
                    "dpms-mode",
                    "DPMS Mode",
                    "The DPMS mode for this GnomeRRScreen",
                    GNOME_TYPE_RR_DPMS_MODE,
                    GNOME_RR_DPMS_UNKNOWN,
                    G_PARAM_READWRITE |
                    G_PARAM_STATIC_STRINGS)
            );

    screen_signals[SCREEN_CHANGED] = g_signal_new("changed",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GnomeRRScreenClass, changed),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
	    0);

    /**
     * GnomeRRScreen::output-connected:
     * @screen: the #GnomeRRScreen that emitted the signal
     * @output: the #GnomeRROutput that was connected
     *
     * This signal is emitted when a display device is connected to a
     * port, or a port is hotplugged with an active output. The latter
     * can happen if a laptop is docked, and the dock provides a new
     * active output.
     *
     * The @output value is not a #GObject. The returned @output value can
     * only assume to be valid during the emission of the signal (i.e. within
     * your signal handler only), as it may change later when the @screen
     * is modified due to an event from the X server, or due to another
     * place in the application modifying the @screen and the @output.
     * Therefore, deal with changes to the @output right in your signal
     * handler, instead of keeping the @output reference for an async or
     * idle function.
     **/
    screen_signals[SCREEN_OUTPUT_CONNECTED] = g_signal_new("output-connected",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GnomeRRScreenClass, output_connected),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    /**
     * GnomeRRScreen::output-disconnected:
     * @screen: the #GnomeRRScreen that emitted the signal
     * @output: the #GnomeRROutput that was disconnected
     *
     * This signal is emitted when a display device is disconnected from
     * a port, or a port output is hot-unplugged. The latter can happen
     * if a laptop is undocked, and the dock provided the output.
     *
     * The @output value is not a #GObject. The returned @output value can
     * only assume to be valid during the emission of the signal (i.e. within
     * your signal handler only), as it may change later when the @screen
     * is modified due to an event from the X server, or due to another
     * place in the application modifying the @screen and the @output.
     * Therefore, deal with changes to the @output right in your signal
     * handler, instead of keeping the @output reference for an async or
     * idle function.
     **/
    screen_signals[SCREEN_OUTPUT_DISCONNECTED] = g_signal_new("output-disconnected",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GnomeRRScreenClass, output_disconnected),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);
}

void
gnome_rr_screen_init (GnomeRRScreen *self)
{
    self->priv = gnome_rr_screen_get_instance_private (self);
}

/* Weak reference callback set in gnome_rr_screen_new(); we remove the GObject data from the GdkScreen. */
static void
rr_screen_weak_notify_cb (gpointer data, GObject *where_the_object_was)
{
    GdkScreen *screen = GDK_SCREEN (data);

    g_object_set_data (G_OBJECT (screen), "GnomeRRScreen", NULL);
}

/**
 * gnome_rr_screen_new:
 * @screen: the #GdkScreen on which to operate
 * @error: will be set if XRandR is not supported
 *
 * Creates a unique #GnomeRRScreen instance for the specified @screen.
 *
 * Returns: a unique #GnomeRRScreen instance, specific to the @screen, or NULL
 * if this could not be created, for instance if the driver does not support
 * Xrandr 1.2.  Each #GdkScreen thus has a single instance of #GnomeRRScreen.
 */
GnomeRRScreen *
gnome_rr_screen_new (GdkScreen *screen,
		     GError **error)
{
    GnomeRRScreen *rr_screen;

    g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    rr_screen = g_object_get_data (G_OBJECT (screen), "GnomeRRScreen");
    if (rr_screen)
	g_object_ref (rr_screen);
    else {
	rr_screen = g_initable_new (GNOME_TYPE_RR_SCREEN, NULL, error, "gdk-screen", screen, NULL);
	if (rr_screen) {
	    g_object_set_data (G_OBJECT (screen), "GnomeRRScreen", rr_screen);
	    g_object_weak_ref (G_OBJECT (rr_screen), rr_screen_weak_notify_cb, screen);
	}
    }

    return rr_screen;
}

void
gnome_rr_screen_new_async (GdkScreen           *screen,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    g_return_if_fail (GDK_IS_SCREEN (screen));

    g_async_initable_new_async (GNOME_TYPE_RR_SCREEN, G_PRIORITY_DEFAULT,
                                NULL, callback, user_data,
                                "gdk-screen", screen, NULL);
}

GnomeRRScreen *
gnome_rr_screen_new_finish (GAsyncResult  *result,
                            GError       **error)
{
    GObject *source_object;
    GnomeRRScreen *screen;

    source_object = g_async_result_get_source_object (result);
    screen = GNOME_RR_SCREEN (g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), result, error));

    g_object_unref (source_object);
    return screen;
}

/**
 * gnome_rr_screen_get_ranges:
 * @screen: a #GnomeRRScreen
 * @min_width: (out): the minimum width
 * @max_width: (out): the maximum width
 * @min_height: (out): the minimum height
 * @max_height: (out): the maximum height
 *
 * Get the ranges of the screen
 */
void
gnome_rr_screen_get_ranges (GnomeRRScreen *screen,
			    int	          *min_width,
			    int	          *max_width,
			    int           *min_height,
			    int	          *max_height)
{
    GnomeRRScreenPrivate *priv;

    g_return_if_fail (GNOME_IS_RR_SCREEN (screen));

    priv = screen->priv;
    
    if (min_width)
	*min_width = priv->info->min_width;
    
    if (max_width)
	*max_width = priv->info->max_width;
    
    if (min_height)
	*min_height = priv->info->min_height;
    
    if (max_height)
	*max_height = priv->info->max_height;
}

/**
 * gnome_rr_screen_refresh:
 * @screen: a #GnomeRRScreen
 * @error: location to store error, or %NULL
 *
 * Refreshes the screen configuration, and calls the screen's callback if it
 * exists and if the screen's configuration changed.
 *
 * Return value: TRUE if the screen's configuration changed; otherwise, the
 * function returns FALSE and a NULL error if the configuration didn't change,
 * or FALSE and a non-NULL error if there was an error while refreshing the
 * configuration.
 */
gboolean
gnome_rr_screen_refresh (GnomeRRScreen *screen,
			 GError       **error)
{
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    return screen_update (screen, REFRESH_NONE, error);
}

/**
 * gnome_rr_screen_get_dpms_mode:
 * @mode: (out): The current #GnomeRRDpmsMode of this screen
 **/
gboolean
gnome_rr_screen_get_dpms_mode (GnomeRRScreen    *screen,
                               GnomeRRDpmsMode  *mode,
                               GError          **error)
{
    MetaPowerSave power_save;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);

    power_save = meta_dbus_display_config_get_power_save_mode (screen->priv->proxy);
    switch (power_save) {
    case META_POWER_SAVE_UNKNOWN:
        g_set_error_literal (error,
                             GNOME_RR_ERROR,
                             GNOME_RR_ERROR_NO_DPMS_EXTENSION,
                             "Display is not DPMS capable");
        return FALSE;
    case META_POWER_SAVE_ON:
        *mode = GNOME_RR_DPMS_ON;
        break;
    case META_POWER_SAVE_STANDBY:
        *mode = GNOME_RR_DPMS_STANDBY;
        break;
    case META_POWER_SAVE_SUSPEND:
        *mode = GNOME_RR_DPMS_SUSPEND;
        break;
    case META_POWER_SAVE_OFF:
        *mode = GNOME_RR_DPMS_OFF;
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    return TRUE;
}

/**
 * gnome_rr_screen_set_dpms_mode:
 *
 * This method also disables the DPMS timeouts.
 **/
gboolean
gnome_rr_screen_set_dpms_mode (GnomeRRScreen    *screen,
                               GnomeRRDpmsMode   mode,
                               GError          **error)
{
    MetaPowerSave power_save;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    switch (mode) {
    case GNOME_RR_DPMS_UNKNOWN:
        power_save = META_POWER_SAVE_UNKNOWN;
        break;
    case GNOME_RR_DPMS_ON:
        power_save = META_POWER_SAVE_ON;
        break;
    case GNOME_RR_DPMS_STANDBY:
	power_save = META_POWER_SAVE_STANDBY;
        break;
    case GNOME_RR_DPMS_SUSPEND:
	power_save = META_POWER_SAVE_SUSPEND;
        break;
    case GNOME_RR_DPMS_OFF:
	power_save = META_POWER_SAVE_OFF;
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    meta_dbus_display_config_set_power_save_mode (screen->priv->proxy, power_save);

    return TRUE;
}

/**
 * gnome_rr_screen_list_modes:
 *
 * List available XRandR modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRMode **
gnome_rr_screen_list_modes (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->modes;
}

/**
 * gnome_rr_screen_list_clone_modes:
 *
 * List available XRandR clone modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRMode **
gnome_rr_screen_list_clone_modes   (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->clone_modes;
}

/**
 * gnome_rr_screen_list_crtcs:
 *
 * List all CRTCs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRCrtc **
gnome_rr_screen_list_crtcs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->crtcs;
}

/**
 * gnome_rr_screen_list_outputs:
 *
 * List all outputs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRROutput **
gnome_rr_screen_list_outputs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->outputs;
}

/**
 * gnome_rr_screen_get_crtc_by_id:
 *
 * Returns: (transfer none): the CRTC identified by @id
 */
GnomeRRCrtc *
gnome_rr_screen_get_crtc_by_id (GnomeRRScreen *screen,
				guint32        id)
{
    GnomeRRCrtc **crtcs;
    int i;
    
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    crtcs = screen->priv->info->crtcs;
    
    for (i = 0; crtcs[i] != NULL; ++i)
    {
	if (crtcs[i]->id == id)
	    return crtcs[i];
    }
    
    return NULL;
}

/**
 * gnome_rr_screen_get_output_by_id:
 *
 * Returns: (transfer none): the output identified by @id
 */
GnomeRROutput *
gnome_rr_screen_get_output_by_id (GnomeRRScreen *screen,
				  guint32        id)
{
    GnomeRROutput **outputs;
    int i;
    
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    outputs = screen->priv->info->outputs;

    for (i = 0; outputs[i] != NULL; ++i)
    {
	if (outputs[i]->id == id)
	    return outputs[i];
    }
    
    return NULL;
}

/* GnomeRROutput */
static GnomeRROutput *
output_new (ScreenInfo *info, guint id)
{
    GnomeRROutput *output = g_slice_new0 (GnomeRROutput);
    
    output->id = id;
    output->info = info;
    
    return output;
}

static void
append_output_array (GnomeRROutput ***array, GnomeRROutput *output)
{
    unsigned i;

    for (i = 0; (*array)[i]; i++);

    *array = g_renew (GnomeRROutput *, *array, i + 2);

    (*array)[i] = output;
    (*array)[i + 1] = NULL;
}

static void
output_initialize (GnomeRROutput *output, GVariant *info)
{
    GPtrArray *a;
    GVariantIter *crtcs, *clones, *modes;
    GVariant *properties, *edid, *tile;
    gint32 current_crtc_id;
    guint32 id;

    g_variant_get (info, META_OUTPUT_STRUCT,
		   &output->id, &output->winsys_id,
		   &current_crtc_id, &crtcs,
		   &output->name,
		   &modes, &clones, &properties);

    /* Possible crtcs */
    a = g_ptr_array_new ();
    while (g_variant_iter_loop (crtcs, "u", &id))
    {
	GnomeRRCrtc *crtc = crtc_by_id (output->info, id);
	
	if (!crtc)
	    continue;

	g_ptr_array_add (a, crtc);

	if (current_crtc_id != -1 && crtc->id == (guint32) current_crtc_id)
	{
	    output->current_crtc = crtc;
	    append_output_array (&crtc->current_outputs, output);
	}

	append_output_array (&crtc->possible_outputs, output);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);
    g_variant_iter_free (crtcs);

    /* Clones */
    a = g_ptr_array_new ();
    while (g_variant_iter_loop (clones, "u", &id))
    {
	GnomeRROutput *gnome_rr_output = gnome_rr_output_by_id (output->info, id);
	
	if (gnome_rr_output)
	    g_ptr_array_add (a, gnome_rr_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (GnomeRROutput **)g_ptr_array_free (a, FALSE);
    g_variant_iter_free (clones);
    
    /* Modes */
    a = g_ptr_array_new ();
    while (g_variant_iter_loop (modes, "u", &id))
    {
	GnomeRRMode *mode = mode_by_id (output->info, id);
	
	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);
    g_variant_iter_free (modes);

    g_variant_lookup (properties, "vendor", "s", &output->vendor);
    g_variant_lookup (properties, "product", "s", &output->product);
    g_variant_lookup (properties, "serial", "s", &output->serial);
    g_variant_lookup (properties, "width-mm", "i", &output->width_mm);
    g_variant_lookup (properties, "height-mm", "i", &output->height_mm);
    g_variant_lookup (properties, "display-name", "s", &output->display_name);
    g_variant_lookup (properties, "connector-type", "s", &output->connector_type);
    g_variant_lookup (properties, "backlight", "i", &output->backlight);
    g_variant_lookup (properties, "min-backlight-step", "i", &output->min_backlight_step);
    g_variant_lookup (properties, "primary", "b", &output->is_primary);
    g_variant_lookup (properties, "presentation", "b", &output->is_presentation);
    g_variant_lookup (properties, "underscanning", "b", &output->is_underscanning);
    g_variant_lookup (properties, "supports-underscanning", "b", &output->supports_underscanning);

    if ((edid = g_variant_lookup_value (properties, "edid", G_VARIANT_TYPE ("ay"))))
      {
	output->edid = g_variant_get_data_as_bytes (edid);
	g_variant_unref (edid);
      }
    else
      g_variant_lookup (properties, "edid-file", "s", &output->edid_file);

    if ((tile = g_variant_lookup_value (properties, "tile", G_VARIANT_TYPE ("(uuuuuuuu)"))))
      {
	g_variant_get (tile, "(uuuuuuuu)",
		       &output->tile_info.group_id, &output->tile_info.flags,
		       &output->tile_info.max_horiz_tiles, &output->tile_info.max_vert_tiles,
		       &output->tile_info.loc_horiz, &output->tile_info.loc_vert,
		       &output->tile_info.width, &output->tile_info.height);
	g_variant_unref (tile);
      }
    else
      memset(&output->tile_info, 0, sizeof(output->tile_info));

    if (output->is_primary)
	output->info->primary = output;

    g_variant_unref (properties);
}

static GnomeRROutput*
output_copy (const GnomeRROutput *from)
{
    GPtrArray *array;
    GnomeRRCrtc **p_crtc;
    GnomeRROutput **p_output;
    GnomeRRMode **p_mode;
    GnomeRROutput *output = g_slice_new0 (GnomeRROutput);

    output->id = from->id;
    output->info = from->info;
    output->name = g_strdup (from->name);
    output->display_name = g_strdup (from->display_name);
    output->connector_type = g_strdup (from->connector_type);
    output->vendor = g_strdup (from->vendor);
    output->product = g_strdup (from->product);
    output->serial = g_strdup (from->serial);
    output->current_crtc = from->current_crtc;
    output->backlight = from->backlight;
    if (from->edid)
      output->edid = g_bytes_ref (from->edid);
    output->edid_file = g_strdup (from->edid_file);

    output->is_primary = from->is_primary;
    output->is_presentation = from->is_presentation;

    array = g_ptr_array_new ();
    for (p_crtc = from->possible_crtcs; *p_crtc != NULL; p_crtc++)
    {
        g_ptr_array_add (array, *p_crtc);
    }
    output->possible_crtcs = (GnomeRRCrtc**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->clones; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    output->clones = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_mode = from->modes; *p_mode != NULL; p_mode++)
    {
        g_ptr_array_add (array, *p_mode);
    }
    output->modes = (GnomeRRMode**) g_ptr_array_free (array, FALSE);

    return output;
}

static void
output_free (GnomeRROutput *output)
{
    g_free (output->clones);
    g_free (output->modes);
    g_free (output->possible_crtcs);
    g_free (output->name);
    g_free (output->vendor);
    g_free (output->product);
    g_free (output->serial);
    g_free (output->display_name);
    g_free (output->connector_type);
    g_free (output->edid_file);
    if (output->edid)
      g_bytes_unref (output->edid);
    g_slice_free (GnomeRROutput, output);
}

guint32
gnome_rr_output_get_id (GnomeRROutput *output)
{
    g_assert(output != NULL);
    
    return output->id;
}

const guint8 *
gnome_rr_output_get_edid_data (GnomeRROutput *output,
			       gsize         *size)
{
  if (output->edid)
    return g_bytes_get_data (output->edid, size);

  if (output->edid_file)
    {
      GMappedFile *mmap;

      mmap = g_mapped_file_new (output->edid_file, FALSE, NULL);

      if (mmap)
	{
	  output->edid = g_mapped_file_get_bytes (mmap);

	  g_mapped_file_unref (mmap);

	  return g_bytes_get_data (output->edid, size);
	}
    }

  return NULL;
}

/**
 * gnome_rr_output_get_ids_from_edid:
 * @output: a #GnomeRROutput
 * @vendor: (out) (allow-none):
 * @product: (out) (allow-none):
 * @serial: (out) (allow-none):
 */
void
gnome_rr_output_get_ids_from_edid (GnomeRROutput         *output,
                                   char                 **vendor,
                                   char                 **product,
                                   char                 **serial)
{
    g_return_if_fail (output != NULL);

    *vendor = g_strdup (output->vendor);
    *product = g_strdup (output->product);
    *serial = g_strdup (output->serial);
}

/**
 * gnome_rr_output_get_physical_size:
 * @output: a #GnomeRROutput
 * @width_mm: (out) (allow-none):
 * @height_mm: (out) (allow-none):
 */
void
gnome_rr_output_get_physical_size (GnomeRROutput *output,
				   int           *width_mm,
				   int           *height_mm)
{
    g_return_if_fail (output != NULL);

    if (width_mm)
	*width_mm = output->width_mm;
    if (height_mm)
	*height_mm = output->height_mm;
}

const char *
gnome_rr_output_get_display_name (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->display_name;
}

/**
 * gnome_rr_output_get_backlight:
 *
 * Returns: The currently set backlight brightness
 */
int
gnome_rr_output_get_backlight (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);

    return output->backlight;
}

/**
 * gnome_rr_output_get_min_backlight_step:
 *
 * Returns: The minimum backlight step available in percent
 */
int
gnome_rr_output_get_min_backlight_step (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);

    return output->min_backlight_step;
}

/**
 * gnome_rr_output_set_backlight:
 * @value: the absolute value which is 0 >= this <= 100
 *
 * Returns: %TRUE for success
 */
gboolean
gnome_rr_output_set_backlight (GnomeRROutput *output, gint value, GError **error)
{
    g_return_val_if_fail (output != NULL, FALSE);

    return meta_dbus_display_config_call_change_backlight_sync (output->info->screen->priv->proxy,
								output->info->serial,
								output->id, value,
								&output->backlight,
								NULL, error);
}

/**
 * gnome_rr_screen_get_output_by_name:
 *
 * Returns: (transfer none): the output identified by @name
 */
GnomeRROutput *
gnome_rr_screen_get_output_by_name (GnomeRRScreen *screen,
				    const char    *name)
{
    int i;
    
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    for (i = 0; screen->priv->info->outputs[i] != NULL; ++i)
    {
	GnomeRROutput *output = screen->priv->info->outputs[i];
	
	if (strcmp (output->name, name) == 0)
	    return output;
    }
    
    return NULL;
}

/**
 * gnome_rr_output_get_crtc:
 * @output: a #GnomeRROutput
 * Returns: (transfer none):
 */
GnomeRRCrtc *
gnome_rr_output_get_crtc (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    
    return output->current_crtc;
}

/**
 * gnome_rr_output_get_possible_crtcs:
 * @output: a #GnomeRROutput
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRCrtc **
gnome_rr_output_get_possible_crtcs (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->possible_crtcs;
}

gboolean
_gnome_rr_output_connector_type_is_builtin_display (const char *connector_type)
{
    if (!connector_type)
        return FALSE;

    if (strcmp (connector_type, "LVDS") == 0 ||
	strcmp (connector_type, "eDP") == 0  ||
	strcmp (connector_type, "DSI") == 0)
        return TRUE;

    return FALSE;
}

gboolean
gnome_rr_output_is_builtin_display (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);

    return _gnome_rr_output_connector_type_is_builtin_display (output->connector_type);
}

/**
 * gnome_rr_output_get_current_mode:
 * @output: a #GnomeRROutput
 * Returns: (transfer none): the current mode of this output
 */
GnomeRRMode *
gnome_rr_output_get_current_mode (GnomeRROutput *output)
{
    GnomeRRCrtc *crtc;
    GnomeRRMode *mode;
    g_return_val_if_fail (output != NULL, NULL);
    
    if ((crtc = gnome_rr_output_get_crtc (output)))
    {
	int total_w, total_h, tile_w, tile_h;
	mode = gnome_rr_crtc_get_current_mode (crtc);

	if (_gnome_rr_output_get_tiled_display_size (output, &tile_w, &tile_h, &total_w, &total_h))
	{
	    if (mode->width == tile_w &&
		mode->height == tile_h) {
		if (output->modes[0]->tiled)
		    return output->modes[0];
	    }
	}
	return gnome_rr_crtc_get_current_mode (crtc);
    }
    return NULL;
}

/**
 * gnome_rr_output_is_connected:
 * @output: a #GnomeRROutput
 * Returns: If the output is connected
 */
gboolean
gnome_rr_output_is_connected (GnomeRROutput *output)
{
    return gnome_rr_output_get_current_mode (output) != NULL;
}

/**
 * gnome_rr_output_get_position:
 * @output: a #GnomeRROutput
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 */
void
gnome_rr_output_get_position (GnomeRROutput   *output,
			      int             *x,
			      int             *y)
{
    GnomeRRCrtc *crtc;
    
    g_return_if_fail (output != NULL);
    
    if ((crtc = gnome_rr_output_get_crtc (output)))
	gnome_rr_crtc_get_position (crtc, x, y);
}

const char *
gnome_rr_output_get_name (GnomeRROutput *output)
{
    g_assert (output != NULL);
    return output->name;
}

/**
 * gnome_rr_output_get_preferred_mode:
 * @output: a #GnomeRROutput
 * Returns: (transfer none):
 */
GnomeRRMode *
gnome_rr_output_get_preferred_mode (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes[0];
}

/**
 * gnome_rr_output_list_modes:
 * @output: a #GnomeRROutput
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRMode **
gnome_rr_output_list_modes (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

gboolean
gnome_rr_output_supports_mode (GnomeRROutput *output,
			       GnomeRRMode   *mode)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);
    
    for (i = 0; output->modes[i] != NULL; ++i)
    {
	if (output->modes[i] == mode)
	    return TRUE;
    }
    
    return FALSE;
}

gboolean
gnome_rr_output_can_clone (GnomeRROutput *output,
			   GnomeRROutput *clone)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (clone != NULL, FALSE);
    
    for (i = 0; output->clones[i] != NULL; ++i)
    {
	if (output->clones[i] == clone)
	    return TRUE;
    }
    
    return FALSE;
}

gboolean
gnome_rr_output_get_is_primary (GnomeRROutput *output)
{
    return output->is_primary;
}

/* GnomeRRCrtc */
static const GnomeRRRotation rotation_map[] =
{
    GNOME_RR_ROTATION_0,
    GNOME_RR_ROTATION_90,
    GNOME_RR_ROTATION_180,
    GNOME_RR_ROTATION_270,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_0,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_90,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_180,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_270,
};

static GnomeRRRotation
gnome_rr_rotation_from_transform (enum wl_output_transform transform)
{
    return rotation_map[transform];
}

/**
 * gnome_rr_crtc_get_current_mode:
 * @crtc: a #GnomeRRCrtc
 * Returns: (transfer none): the current mode of this crtc
 */
GnomeRRMode *
gnome_rr_crtc_get_current_mode (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);
    
    return crtc->current_mode;
}

guint32
gnome_rr_crtc_get_id (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, 0);
    
    return crtc->id;
}

gboolean
gnome_rr_crtc_can_drive_output (GnomeRRCrtc   *crtc,
				GnomeRROutput *output)
{
    int i;
    
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (output != NULL, FALSE);
    
    for (i = 0; crtc->possible_outputs[i] != NULL; ++i)
    {
	if (crtc->possible_outputs[i] == output)
	    return TRUE;
    }
    
    return FALSE;
}

/**
 * gnome_rr_crtc_get_position:
 * @crtc: a #GnomeRRCrtc
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 */
void
gnome_rr_crtc_get_position (GnomeRRCrtc *crtc,
			    int         *x,
			    int         *y)
{
    g_return_if_fail (crtc != NULL);
    
    if (x)
	*x = crtc->x;
    
    if (y)
	*y = crtc->y;
}

GnomeRRRotation
gnome_rr_crtc_get_current_rotation (GnomeRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return gnome_rr_rotation_from_transform (crtc->transform);
}

static GnomeRRRotation
gnome_rr_rotation_from_all_transforms (int all_transforms)
{
    GnomeRRRotation ret = all_transforms & 0xF;

    if (all_transforms & (1 << WL_OUTPUT_TRANSFORM_FLIPPED))
	ret |= GNOME_RR_REFLECT_X;

    if (all_transforms & (1 << WL_OUTPUT_TRANSFORM_FLIPPED_180))
	ret |= GNOME_RR_REFLECT_Y;

    return ret;
}

GnomeRRRotation
gnome_rr_crtc_get_rotations (GnomeRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return gnome_rr_rotation_from_all_transforms (crtc->all_transforms);
}

gboolean
gnome_rr_crtc_supports_rotation (GnomeRRCrtc *   crtc,
				 GnomeRRRotation rotation)
{
    g_return_val_if_fail (crtc != NULL, FALSE);
    return (gnome_rr_rotation_from_all_transforms (crtc->all_transforms) & rotation);
}

static GnomeRRCrtc *
crtc_new (ScreenInfo *info, guint id)
{
    GnomeRRCrtc *crtc = g_slice_new0 (GnomeRRCrtc);
    
    crtc->id = id;
    crtc->info = info;
    crtc->current_outputs = g_new0 (GnomeRROutput *, 1);
    crtc->possible_outputs = g_new0 (GnomeRROutput *, 1);
    
    return crtc;
}

static GnomeRRCrtc *
crtc_copy (const GnomeRRCrtc *from)
{
    GnomeRROutput **p_output;
    GPtrArray *array;
    GnomeRRCrtc *to = g_slice_new0 (GnomeRRCrtc);

    to->info = from->info;
    to->id = from->id;
    to->current_mode = from->current_mode;
    to->x = from->x;
    to->y = from->y;
    to->transform = from->transform;
    to->all_transforms = from->all_transforms;
    to->gamma_size = from->gamma_size;

    array = g_ptr_array_new ();
    for (p_output = from->current_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->current_outputs = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->possible_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->possible_outputs = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    return to;
}

static void
crtc_initialize (GnomeRRCrtc *crtc, GVariant *info)
{
    GVariantIter *all_transforms;
    int current_mode_id;
    guint transform;

    g_variant_get (info, META_CRTC_STRUCT,
		   &crtc->id, &crtc->winsys_id,
		   &crtc->x, &crtc->y,
		   NULL, NULL,
		   &current_mode_id,
		   &crtc->transform, &all_transforms,
		   NULL);

    if (current_mode_id >= 0)
      crtc->current_mode = mode_by_id (crtc->info, current_mode_id);
    
    while (g_variant_iter_loop (all_transforms, "u", &transform))
	crtc->all_transforms |= 1 << transform;
    g_variant_iter_free (all_transforms);
}

static void
crtc_free (GnomeRRCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_slice_free (GnomeRRCrtc, crtc);
}

/* GnomeRRMode */
static GnomeRRMode *
mode_new (ScreenInfo *info, guint id)
{
    GnomeRRMode *mode = g_slice_new0 (GnomeRRMode);
    
    mode->id = id;
    mode->info = info;
    
    return mode;
}

guint32
gnome_rr_mode_get_id (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->id;
}

guint
gnome_rr_mode_get_width (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

int
gnome_rr_mode_get_freq (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->freq) / 1000;
}

double
gnome_rr_mode_get_freq_f (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0.0);
    return (mode->freq) / 1000.0;
}

guint
gnome_rr_mode_get_height (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

/**
 * gnome_rr_mode_get_is_tiled:
 * @mode: a #GnomeRRMode
 *
 * Returns TRUE if this mode is a tiled
 * mode created for span a tiled monitor.
 */
gboolean
gnome_rr_mode_get_is_tiled (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, FALSE);
    return mode->tiled;
}

gboolean
gnome_rr_mode_get_is_interlaced (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->flags & DRM_MODE_FLAG_INTERLACE) != 0;
}

static void
mode_initialize (GnomeRRMode *mode, GVariant *info)
{
    gdouble frequency;

    g_variant_get (info, META_MONITOR_MODE_STRUCT,
		   &mode->id, &mode->winsys_id,
		   &mode->width, &mode->height,
		   &frequency, &mode->flags);
    
    mode->freq = frequency * 1000;
}

static GnomeRRMode *
mode_copy (const GnomeRRMode *from)
{
    GnomeRRMode *to = g_slice_new0 (GnomeRRMode);

    to->id = from->id;
    to->info = from->info;
    to->width = from->width;
    to->height = from->height;
    to->freq = from->freq;

    return to;
}

static void
mode_free (GnomeRRMode *mode)
{
    g_slice_free (GnomeRRMode, mode);
}

gboolean
_gnome_rr_screen_apply_configuration (GnomeRRScreen  *screen,
				      gboolean        persistent,
				      GVariant       *crtcs,
				      GVariant       *outputs,
				      GError        **error)
{
  return meta_dbus_display_config_call_apply_configuration_sync (screen->priv->proxy,
								 screen->priv->info->serial,
								 persistent,
								 crtcs, outputs,
								 NULL, error);
}

gboolean
gnome_rr_crtc_set_gamma (GnomeRRCrtc    *crtc,
			 int             size,
			 unsigned short *red,
			 unsigned short *green,
			 unsigned short *blue)
{
  GBytes *red_bytes, *green_bytes, *blue_bytes;
  GVariant *red_v, *green_v, *blue_v;
  gboolean ok;

  red_bytes = g_bytes_new (red, size * sizeof (unsigned short));
  green_bytes = g_bytes_new (green, size * sizeof (unsigned short));
  blue_bytes = g_bytes_new (blue, size * sizeof (unsigned short));

  red_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"),
				    red_bytes, TRUE);
  green_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"),
				      green_bytes, TRUE);
  blue_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"),
				     blue_bytes, TRUE);

  ok = meta_dbus_display_config_call_set_crtc_gamma_sync (crtc->info->screen->priv->proxy,
							  crtc->info->serial,
							  crtc->id,
							  red_v,
							  green_v,
							  blue_v,
							  NULL, NULL);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);
  /* The variant above are floating, no need to free them */

  return ok;
}

/**
 * gnome_rr_crtc_get_gamma:
 * @crtc: a #GnomeRRCrtc
 * @size:
 * @red: (out): the minimum width
 * @green: (out): the maximum width
 * @blue: (out): the minimum height
 *
 * Returns: %TRUE for success
 */
gboolean
gnome_rr_crtc_get_gamma (GnomeRRCrtc     *crtc,
			 int             *size,
			 unsigned short **red,
			 unsigned short **green,
			 unsigned short **blue)
{
  GBytes *red_bytes, *green_bytes, *blue_bytes;
  GVariant *red_v, *green_v, *blue_v;
  gboolean ok;
  gsize dummy;

  ok = meta_dbus_display_config_call_get_crtc_gamma_sync (crtc->info->screen->priv->proxy,
							  crtc->info->serial,
							  crtc->id,
							  &red_v,
							  &green_v,
							  &blue_v,
							  NULL, NULL);
  if (!ok)
    return FALSE;

  red_bytes = g_variant_get_data_as_bytes (red_v);
  green_bytes = g_variant_get_data_as_bytes (green_v);
  blue_bytes = g_variant_get_data_as_bytes (blue_v);

  /* Unref the variant early so that the bytes hold the only reference to
     the data and we don't need to copy
  */
  g_variant_unref (red_v);
  g_variant_unref (green_v);
  g_variant_unref (blue_v);

  if (size)
    *size = g_bytes_get_size (red_bytes) / sizeof (unsigned short);

  if (red)
    *red = g_bytes_unref_to_data (red_bytes, &dummy);
  else
    g_bytes_unref (red_bytes);
  if (green)
    *green = g_bytes_unref_to_data (green_bytes, &dummy);
  else
    g_bytes_unref (green_bytes);
  if (blue)
    *blue = g_bytes_unref_to_data (blue_bytes, &dummy);
  else
    g_bytes_unref (blue_bytes);

  return TRUE;
}

gboolean
gnome_rr_output_get_is_underscanning (GnomeRROutput *output)
{
    g_assert(output != NULL);
    return output->is_underscanning;
}

gboolean
gnome_rr_output_supports_underscanning (GnomeRROutput *output)
{
    g_assert (output != NULL);
    return output->supports_underscanning;
}

const char *
_gnome_rr_output_get_connector_type (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->connector_type;
}

gboolean
_gnome_rr_output_get_tile_info (GnomeRROutput *output,
				GnomeRRTile *tile)
{
    if (output->tile_info.group_id == UNDEFINED_GROUP_ID)
        return FALSE;

    if (!tile)
        return FALSE;

    *tile = output->tile_info;
    return TRUE;
}

GType
gnome_rr_dpms_mode_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { GNOME_RR_DPMS_ON, "GNOME_RR_DPMS_ON", "on" },
      { GNOME_RR_DPMS_STANDBY, "GNOME_RR_DPMS_STANDBY", "standby" },
      { GNOME_RR_DPMS_SUSPEND, "GNOME_RR_DPMS_SUSPEND", "suspend" },
      { GNOME_RR_DPMS_OFF, "GNOME_RR_DPMS_OFF", "off" },
      { GNOME_RR_DPMS_UNKNOWN, "GNOME_RR_DPMS_UNKNOWN", "unknown" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("GnomeRRDpmsModeType", values);
  }
  return etype;
}
