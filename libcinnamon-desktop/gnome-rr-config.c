/* gnome-rr-config.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2007, 2008, 2013 Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "gnome-rr-config.h"

#include "gnome-rr-private.h"

#define CONFIG_INTENDED_BASENAME "cinnamon-monitors.xml"
#define CONFIG_BACKUP_BASENAME "cinnamon-monitors.xml.backup"

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning */
#define DPI_FALLBACK 96.0

/* In version 0 of the config file format, we had several <configuration>
 * toplevel elements and no explicit version number.  So, the filed looked
 * like
 *
 *   <configuration>
 *     ...
 *   </configuration>
 *   <configuration>
 *     ...
 *   </configuration>
 *
 * Since version 1 of the config file, the file has a toplevel <monitors>
 * element to group all the configurations.  That element has a "version"
 * attribute which is an integer. So, the file looks like this:
 *
 *   <monitors version="1">
 *     <configuration>
 *       ...
 *     </configuration>
 *     <configuration>
 *       ...
 *     </configuration>
 *   </monitors>
 */

typedef struct CrtcAssignment CrtcAssignment;

static gboolean         crtc_assignment_apply (CrtcAssignment   *assign,
					       gboolean          persistent,
					       GError          **error);
static CrtcAssignment  *crtc_assignment_new   (GnomeRRScreen      *screen,
					       GnomeRROutputInfo **outputs,
					       GError            **error);
static void             crtc_assignment_free  (CrtcAssignment   *assign);

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (GnomeRRConfig, gnome_rr_config, G_TYPE_OBJECT)

static void
gnome_rr_config_init (GnomeRRConfig *self)
{
    self->priv = gnome_rr_config_get_instance_private (self);

    self->priv->clone = FALSE;
    self->priv->screen = NULL;
    self->priv->outputs = NULL;
}

static void
gnome_rr_config_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    switch (property_id) {
	case PROP_SCREEN:
	    self->priv->screen = g_value_dup_object (value);
	    return;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
    }
}

static void
gnome_rr_config_finalize (GObject *gobject)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    if (self->priv->screen)
	g_object_unref (self->priv->screen);

    if (self->priv->outputs) {
	int i;

        for (i = 0; self->priv->outputs[i] != NULL; i++) {
	    GnomeRROutputInfo *output = self->priv->outputs[i];
	    g_object_unref (output);
	}
	g_free (self->priv->outputs);
    }

    G_OBJECT_CLASS (gnome_rr_config_parent_class)->finalize (gobject);
}

gboolean
gnome_rr_config_load_current (GnomeRRConfig *config, GError **error)
{
    GPtrArray *a;
    GnomeRROutput **rr_outputs;
    int i;
    int clone_width = -1;
    int clone_height = -1;
    int last_x;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (config), FALSE);

    a = g_ptr_array_new ();
    rr_outputs = gnome_rr_screen_list_outputs (config->priv->screen);

    config->priv->clone = FALSE;
    
    for (i = 0; rr_outputs[i] != NULL; ++i)
    {
	GnomeRROutput *rr_output = rr_outputs[i];
	GnomeRROutputInfo *output = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	GnomeRRMode *mode = NULL;
	GnomeRRCrtc *crtc;

	output->priv->name = g_strdup (gnome_rr_output_get_name (rr_output));
	output->priv->connected = TRUE;
	output->priv->display_name = g_strdup (gnome_rr_output_get_display_name (rr_output));
	output->priv->connector_type = g_strdup (_gnome_rr_output_get_connector_type (rr_output));
	output->priv->config = config;
	output->priv->is_tiled = _gnome_rr_output_get_tile_info (rr_output,
								 &output->priv->tile);
	if (output->priv->is_tiled)
	{
	    _gnome_rr_output_get_tiled_display_size (rr_output, NULL, NULL,
						     &output->priv->total_tiled_width,
						     &output->priv->total_tiled_height);
	}

	if (!output->priv->connected)
	{
	    output->priv->x = -1;
	    output->priv->y = -1;
	    output->priv->width = -1;
	    output->priv->height = -1;
	    output->priv->rate = -1;
	}
	else
	{
	    gnome_rr_output_get_ids_from_edid (rr_output,
					       &output->priv->vendor,
					       &output->priv->product,
					       &output->priv->serial);
		
	    crtc = gnome_rr_output_get_crtc (rr_output);
	    mode = crtc ? gnome_rr_crtc_get_current_mode (crtc) : NULL;

	    if (crtc && mode)
	    {
		output->priv->on = TRUE;

		gnome_rr_crtc_get_position (crtc, &output->priv->x, &output->priv->y);
		output->priv->width = gnome_rr_mode_get_width (mode);
		output->priv->height = gnome_rr_mode_get_height (mode);
		output->priv->rate = gnome_rr_mode_get_freq (mode);
		output->priv->rotation = gnome_rr_crtc_get_current_rotation (crtc);
                output->priv->available_rotations = gnome_rr_crtc_get_rotations (crtc);

		if (output->priv->x == 0 && output->priv->y == 0) {
		    if (clone_width == -1) {
			clone_width = output->priv->width;
			clone_height = output->priv->height;
		    } else if (clone_width == output->priv->width &&
			       clone_height == output->priv->height) {
			config->priv->clone = TRUE;
		    }
		}
	    }
	    else
	    {
		output->priv->on = FALSE;
		config->priv->clone = FALSE;
	    }

	    /* Get preferred size for the monitor */
	    mode = gnome_rr_output_get_preferred_mode (rr_output);
	    output->priv->pref_width = gnome_rr_mode_get_width (mode);
	    output->priv->pref_height = gnome_rr_mode_get_height (mode);
	}

        output->priv->primary = gnome_rr_output_get_is_primary (rr_output);
        output->priv->underscanning = gnome_rr_output_get_is_underscanning (rr_output);

	g_ptr_array_add (a, output);
    }

    g_ptr_array_add (a, NULL);
    
    config->priv->outputs = (GnomeRROutputInfo **)g_ptr_array_free (a, FALSE);

    /* Walk the outputs computing the right-most edge of all
     * lit-up displays
     */
    last_x = 0;
    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->on)
	{
	    last_x = MAX (last_x, output->priv->x + output->priv->width);
	}
    }

    /* Now position all off displays to the right of the
     * on displays
     */
    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->connected && !output->priv->on)
	{
	    output->priv->x = last_x;
	    last_x = output->priv->x + output->priv->width;
	}
    }
    
    g_assert (gnome_rr_config_match (config, config));

    return TRUE;
}

static void
gnome_rr_config_class_init (GnomeRRConfigClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = gnome_rr_config_set_property;
    gobject_class->finalize = gnome_rr_config_finalize;

    g_object_class_install_property (gobject_class, PROP_SCREEN,
				     g_param_spec_object ("screen", "Screen", "The GnomeRRScreen this config applies to", GNOME_TYPE_RR_SCREEN,
							  G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

GnomeRRConfig *
gnome_rr_config_new_current (GnomeRRScreen *screen, GError **error)
{
    GnomeRRConfig *self = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);

    if (gnome_rr_config_load_current (self, error))
      return self;
    else
      {
	g_object_unref (self);
	return NULL;
      }
}

static gboolean
output_match (GnomeRROutputInfo *output1, GnomeRROutputInfo *output2)
{
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output1));
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output2));

    if (g_strcmp0 (output1->priv->name, output2->priv->name) != 0)
	return FALSE;

    if (g_strcmp0 (output1->priv->vendor, output2->priv->vendor) != 0)
	return FALSE;

    if (g_strcmp0 (output1->priv->product, output2->priv->product) != 0)
	return FALSE;

    if (g_strcmp0 (output1->priv->serial, output2->priv->serial) != 0)
	return FALSE;
    
    return TRUE;
}

static gboolean
output_equal (GnomeRROutputInfo *output1, GnomeRROutputInfo *output2)
{
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output1));
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output2));

    if (!output_match (output1, output2))
	return FALSE;

    if (output1->priv->on != output2->priv->on)
	return FALSE;

    if (output1->priv->on)
    {
	if (output1->priv->width != output2->priv->width)
	    return FALSE;
	
	if (output1->priv->height != output2->priv->height)
	    return FALSE;
	
	if (output1->priv->rate != output2->priv->rate)
	    return FALSE;
	
	if (output1->priv->x != output2->priv->x)
	    return FALSE;
	
	if (output1->priv->y != output2->priv->y)
	    return FALSE;
	
	if (output1->priv->rotation != output2->priv->rotation)
	    return FALSE;

	if (output1->priv->underscanning != output2->priv->underscanning)
	    return FALSE;
    }

    return TRUE;
}

static GnomeRROutputInfo *
find_output (GnomeRRConfig *config, const char *name)
{
    int i;

    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];
	
	if (strcmp (name, output->priv->name) == 0)
	    return output;
    }

    return NULL;
}

/* Match means "these configurations apply to the same hardware
 * setups"
 */
gboolean
gnome_rr_config_match (GnomeRRConfig *c1, GnomeRRConfig *c2)
{
    int i;
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c2), FALSE);

    for (i = 0; c1->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output1 = c1->priv->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->priv->name);
	if (!output2 || !output_match (output1, output2))
	    return FALSE;
    }
    
    return TRUE;
}

/* Equal means "the configurations will result in the same
 * modes being set on the outputs"
 */
gboolean
gnome_rr_config_equal (GnomeRRConfig  *c1,
		       GnomeRRConfig  *c2)
{
    int i;
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c2), FALSE);

    for (i = 0; c1->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output1 = c1->priv->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->priv->name);
	if (!output2 || !output_equal (output1, output2))
	    return FALSE;
    }
    
    return TRUE;
}

static GnomeRROutputInfo **
make_outputs (GnomeRRConfig *config)
{
    GPtrArray *outputs;
    GnomeRROutputInfo *first_on;
    int i;

    outputs = g_ptr_array_new ();

    first_on = NULL;
    
    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *old = config->priv->outputs[i];
	GnomeRROutputInfo *new = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	*(new->priv) = *(old->priv);

        new->priv->name = g_strdup (old->priv->name);
        new->priv->display_name = g_strdup (old->priv->display_name);
        new->priv->connector_type = g_strdup (old->priv->connector_type);
        new->priv->vendor = g_strdup (old->priv->vendor);
        new->priv->product = g_strdup (old->priv->product);
        new->priv->serial = g_strdup (old->priv->serial);

	if (old->priv->on && !first_on)
	    first_on = old;
	
	if (config->priv->clone && new->priv->on)
	{
	    g_assert (first_on);

	    new->priv->width = first_on->priv->width;
	    new->priv->height = first_on->priv->height;
	    new->priv->rotation = first_on->priv->rotation;
	    new->priv->x = 0;
	    new->priv->y = 0;
	}

	g_ptr_array_add (outputs, new);
    }

    g_ptr_array_add (outputs, NULL);

    return (GnomeRROutputInfo **)g_ptr_array_free (outputs, FALSE);
}

gboolean
gnome_rr_config_applicable (GnomeRRConfig  *configuration,
			    GnomeRRScreen  *screen,
			    GError        **error)
{
    GnomeRROutputInfo **outputs;
    CrtcAssignment *assign;
    gboolean result;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    outputs = make_outputs (configuration);
    assign = crtc_assignment_new (screen, outputs, error);

    if (assign)
    {
	result = TRUE;
	crtc_assignment_free (assign);
    }
    else
    {
	result = FALSE;
    }

    for (i = 0; outputs[i] != NULL; i++) {
	 g_object_unref (outputs[i]);
    }

    g_free (outputs);

    return result;
}

/* Database management */

void
gnome_rr_config_sanitize (GnomeRRConfig *config)
{
    int i;
    int x_offset, y_offset;
    gboolean found;

    /* Offset everything by the top/left-most coordinate to
     * make sure the configuration starts at (0, 0)
     */
    x_offset = y_offset = G_MAXINT;
    for (i = 0; config->priv->outputs[i]; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->on)
	{
	    x_offset = MIN (x_offset, output->priv->x);
	    y_offset = MIN (y_offset, output->priv->y);
	}
    }

    for (i = 0; config->priv->outputs[i]; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];
	
	if (output->priv->on)
	{
	    output->priv->x -= x_offset;
	    output->priv->y -= y_offset;
	}
    }

    /* Only one primary, please */
    found = FALSE;
    for (i = 0; config->priv->outputs[i]; ++i)
    {
        if (config->priv->outputs[i]->priv->primary)
        {
            if (found)
            {
                config->priv->outputs[i]->priv->primary = FALSE;
            }
            else
            {
                found = TRUE;
            }
        }
    }
}

gboolean
gnome_rr_config_ensure_primary (GnomeRRConfig *configuration)
{
        int i;
        GnomeRROutputInfo  *builtin_display;
        GnomeRROutputInfo  *top_left;
        gboolean found;
	GnomeRRConfigPrivate *priv;

	g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);

        builtin_display = NULL;
        top_left = NULL;
        found = FALSE;
	priv = configuration->priv;

        for (i = 0; priv->outputs[i] != NULL; ++i) {
                GnomeRROutputInfo *info = priv->outputs[i];

                if (!info->priv->on) {
                       info->priv->primary = FALSE;
                       continue;
                }

                /* ensure only one */
                if (info->priv->primary) {
                        if (found) {
                                info->priv->primary = FALSE;
                        } else {
                                found = TRUE;
                        }
                }

                if (top_left == NULL
                    || (info->priv->x < top_left->priv->x
                        && info->priv->y < top_left->priv->y)) {
                        top_left = info;
                }
                if (builtin_display == NULL
                    && _gnome_rr_output_connector_type_is_builtin_display (info->priv->connector_type)) {
                        builtin_display = info;
                }
        }

        if (!found) {
                if (builtin_display != NULL) {
                        builtin_display->priv->primary = TRUE;
                } else if (top_left != NULL) {
                        /* Note: top_left can be NULL if all outputs are off */
                        top_left->priv->primary = TRUE;
                }
        }

        return !found;
}

static gboolean
gnome_rr_config_apply_helper (GnomeRRConfig *config,
			      GnomeRRScreen *screen,
			      gboolean       persistent,
			      GError       **error)
{
    CrtcAssignment *assignment;
    GnomeRROutputInfo **outputs;
    gboolean result = FALSE;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (config), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);

    outputs = make_outputs (config);

    assignment = crtc_assignment_new (screen, outputs, error);

    if (assignment)
    {
	if (crtc_assignment_apply (assignment, persistent, error))
	    result = TRUE;

	crtc_assignment_free (assignment);
    }

    for (i = 0; outputs[i] != NULL; i++)
	g_object_unref (outputs[i]);
    g_free (outputs);

    return result;
}

gboolean
gnome_rr_config_apply (GnomeRRConfig *config,
		       GnomeRRScreen *screen,
		       GError       **error)
{
    return gnome_rr_config_apply_helper (config, screen, FALSE, error);
}

gboolean
gnome_rr_config_apply_persistent (GnomeRRConfig *config,
				  GnomeRRScreen *screen,
				  GError       **error)
{
    return gnome_rr_config_apply_helper (config, screen, TRUE, error);
}

/**
 * gnome_rr_config_get_outputs:
 *
 * Returns: (array zero-terminated=1) (element-type CinnamonDesktop.RROutputInfo) (transfer none): the output configuration for this #GnomeRRConfig
 */
GnomeRROutputInfo **
gnome_rr_config_get_outputs (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), NULL);

    return self->priv->outputs;
}

/**
 * gnome_rr_config_get_clone:
 *
 * Returns: whether at least two outputs are at (0, 0) offset and they
 * have the same width/height.  Those outputs are of course connected and on
 * (i.e. they have a CRTC assigned).
 */
gboolean
gnome_rr_config_get_clone (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), FALSE);

    return self->priv->clone;
}

void
gnome_rr_config_set_clone (GnomeRRConfig *self, gboolean clone)
{
    g_return_if_fail (GNOME_IS_RR_CONFIG (self));

    self->priv->clone = clone;
}

/*
 * CRTC assignment
 */
typedef struct CrtcInfo CrtcInfo;

struct CrtcInfo
{
    GnomeRRMode    *mode;
    int        x;
    int        y;
    GnomeRRRotation rotation;
    GPtrArray *outputs;
};

struct CrtcAssignment
{
    GnomeRROutputInfo **outputs;
    GnomeRRScreen *screen;
    GHashTable *info;
    GnomeRROutput *primary;
};

static gboolean
can_clone (CrtcInfo *info,
	   GnomeRROutput *output)
{
    guint i;

    for (i = 0; i < info->outputs->len; ++i)
    {
	GnomeRROutput *clone = info->outputs->pdata[i];

	if (!gnome_rr_output_can_clone (clone, output))
	    return FALSE;
    }

    return TRUE;
}

static gboolean
crtc_assignment_assign (CrtcAssignment   *assign,
			GnomeRRCrtc      *crtc,
			GnomeRRMode      *mode,
			int               x,
			int               y,
			GnomeRRRotation   rotation,
                        gboolean          primary,
			GnomeRROutput    *output,
			GError          **error)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
    guint32 crtc_id;
    const char *output_name;

    crtc_id = gnome_rr_crtc_get_id (crtc);
    output_name = gnome_rr_output_get_name (output);

    if (!gnome_rr_crtc_can_drive_output (crtc, output))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d cannot drive output %s"), crtc_id, output_name);
	return FALSE;
    }

    if (!gnome_rr_output_supports_mode (output, mode))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("output %s does not support mode %dx%d@%dHz"),
		     output_name,
		     gnome_rr_mode_get_width (mode),
		     gnome_rr_mode_get_height (mode),
		     gnome_rr_mode_get_freq (mode));
	return FALSE;
    }

    if (!gnome_rr_crtc_supports_rotation (crtc, rotation))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d does not support rotation=%d"),
		     crtc_id, rotation);
	return FALSE;
    }

    if (info)
    {
	if (!(info->mode == mode	&&
	      info->x == x		&&
	      info->y == y		&&
	      info->rotation == rotation))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("output %s does not have the same parameters as another cloned output:\n"
			   "existing mode = %d, new mode = %d\n"
			   "existing coordinates = (%d, %d), new coordinates = (%d, %d)\n"
			   "existing rotation = %d, new rotation = %d"),
			 output_name,
			 gnome_rr_mode_get_id (info->mode), gnome_rr_mode_get_id (mode),
			 info->x, info->y,
			 x, y,
			 info->rotation, rotation);
	    return FALSE;
	}

	if (!can_clone (info, output))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("cannot clone to output %s"),
			 output_name);
	    return FALSE;
	}

	g_ptr_array_add (info->outputs, output);

	if (primary && !assign->primary)
	{
	    assign->primary = output;
	}

	return TRUE;
    }
    else
    {	
	info = g_new0 (CrtcInfo, 1);
	
	info->mode = mode;
	info->x = x;
	info->y = y;
	info->rotation = rotation;
	info->outputs = g_ptr_array_new ();
	
	g_ptr_array_add (info->outputs, output);
	
	g_hash_table_insert (assign->info, crtc, info);
	    
        if (primary && !assign->primary)
        {
            assign->primary = output;
        }

	return TRUE;
    }
}

static void
crtc_assignment_unassign (CrtcAssignment *assign,
			  GnomeRRCrtc         *crtc,
			  GnomeRROutput       *output)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);

    if (info)
    {
	g_ptr_array_remove (info->outputs, output);

        if (assign->primary == output)
        {
            assign->primary = NULL;
        }

	if (info->outputs->len == 0)
	    g_hash_table_remove (assign->info, crtc);
    }
}

static void
crtc_assignment_free (CrtcAssignment *assign)
{
    g_hash_table_destroy (assign->info);

    g_free (assign);
}

static gboolean
mode_is_rotated (CrtcInfo *info)
{
    if ((info->rotation & GNOME_RR_ROTATION_270)		||
	(info->rotation & GNOME_RR_ROTATION_90))
    {
	return TRUE;
    }
    return FALSE;
}

static void
accumulate_error (GString *accumulated_error, GError *error)
{
    g_string_append_printf (accumulated_error, "    %s\n", error->message);
    g_error_free (error);
}

/* Check whether the given set of settings can be used
 * at the same time -- ie. whether there is an assignment
 * of CRTC's to outputs.
 *
 * Brute force - the number of objects involved is small
 * enough that it doesn't matter.
 */
static gboolean
real_assign_crtcs (GnomeRRScreen *screen,
		   GnomeRROutputInfo **outputs,
		   CrtcAssignment *assignment,
		   GError **error)
{
    GnomeRRCrtc **crtcs = gnome_rr_screen_list_crtcs (screen);
    GnomeRROutputInfo *output;
    int i;
    gboolean tried_mode;
    GError *my_error;
    GString *accumulated_error;
    gboolean success;

    output = *outputs;
    if (!output)
	return TRUE;

    /* It is always allowed for an output to be turned off */
    if (!output->priv->on)
    {
	return real_assign_crtcs (screen, outputs + 1, assignment, error);
    }

    success = FALSE;
    tried_mode = FALSE;
    accumulated_error = g_string_new (NULL);

    for (i = 0; crtcs[i] != NULL; ++i)
    {
	GnomeRRCrtc *crtc = crtcs[i];
	int crtc_id = gnome_rr_crtc_get_id (crtc);
	int pass;

	g_string_append_printf (accumulated_error,
				_("Trying modes for CRTC %d\n"),
				crtc_id);

	/* Make two passes, one where frequencies must match, then
	 * one where they don't have to
	 */
	for (pass = 0; pass < 2; ++pass)
	{
	    GnomeRROutput *gnome_rr_output = gnome_rr_screen_get_output_by_name (screen, output->priv->name);
	    GnomeRRMode **modes = gnome_rr_output_list_modes (gnome_rr_output);
	    int j;

	    for (j = 0; modes[j] != NULL; ++j)
	    {
		GnomeRRMode *mode = modes[j];
		int mode_width;
		int mode_height;
		int mode_freq;

		mode_width = gnome_rr_mode_get_width (mode);
		mode_height = gnome_rr_mode_get_height (mode);
		mode_freq = gnome_rr_mode_get_freq (mode);

		g_string_append_printf (accumulated_error,
					_("CRTC %d: trying mode %dx%d@%dHz with output at %dx%d@%dHz (pass %d)\n"),
					crtc_id,
					mode_width, mode_height, mode_freq,
					output->priv->width, output->priv->height, output->priv->rate,
					pass);

		if (mode_width == output->priv->width	&&
		    mode_height == output->priv->height &&
		    (pass == 1 || mode_freq == output->priv->rate))
		{
		    tried_mode = TRUE;

		    my_error = NULL;
		    if (crtc_assignment_assign (
			    assignment, crtc, modes[j],
			    output->priv->x, output->priv->y,
			    output->priv->rotation,
                            output->priv->primary,
			    gnome_rr_output,
			    &my_error))
		    {
			my_error = NULL;
			if (real_assign_crtcs (screen, outputs + 1, assignment, &my_error)) {
			    success = TRUE;
			    goto out;
			} else
			    accumulate_error (accumulated_error, my_error);

			crtc_assignment_unassign (assignment, crtc, gnome_rr_output);
		    } else
			accumulate_error (accumulated_error, my_error);
		}
	    }
	}
    }

out:

    if (success)
	g_string_free (accumulated_error, TRUE);
    else {
	char *str;

	str = g_string_free (accumulated_error, FALSE);

	if (tried_mode)
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("could not assign CRTCs to outputs:\n%s"),
			 str);
	else
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("none of the selected modes were compatible with the possible modes:\n%s"),
			 str);

	g_free (str);
    }

    return success;
}

static void
crtc_info_free (CrtcInfo *info)
{
    g_ptr_array_free (info->outputs, TRUE);
    g_free (info);
}

static void
get_required_virtual_size (CrtcAssignment *assign, int *width, int *height)
{
    GList *active_crtcs = g_hash_table_get_keys (assign->info);
    GList *list;
    int d;

    if (!width)
	width = &d;
    if (!height)
	height = &d;
    
    /* Compute size of the screen */
    *width = *height = 1;
    for (list = active_crtcs; list != NULL; list = list->next)
    {
	GnomeRRCrtc *crtc = list->data;
	CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
	int w, h;

	w = gnome_rr_mode_get_width (info->mode);
	h = gnome_rr_mode_get_height (info->mode);
	
	if (mode_is_rotated (info))
	{
	    int tmp = h;
	    h = w;
	    w = tmp;
	}
	
	*width = MAX (*width, info->x + w);
	*height = MAX (*height, info->y + h);
    }

    g_list_free (active_crtcs);
}

static CrtcAssignment *
crtc_assignment_new (GnomeRRScreen      *screen,
		     GnomeRROutputInfo **outputs,
		     GError            **error)
{
    CrtcAssignment *assignment = g_new0 (CrtcAssignment, 1);

    assignment->outputs = outputs;
    assignment->info = g_hash_table_new_full (
	g_direct_hash, g_direct_equal, NULL, (GFreeFunc)crtc_info_free);

    if (real_assign_crtcs (screen, outputs, assignment, error))
    {
	int width, height;
	int min_width, max_width, min_height, max_height;

	get_required_virtual_size (assignment, &width, &height);

	gnome_rr_screen_get_ranges (
	    screen, &min_width, &max_width, &min_height, &max_height);
    
	if (width < min_width || width > max_width ||
	    height < min_height || height > max_height)
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_BOUNDS_ERROR,
			 /* Translators: the "requested", "minimum", and
			  * "maximum" words here are not keywords; please
			  * translate them as usual. */
			 _("required virtual size does not fit available size: "
			   "requested=(%d, %d), minimum=(%d, %d), maximum=(%d, %d)"),
			 width, height,
			 min_width, min_height,
			 max_width, max_height);
	    goto fail;
	}

	assignment->screen = screen;
	
	return assignment;
    }

fail:
    crtc_assignment_free (assignment);
    
    return NULL;
}

#define ROTATION_MASK 0x7F

static enum wl_output_transform
rotation_to_transform (GnomeRRRotation rotation)
{
  static const enum wl_output_transform y_reflected_map[] = {
    WL_OUTPUT_TRANSFORM_FLIPPED_180,
    WL_OUTPUT_TRANSFORM_FLIPPED_90,
    WL_OUTPUT_TRANSFORM_FLIPPED,
    WL_OUTPUT_TRANSFORM_FLIPPED_270
  };
  enum wl_output_transform ret;

  switch (rotation & ROTATION_MASK)
    {
    default:
    case GNOME_RR_ROTATION_0:
      ret = WL_OUTPUT_TRANSFORM_NORMAL;
      break;
    case GNOME_RR_ROTATION_90:
      ret = WL_OUTPUT_TRANSFORM_90;
      break;
    case GNOME_RR_ROTATION_180:
      ret = WL_OUTPUT_TRANSFORM_180;
      break;
    case GNOME_RR_ROTATION_270:
      ret = WL_OUTPUT_TRANSFORM_270;
      break;
    }

  if (rotation & GNOME_RR_REFLECT_X)
    return ret + 4;
  else if (rotation & GNOME_RR_REFLECT_Y)
    return y_reflected_map[ret];
  else
    return ret;
}

static gboolean
crtc_assignment_apply (CrtcAssignment *assign, gboolean persistent, GError **error)
{
    GVariantBuilder crtc_builder, output_builder, nested_outputs;
    GHashTableIter iter;
    GnomeRRCrtc *crtc;
    CrtcInfo *info;
    unsigned i;

    g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uiiiuaua{sv})"));
    g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(ua{sv})"));

    g_hash_table_iter_init (&iter, assign->info);
    while (g_hash_table_iter_next (&iter, (void*) &crtc, (void*) &info))
    {
	g_variant_builder_init (&nested_outputs, G_VARIANT_TYPE ("au"));
	for (i = 0; i < info->outputs->len; i++)
	{
	    GnomeRROutput *output = g_ptr_array_index (info->outputs, i);

	    g_variant_builder_add (&nested_outputs, "u",
				   gnome_rr_output_get_id (output));
	}

	g_variant_builder_add (&crtc_builder, "(uiiiuaua{sv})",
			       gnome_rr_crtc_get_id (crtc),
			       info->mode ?
			       gnome_rr_mode_get_id (info->mode) : (guint32) -1,
			       info->x,
			       info->y,
			       rotation_to_transform (info->rotation),
			       &nested_outputs,
			       NULL);
    }

    for (i = 0; assign->outputs[i]; i++)
    {
	GnomeRROutputInfo *output = assign->outputs[i];
	GnomeRROutput *gnome_rr_output = gnome_rr_screen_get_output_by_name (assign->screen,
									     output->priv->name);

	g_variant_builder_add (&output_builder, "(u@a{sv})",
			       gnome_rr_output_get_id (gnome_rr_output),
			       g_variant_new_parsed ("{ 'primary': <%b>,"
						     "  'presentation': <%b>,"
						     "  'underscanning': <%b> }",
						     output->priv->primary,
						     FALSE,
						     output->priv->underscanning));
    }

    return _gnome_rr_screen_apply_configuration (assign->screen,
						 persistent,
						 g_variant_builder_end (&crtc_builder),
						 g_variant_builder_end (&output_builder),
						 error);
}
