/* gnome-rr-config.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
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
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <X11/Xlib.h>
#include <gdk/gdkx.h>

#include "gnome-rr-config.h"

#include "edid.h"
#include "gnome-rr-private.h"

#define CONFIG_INTENDED_BASENAME "cinnamon-monitors.xml"
#define CONFIG_BACKUP_BASENAME "cinnamon-monitors.xml.backup"
#define CONFIG_LEGACY_BASENAME "monitors.xml"

#define BASE_SCALE_NOT_CONFIGURED 0
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

/* A helper wrapper around the GMarkup parser stuff */
static gboolean parse_file_gmarkup (const gchar *file,
				    const GMarkupParser *parser,
				    gpointer data,
				    GError **err);

typedef struct CrtcAssignment CrtcAssignment;

static gboolean         crtc_assignment_apply (CrtcAssignment   *assign,
					       guint32           timestamp,
					       GError          **error,
                           guint            *global_scale);
static CrtcAssignment  *crtc_assignment_new   (GnomeRRConfig *config,
                                               GnomeRRScreen      *screen,
                                               GnomeRROutputInfo **outputs,
                                               GError            **error);
static void             crtc_assignment_free  (CrtcAssignment   *assign);

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_LAST
};

G_DEFINE_TYPE (GnomeRRConfig, gnome_rr_config, G_TYPE_OBJECT)

typedef struct Parser Parser;

/* Parser for monitor configurations */
struct Parser
{
    int			config_file_version;
    GnomeRROutputInfo *	output;
    GnomeRRConfig *	configuration;
    GPtrArray *		outputs;
    GPtrArray *		configurations;
    GQueue *		stack;
};

static int
parse_int (const char *text)
{
    return strtol (text, NULL, 0);
}

static guint64
parse_uint64 (const char *text)
{
    return strtoul (text, NULL, 0);
}

static double
parse_double (const char *text)
{
    return strtod (text, NULL);
}

static gboolean
stack_is (Parser *parser,
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
    while (s)
    {
	stack = g_list_prepend (stack, (gpointer)s);
	s = va_arg (args, const char *);
    }

    va_end (args);
	
    l1 = stack;
    l2 = parser->stack->head;
    
    while (l1 && l2)
    {
	if (strcmp (l1->data, l2->data) != 0)
	{
	    g_list_free (stack);
	    return FALSE;
	}
	
	l1 = l1->next;
	l2 = l2->next;
    }
    
    g_list_free (stack);
    
    return (!l1 && !l2);
}

static void
handle_start_element (GMarkupParseContext *context,
		      const gchar         *name,
		      const gchar        **attr_names,
		      const gchar        **attr_values,
		      gpointer             user_data,
		      GError             **err)
{
    Parser *parser = user_data;

    if (strcmp (name, "output") == 0)
    {
	int i;
	g_assert (parser->output == NULL);

	parser->output = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	parser->output->priv->rotation = 0;
	
	for (i = 0; attr_names[i] != NULL; ++i)
	{
	    if (strcmp (attr_names[i], "name") == 0)
	    {
		parser->output->priv->name = g_strdup (attr_values[i]);
		break;
	    }
	}

	if (!parser->output->priv->name)
	{
	    /* This really shouldn't happen, but it's better to make
	     * something up than to crash later.
	     */
	    g_warning ("Malformed monitor configuration file");
	    
	    parser->output->priv->name = g_strdup ("default");
	}	
	parser->output->priv->connected = FALSE;
	parser->output->priv->on = FALSE;
	parser->output->priv->primary = FALSE;
    }
    else if (strcmp (name, "configuration") == 0)
    {
	g_assert (parser->configuration == NULL);

	parser->configuration = g_object_new (GNOME_TYPE_RR_CONFIG, NULL);
	parser->configuration->priv->clone = FALSE;
	parser->configuration->priv->outputs = NULL;
    }
    else if (strcmp (name, "monitors") == 0)
    {
	int i;

	for (i = 0; attr_names[i] != NULL; i++)
	{
	    if (strcmp (attr_names[i], "version") == 0)
	    {
		parser->config_file_version = parse_int (attr_values[i]);
		break;
	    }
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
    Parser *parser = user_data;
    
    if (strcmp (name, "output") == 0)
    {
	/* If no rotation properties were set, just use GNOME_RR_ROTATION_0 */
	if (parser->output->priv->rotation == 0)
	    parser->output->priv->rotation = GNOME_RR_ROTATION_0;
	
	g_ptr_array_add (parser->outputs, parser->output);

	parser->output = NULL;
    }
    else if (strcmp (name, "configuration") == 0)
    {
	g_ptr_array_add (parser->outputs, NULL);
	parser->configuration->priv->outputs =
	    (GnomeRROutputInfo **)g_ptr_array_free (parser->outputs, FALSE);
	parser->outputs = g_ptr_array_new ();
	g_ptr_array_add (parser->configurations, parser->configuration);
	parser->configuration = NULL;
    }
    
    g_free (g_queue_pop_tail (parser->stack));
}

#define TOPLEVEL_ELEMENT (parser->config_file_version > 0 ? "monitors" : NULL)

static void
handle_text (GMarkupParseContext *context,
	     const gchar         *text,
	     gsize                text_len,
	     gpointer             user_data,
	     GError             **err)
{
    Parser *parser = user_data;

    if (stack_is (parser, "vendor", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;
	
	strncpy ((gchar*) parser->output->priv->vendor, text, 3);
	parser->output->priv->vendor[3] = 0;
    }
    else if (stack_is (parser, "clone", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	    parser->configuration->priv->clone = TRUE;
    }
    else if (stack_is (parser, "base_scale", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
        parser->configuration->priv->base_scale = (guint) parse_uint64 (text);
    }
    else if (stack_is (parser, "product", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;

	parser->output->priv->product = parse_int (text);
    }
    else if (stack_is (parser, "serial", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;

	parser->output->priv->serial = parse_uint64 (text);
    }
    else if (stack_is (parser, "width", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->width = parse_int (text);
    }
    else if (stack_is (parser, "x", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->x = parse_int (text);
    }
    else if (stack_is (parser, "y", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->y = parse_int (text);
    }
    else if (stack_is (parser, "scale", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
    parser->output->priv->on = TRUE;

    parser->output->priv->scale = parse_double (text);
    }
    else if (stack_is (parser, "height", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->height = parse_int (text);
    }
    else if (stack_is (parser, "rate", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->rate = parse_double (text);
    }
    else if (stack_is (parser, "rotation", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "normal") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_0;
	}
	else if (strcmp (text, "left") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_90;
	}
	else if (strcmp (text, "upside_down") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_180;
	}
	else if (strcmp (text, "right") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_270;
	}
    }
    else if (stack_is (parser, "reflect_x", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_REFLECT_X;
	}
    }
    else if (stack_is (parser, "reflect_y", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_REFLECT_Y;
	}
    }
    else if (stack_is (parser, "primary", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->primary = TRUE;
	}
    }
    else
    {
	/* Ignore other properties so we can expand the format in the future */
    }
}

static void
parser_free (Parser *parser)
{
    int i;
    GList *list;

    g_assert (parser != NULL);

    if (parser->output)
	g_object_unref (parser->output);

    if (parser->configuration)
	g_object_unref (parser->configuration);

    for (i = 0; i < parser->outputs->len; ++i)
    {
	GnomeRROutputInfo *output = parser->outputs->pdata[i];

	g_object_unref (output);
    }

    g_ptr_array_free (parser->outputs, TRUE);

    for (i = 0; i < parser->configurations->len; ++i)
    {
	GnomeRRConfig *config = parser->configurations->pdata[i];

	g_object_unref (config);
    }

    g_ptr_array_free (parser->configurations, TRUE);

    for (list = parser->stack->head; list; list = list->next)
	g_free (list->data);
    g_queue_free (parser->stack);
    
    g_free (parser);
}

static void
check_auto_scaling (GnomeRRConfig **configs)
{
    GnomeRRConfig *config;
    gint i;

    if (configs == NULL)
    {
        return;
    }

    i = 0;
    config = configs[i];

    while (config != NULL)
    {
        if (config->priv->base_scale == BASE_SCALE_NOT_CONFIGURED)
        {
            config->priv->auto_scale = TRUE;
            config->priv->base_scale = gnome_rr_screen_get_global_scale (NULL);
        }

        config = configs[++i];
    }
}

static GnomeRRConfig **
configurations_read_from_file (const gchar *filename, GError **error)
{
    Parser *parser = g_new0 (Parser, 1);
    GnomeRRConfig **result;
    GMarkupParser callbacks = {
	handle_start_element,
	handle_end_element,
	handle_text,
	NULL, /* passthrough */
	NULL, /* error */
    };

    parser->config_file_version = 0;
    parser->configurations = g_ptr_array_new ();
    parser->outputs = g_ptr_array_new ();
    parser->stack = g_queue_new ();
    
    if (!parse_file_gmarkup (filename, &callbacks, parser, error))
    {
	result = NULL;
	
	g_assert (parser->outputs);
	goto out;
    }

    g_assert (parser->outputs);

    g_ptr_array_add (parser->configurations, NULL);
    result = (GnomeRRConfig **)g_ptr_array_free (parser->configurations, FALSE);
    parser->configurations = g_ptr_array_new ();
    
    g_assert (parser->outputs);
out:
    parser_free (parser);

    check_auto_scaling (result);

    return result;
}

static void
gnome_rr_config_init (GnomeRRConfig *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_RR_CONFIG, GnomeRRConfigPrivate);

    self->priv->clone = FALSE;
    self->priv->base_scale = BASE_SCALE_NOT_CONFIGURED;
    self->priv->auto_scale = FALSE;
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
    config->priv->base_scale = gnome_rr_screen_get_global_scale (config->priv->screen);
    if (gnome_rr_screen_get_global_scale_setting (config->priv->screen) == 0)
    {
        config->priv->auto_scale = TRUE;
    }
    
    for (i = 0; rr_outputs[i] != NULL; ++i)
    {
	GnomeRROutput *rr_output = rr_outputs[i];
	GnomeRROutputInfo *output = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	GnomeRRMode *mode = NULL;
	const guint8 *edid_data = gnome_rr_output_get_edid_data (rr_output, NULL);
	GnomeRRCrtc *crtc;

	output->priv->name = g_strdup (gnome_rr_output_get_name (rr_output));
	output->priv->connected = gnome_rr_output_is_connected (rr_output);

	if (!output->priv->connected)
	{
	    output->priv->x = -1;
	    output->priv->y = -1;
	    output->priv->width = -1;
	    output->priv->height = -1;
	    output->priv->rate = -1.0f;
        output->priv->scale = MINIMUM_LOGICAL_SCALE_FACTOR;
	    output->priv->rotation = GNOME_RR_ROTATION_0;
        output->priv->doublescan = FALSE;
        output->priv->interlaced = FALSE;
        output->priv->interlaced = FALSE;
	}
	else
	{
	    MonitorInfo *info = NULL;

	    if (edid_data)
		info = decode_edid (edid_data);

	    if (info)
	    {
		memcpy (output->priv->vendor, info->manufacturer_code,
			sizeof (output->priv->vendor));
		
		output->priv->product = info->product_code;
		output->priv->serial = info->serial_number;
		output->priv->aspect = info->aspect_ratio;
	    }
	    else
	    {
		strcpy (output->priv->vendor, "???");
		output->priv->product = 0;
		output->priv->serial = 0;
	    }

	    if (gnome_rr_output_is_laptop (rr_output))
		output->priv->display_name = g_strdup (_("Laptop"));
	    else
		output->priv->display_name = make_display_name (info);
		
	    g_free (info);
		
	    crtc = gnome_rr_output_get_crtc (rr_output);
	    mode = crtc? gnome_rr_crtc_get_current_mode (crtc) : NULL;
	    
	    if (crtc && mode)
	    {
		output->priv->on = TRUE;
		
		gnome_rr_crtc_get_position (crtc, &output->priv->x, &output->priv->y);
		output->priv->width = gnome_rr_mode_get_width (mode);
		output->priv->height = gnome_rr_mode_get_height (mode);
		output->priv->rate = gnome_rr_mode_get_freq_f (mode);
		output->priv->rotation = gnome_rr_crtc_get_current_rotation (crtc);

        output->priv->scale = gnome_rr_crtc_get_scale (crtc);

        gnome_rr_mode_get_flags (mode,
                                 &output->priv->doublescan,
                                 &output->priv->interlaced,
                                 &output->priv->vsync);

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
	    
	    if (!mode)
	    {
		GnomeRRMode **modes = gnome_rr_output_list_modes (rr_output);
		
		/* FIXME: we should pick the "best" mode here, where best is
		 * sorted wrt
		 *
		 * - closest aspect ratio
		 * - mode area
		 * - refresh rate
		 * - We may want to extend randrwrap so that get_preferred
		 *   returns that - although that could also depend on
		 *   the crtc.
		 */
		if (modes[0])
		    mode = modes[0];
	    }
	    
	    if (mode)
	    {
		output->priv->pref_width = gnome_rr_mode_get_width (mode);
		output->priv->pref_height = gnome_rr_mode_get_height (mode);
	    }
	    else
	    {
		/* Pick some random numbers. This should basically never happen */
		output->priv->pref_width = 1024;
		output->priv->pref_height = 768;
	    }
	}

        output->priv->primary = gnome_rr_output_get_is_primary (rr_output);
 
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
	    last_x = output->priv->x + (output->priv->width * config->priv->base_scale);
	}
    }
    
    g_assert (gnome_rr_config_match (config, config));

    return TRUE;
}

static void
ensure_scale_factor (GnomeRRConfig     *config_from_file,
                     GnomeRROutputInfo *info_from_file)
{
    /* Loading an old config for the first time, there probably won't be any
     * scale factor.  If this happens, give it the matching current output's
     * scale factor (what actual 'is' right now) - to maintain their existing
     * configuration. */
    int k;

    for (k = 0; config_from_file->priv->outputs[k] != NULL; ++k)
    {
        if (config_from_file->priv->auto_scale)
        {
            info_from_file->priv->scale = (float) config_from_file->priv->base_scale;
            continue;
        }

        gchar *current_output_name, *new_output_name;

        current_output_name = config_from_file->priv->outputs[k]->priv->name;
        new_output_name = info_from_file->priv->name;

        if (g_strcmp0 (current_output_name, new_output_name) == 0)
        {
            info_from_file->priv->scale = config_from_file->priv->outputs[k]->priv->scale;
        }
    }

    if (info_from_file->priv->scale == 0)
    {
        info_from_file->priv->scale = MINIMUM_LOGICAL_SCALE_FACTOR;
    }
}

gboolean
gnome_rr_config_load_filename (GnomeRRConfig *result, const char *filename, GError **error)
{
    GnomeRRConfig *current;
    GnomeRRConfig **configs;
    gboolean found = FALSE;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (result), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    current = gnome_rr_config_new_current (result->priv->screen, error);

    configs = configurations_read_from_file (filename, error);

    if (configs)
    {
	int i;

	for (i = 0; configs[i] != NULL; ++i)
	{
	    if (gnome_rr_config_match (configs[i], current))
	    {
            int j;
            GPtrArray *array;
            result->priv->clone = configs[i]->priv->clone;
            result->priv->auto_scale = configs[i]->priv->auto_scale;
            result->priv->base_scale = configs[i]->priv->base_scale;

            array = g_ptr_array_new ();
            for (j = 0; configs[i]->priv->outputs[j] != NULL; j++) {
                g_object_ref (configs[i]->priv->outputs[j]);
                g_ptr_array_add (array, configs[i]->priv->outputs[j]);

                ensure_scale_factor (configs[i], configs[i]->priv->outputs[j]);
            }
            g_ptr_array_add (array, NULL);
            result->priv->outputs = (GnomeRROutputInfo **) g_ptr_array_free (array, FALSE);

            found = TRUE;
            break;
        }

        g_object_unref (configs[i]);
    }

    g_free (configs);

	if (!found)
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_NO_MATCHING_CONFIG,
			 _("none of the saved display configurations matched the active configuration"));
    }

    g_object_unref (current);
    return found;
}

static void
gnome_rr_config_class_init (GnomeRRConfigClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GnomeRROutputInfoPrivate));

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

GnomeRRConfig *
gnome_rr_config_new_stored (GnomeRRScreen *screen, GError **error)
{
    GnomeRRConfig *self = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);
    char *filename;
    gboolean success;

    filename = gnome_rr_config_get_intended_filename ();

    success = gnome_rr_config_load_filename (self, filename, error);

    if (!success)
    {
        g_clear_error (error);
        g_debug ("existing monitor config (%s) not found.  Looking for legacy configuration (monitors.xml)", filename);
        g_free (filename);
        filename = gnome_rr_config_get_legacy_filename ();

        success = gnome_rr_config_load_filename (self, filename, error);
    }

    g_free (filename);

    if (success)
      return self;
    else
      {
	g_object_unref (self);
	return NULL;
      }
}

static gboolean
parse_file_gmarkup (const gchar          *filename,
		    const GMarkupParser  *parser,
		    gpointer             data,
		    GError              **err)
{
    GMarkupParseContext *context = NULL;
    gchar *contents = NULL;
    gboolean result = TRUE;
    gsize len;

    if (!g_file_get_contents (filename, &contents, &len, err))
    {
	result = FALSE;
	goto out;
    }
    
    context = g_markup_parse_context_new (parser, 0, data, NULL);

    if (!g_markup_parse_context_parse (context, contents, len, err))
    {
	result = FALSE;
	goto out;
    }

    if (!g_markup_parse_context_end_parse (context, err))
    {
	result = FALSE;
	goto out;
    }

out:
    if (contents)
	g_free (contents);

    if (context)
	g_markup_parse_context_free (context);

    return result;
}

static gboolean
output_match (GnomeRROutputInfo *output1, GnomeRROutputInfo *output2)
{
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output1));
    g_assert (GNOME_IS_RR_OUTPUT_INFO (output2));

    if (strcmp (output1->priv->name, output2->priv->name) != 0)
	return FALSE;

    if (strcmp (output1->priv->vendor, output2->priv->vendor) != 0)
	return FALSE;

    if (output1->priv->product != output2->priv->product)
	return FALSE;

    if (output1->priv->serial != output2->priv->serial)
	return FALSE;

    if (output1->priv->connected != output2->priv->connected)
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

    if (output1->priv->scale != output2->priv->scale)
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

    if (c1->priv->auto_scale != c2->priv->auto_scale)
    {
        return FALSE;
    }

    if (c1->priv->base_scale != c2->priv->base_scale)
    {
        return FALSE;
    }

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
	if (old->priv->name)
	    new->priv->name = g_strdup (old->priv->name);
	if (old->priv->display_name)
	    new->priv->display_name = g_strdup (old->priv->display_name);

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
        new->priv->scale = first_on->priv->scale;
        new->priv->rate = 60.0f;
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
    assign = crtc_assignment_new (configuration, screen, outputs, error);

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

    return result;
}

/* Database management */

static void
ensure_config_directory (void)
{
    g_mkdir_with_parents (g_get_user_config_dir (), 0700);
}

char *
gnome_rr_config_get_backup_filename (void)
{
    ensure_config_directory ();
    return g_build_filename (g_get_user_config_dir (), CONFIG_BACKUP_BASENAME, NULL);
}

char *
gnome_rr_config_get_intended_filename (void)
{
    ensure_config_directory ();
    return g_build_filename (g_get_user_config_dir (), CONFIG_INTENDED_BASENAME, NULL);
}

char *
gnome_rr_config_get_legacy_filename (void)
{
    ensure_config_directory ();
    return g_build_filename (g_get_user_config_dir (), CONFIG_LEGACY_BASENAME, NULL);
}

static const char *
get_rotation_name (GnomeRRRotation r)
{
    if (r & GNOME_RR_ROTATION_0)
	return "normal";
    if (r & GNOME_RR_ROTATION_90)
	return "left";
    if (r & GNOME_RR_ROTATION_180)
	return "upside_down";
    if (r & GNOME_RR_ROTATION_270)
	return "right";

    return "normal";
}

static const char *
yes_no (int x)
{
    return x? "yes" : "no";
}

static const char *
get_reflect_x (GnomeRRRotation r)
{
    return yes_no (r & GNOME_RR_REFLECT_X);
}

static const char *
get_reflect_y (GnomeRRRotation r)
{
    return yes_no (r & GNOME_RR_REFLECT_Y);
}

static void
emit_configuration (GnomeRRConfig *config,
		    GString *string)
{
    int j;

    g_string_append_printf (string, "  <configuration>\n");

    g_string_append_printf (string, "      <clone>%s</clone>\n", yes_no (config->priv->clone));

    if (!config->priv->auto_scale)
    {
        g_string_append_printf (string, "      <base_scale>%d</base_scale>\n", config->priv->base_scale);
    }
    
    for (j = 0; config->priv->outputs[j] != NULL; ++j)
    {
	GnomeRROutputInfo *output = config->priv->outputs[j];
	
	g_string_append_printf (
	    string, "      <output name=\"%s\">\n", output->priv->name);
	
	if (output->priv->connected && *output->priv->vendor != '\0')
	{
	    g_string_append_printf (
		string, "          <vendor>%s</vendor>\n", output->priv->vendor);
	    g_string_append_printf (
		string, "          <product>0x%04x</product>\n", output->priv->product);
	    g_string_append_printf (
		string, "          <serial>0x%08x</serial>\n", output->priv->serial);
	}
	
	/* An unconnected output which is on does not make sense */
	if (output->priv->connected && output->priv->on)
	{
	    g_string_append_printf (
		string, "          <width>%d</width>\n", output->priv->width);
	    g_string_append_printf (
		string, "          <height>%d</height>\n", output->priv->height);
	    g_string_append_printf (
		string, "          <rate>%f</rate>\n", output->priv->rate);
	    g_string_append_printf (
		string, "          <x>%d</x>\n", output->priv->x);
	    g_string_append_printf (
		string, "          <y>%d</y>\n", output->priv->y);

        if (!config->priv->auto_scale)
        {
            g_string_append_printf (
            string, "          <scale>%f</scale>\n", output->priv->scale);
        }

        g_string_append_printf (
		string, "          <rotation>%s</rotation>\n", get_rotation_name (output->priv->rotation));
	    g_string_append_printf (
		string, "          <reflect_x>%s</reflect_x>\n", get_reflect_x (output->priv->rotation));
	    g_string_append_printf (
		string, "          <reflect_y>%s</reflect_y>\n", get_reflect_y (output->priv->rotation));
            g_string_append_printf (
                string, "          <primary>%s</primary>\n", yes_no (output->priv->primary));
	}
	
	g_string_append_printf (string, "      </output>\n");
    }
    
    g_string_append_printf (string, "  </configuration>\n");
}

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

        output->priv->x -= x_offset;
        output->priv->y -= y_offset;
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
        GnomeRROutputInfo  *laptop;
        GnomeRROutputInfo  *top_left;
        gboolean found;
	GnomeRRConfigPrivate *priv;

	g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);

        laptop = NULL;
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
                if (laptop == NULL
                    && _gnome_rr_output_name_is_laptop (info->priv->name)) {
                        /* shame we can't find the connector type
                           as with gnome_rr_output_is_laptop */
                        laptop = info;
                }
        }

        if (!found) {
                if (laptop != NULL) {
                        laptop->priv->primary = TRUE;
                } else if (top_left != NULL) {
                        /* Note: top_left can be NULL if all outputs are off */
                        top_left->priv->primary = TRUE;
                }
        }

        return !found;
}

gboolean
gnome_rr_config_save (GnomeRRConfig *configuration, GError **error)
{
    GnomeRRConfig **configurations;
    GString *output;
    int i;
    gchar *intended_filename;
    gchar *backup_filename;
    gboolean result;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    output = g_string_new ("");

    backup_filename = gnome_rr_config_get_backup_filename ();
    intended_filename = gnome_rr_config_get_intended_filename ();

    configurations = configurations_read_from_file (intended_filename, NULL); /* NULL-GError */
    
    g_string_append_printf (output, "<monitors version=\"1\">\n");

    if (configurations)
    {
	for (i = 0; configurations[i] != NULL; ++i)
	{
	    if (!gnome_rr_config_match (configurations[i], configuration))
		emit_configuration (configurations[i], output);
	    g_object_unref (configurations[i]);
	}

	g_free (configurations);
    }

    emit_configuration (configuration, output);

    g_string_append_printf (output, "</monitors>\n");

    /* backup the file first */
    rename (intended_filename, backup_filename); /* no error checking because the intended file may not even exist */

    result = g_file_set_contents (intended_filename, output->str, -1, error);

    if (!result)
	rename (backup_filename, intended_filename); /* no error checking because the backup may not even exist */

    g_free (backup_filename);
    g_free (intended_filename);
    g_string_free (output, TRUE);

    return result;
}

gboolean
gnome_rr_config_apply_with_time (GnomeRRConfig *config,
				 GnomeRRScreen *screen,
				 guint32        timestamp,
				 GError       **error)
{
    CrtcAssignment *assignment;
    GnomeRROutputInfo **outputs;
    gboolean result = FALSE;
    int i;
    guint global_scale;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (config), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);

    gdk_error_trap_push ();

    outputs = make_outputs (config);

    assignment = crtc_assignment_new (config, screen, outputs, error);

    for (i = 0; outputs[i] != NULL; i++)
	g_object_unref (outputs[i]);
    g_free (outputs);

    global_scale = config->priv->base_scale;

    if (assignment)
    {
	if (crtc_assignment_apply (assignment, timestamp, error, &global_scale))
	    result = TRUE;

	crtc_assignment_free (assignment);

	
	gdk_flush ();
	gdk_error_trap_pop (); // ignore errors
    }

    if (result == TRUE)
    {
        gnome_rr_screen_set_global_scale_setting (screen,
                                                  config->priv->auto_scale ? 0 : global_scale);
    }

    return result;
}

/* gnome_rr_config_apply_from_filename_with_time:
 * @screen: A #GnomeRRScreen
 * @filename: Path of the file to look in for stored RANDR configurations.
 * @timestamp: X server timestamp from the event that causes the screen configuration to change (a user's button press, for example)
 * @error: Location to store error, or %NULL
 *
 * Loads the file in @filename and looks for suitable matching RANDR
 * configurations in the file; if one is found, that configuration will be
 * applied to the current set of RANDR outputs.
 *
 * Typically, @filename is the result of gnome_rr_config_get_intended_filename() or
 * gnome_rr_config_get_backup_filename().
 *
 * Returns: TRUE if the RANDR configuration was loaded and applied from
 * the specified file, or FALSE otherwise:
 *
 * If the file in question is loaded successfully but the configuration cannot
 * be applied, the @error will have a domain of #GNOME_RR_ERROR.  Note that an
 * error code of #GNOME_RR_ERROR_NO_MATCHING_CONFIG is not a real error; it
 * simply means that there were no stored configurations that match the current
 * set of RANDR outputs.
 *
 * If the file in question cannot be loaded, the @error will have a domain of
 * #G_FILE_ERROR.  Note that an error code of G_FILE_ERROR_NOENT is not really
 * an error, either; it means that there was no stored configuration file and so
 * nothing is changed.
 */
gboolean
gnome_rr_config_apply_from_filename_with_time (GnomeRRScreen *screen, const char *filename, guint32 timestamp, GError **error)
{
    GnomeRRConfig *stored;

    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    stored = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);

    if (gnome_rr_config_load_filename (stored, filename, error))
    {
	gboolean result;

        gnome_rr_config_ensure_primary (stored);
	result = gnome_rr_config_apply_with_time (stored, screen, timestamp, error);

	g_object_unref (stored);
	return result;
    }
    else
    {
	g_object_unref (stored);
	return FALSE;
    }
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

guint
gnome_rr_config_get_base_scale (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), MINIMUM_GLOBAL_SCALE_FACTOR);

    if (self->priv->auto_scale)
    {
        return gnome_rr_screen_get_global_scale (self->priv->screen);
    }

    return self->priv->base_scale;
}

void
gnome_rr_config_set_base_scale (GnomeRRConfig *self,
                                guint base_scale)
{
    g_return_if_fail (GNOME_IS_RR_CONFIG (self) || base_scale < MINIMUM_GLOBAL_SCALE_FACTOR);

    self->priv->base_scale = base_scale;
}

gboolean
gnome_rr_config_get_auto_scale (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), TRUE);

    return self->priv->auto_scale;
}

void
gnome_rr_config_set_auto_scale (GnomeRRConfig *self,
                                gboolean       auto_scale)
{
    g_return_if_fail (GNOME_IS_RR_CONFIG (self));

    self->priv->auto_scale = auto_scale;
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
    float      scale;
    GnomeRRRotation rotation;
    GPtrArray *outputs;
};

struct CrtcAssignment
{
    GnomeRRScreen *screen;
    GHashTable *info;
    GnomeRROutput *primary;
};

static gboolean
can_clone (CrtcInfo *info,
	   GnomeRROutput *output)
{
    int i;

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
            float             scale,
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
                     _("CRTC %d does not support rotation=%s"),
                     crtc_id,
                     get_rotation_name (rotation));
        return FALSE;
    }

    if (info)
    {
        if (!(info->mode == mode	&&
              info->x == x		&&
              info->y == y		&&
              info->scale == scale &&
              info->rotation == rotation))
        {
            g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
                         _("output %s does not have the same parameters as another cloned output:\n"
                         "existing mode = %d, new mode = %d\n"
                         "existing coordinates = (%d, %d), new coordinates = (%d, %d)\n"
                         "existing rotation = %s, new rotation = %s"
                         "existing scale = %.2f, new scale = %.2f"),
                         output_name,
                         gnome_rr_mode_get_id (info->mode), gnome_rr_mode_get_id (mode),
                         info->x, info->y,
                         x, y,
                         get_rotation_name (info->rotation), get_rotation_name (rotation),
                         info->scale, scale);
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
        CrtcInfo *info = g_new0 (CrtcInfo, 1);

        info->mode = mode;
        info->x = x;
        info->y = y;
        info->rotation = rotation;
        info->scale = scale;
        info->outputs = g_ptr_array_new ();

        g_ptr_array_add (info->outputs, output);

        g_hash_table_insert (assign->info, crtc, info);

        if (primary && !assign->primary)
        {
            assign->primary = output;
        }

        return TRUE;
    }

    return FALSE;
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

typedef struct {
    guint32 timestamp;
    gboolean has_error;
    GError **error;
    guint global_scale;
} ConfigureCrtcState;

// static guint
// get_max_info_scale (CrtcAssignment *assignment)
// {
//     GList *infos, *iter;
//     float max_scale = 0;

//     infos = g_hash_table_get_values (assignment->info);

//     for (iter = infos; iter != NULL; iter = iter->next)
//     {
//         CrtcInfo *info = iter->data;

//         if (info->scale > max_scale)
//         {
//             max_scale = info->scale;
//         }
//     }

//     g_list_free (infos);


//     return CLAMP ((guint) ceilf (max_scale),
//                   MINIMUM_GLOBAL_SCALE_FACTOR,
//                   MAXIMUM_GLOBAL_SCALE_FACTOR);
// }

// static guint
// get_min_info_scale (CrtcAssignment *assignment)
// {
//     GList *infos, *iter;
//     float min_scale = 4.0f;

//     infos = g_hash_table_get_values (assignment->info);

//     for (iter = infos; iter != NULL; iter = iter->next)
//     {
//         CrtcInfo *info = iter->data;

//         if (info->scale < min_scale)
//         {
//             min_scale = info->scale;
//         }
//     }

//     g_list_free (infos);

//     return CLAMP ((guint) floorf (min_scale),
//                   MINIMUM_GLOBAL_SCALE_FACTOR,
//                   MAXIMUM_GLOBAL_SCALE_FACTOR);
// }

static void
configure_crtc (gpointer key,
		gpointer value,
		gpointer data)
{
    GnomeRRCrtc *crtc = key;
    CrtcInfo *info = value;
    ConfigureCrtcState *state = data;

    if (state->has_error)
	return;

    if (!gnome_rr_crtc_set_config_with_time (crtc,
					     state->timestamp,
					     info->x, info->y,
					     info->mode,
					     info->rotation,
					     (GnomeRROutput **)info->outputs->pdata,
					     info->outputs->len,
                         info->scale,
                         state->global_scale,
					     state->error))
	state->has_error = TRUE;
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

static gboolean
crtc_is_rotated (GnomeRRCrtc *crtc)
{
    GnomeRRRotation r = gnome_rr_crtc_get_current_rotation (crtc);

    if ((r & GNOME_RR_ROTATION_270)		||
	(r & GNOME_RR_ROTATION_90))
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
        g_debug (_("Trying modes for CRTC %d"),
                 crtc_id);
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
                double mode_freq;

                mode_width = gnome_rr_mode_get_width (mode);
                mode_height = gnome_rr_mode_get_height (mode);
                mode_freq = gnome_rr_mode_get_freq_f (mode);

                g_string_append_printf (accumulated_error,
					_("CRTC %d: trying mode %dx%d@%.2fHz with output at %dx%d@%.2fHz (pass %d)\n"),
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
                        output->priv->scale,
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
get_required_virtual_size (CrtcAssignment *assign,
                           GnomeRRScreen  *screen,
                           int            *width,
                           int            *height,
                           float          *avg_scale,
                           guint          *global_scale)
{
    GList *active_crtcs = g_hash_table_get_keys (assign->info);
    GList *list;
    int crtc_count;
    float avg_screen_scale;

/*
    if (gnome_rr_screen_get_use_upscaling (screen))
    {
        *global_scale = get_min_info_scale (assign);
    }
    else
    {
        *global_scale = get_max_info_scale (assign);
    }
*/

    /* Compute size of the screen */
    *width = *height = 1;
    avg_screen_scale = 0;
    crtc_count = 0;
    for (list = active_crtcs; list != NULL; list = list->next)
    {
        GnomeRRCrtc *crtc = list->data;
        CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
        int w, h;
        float scale = 1.0f;

        scale = *global_scale / info->scale;

        w = gnome_rr_mode_get_width (info->mode);
        h = gnome_rr_mode_get_height (info->mode);

        if (mode_is_rotated (info))
        {
            int tmp = h;
            h = w;
            w = tmp;
        }

        *width = MAX (*width, info->x + roundf (w * scale));
        *height = MAX (*height, info->y + roundf (h * scale));

        avg_screen_scale += (info->scale - avg_screen_scale) / (float) (++crtc_count);
    }

    *avg_scale = avg_screen_scale;

    g_debug ("Proposed screen size: %dx%d average scale: %.2f, ui scale: %d",
             *width, *height, *avg_scale, *global_scale);

    g_list_free (active_crtcs);
}

static CrtcAssignment *
crtc_assignment_new (GnomeRRConfig      *config,
                     GnomeRRScreen      *screen,
                     GnomeRROutputInfo **outputs,
                     GError            **error)
{
    CrtcAssignment *assignment = g_new0 (CrtcAssignment, 1);

    assignment->info = g_hash_table_new_full (
	g_direct_hash, g_direct_equal, NULL, (GFreeFunc)crtc_info_free);

    if (real_assign_crtcs (screen, outputs, assignment, error))
    {
	int width, height;
	int min_width, max_width, min_height, max_height;
    float scale;
    guint global_scale = config->priv->base_scale;

	get_required_virtual_size (assignment,
                               screen,
                               &width, &height,
                               &scale, &global_scale);

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

static gboolean
crtc_assignment_apply (CrtcAssignment *assign,
                       guint32         timestamp,
                       GError        **error,
                       guint          *global_scale)
{
    GnomeRRCrtc **all_crtcs = gnome_rr_screen_list_crtcs (assign->screen);
    int width, height;
    int i;
    int min_width, max_width, min_height, max_height;
    int width_mm, height_mm;
    float average_scale;
    gboolean success = TRUE;

    /* Compute size of the screen */
    get_required_virtual_size (assign,
                               assign->screen,
                               &width, &height,
                               &average_scale, global_scale);

    gnome_rr_screen_get_ranges (
	assign->screen, &min_width, &max_width, &min_height, &max_height);

    /* We should never get here if the dimensions don't fit in the virtual size,
     * but just in case we do, fix it up.
     */
    width = MAX (min_width, width);
    width = MIN (max_width, width);
    height = MAX (min_height, height);
    height = MIN (max_height, height);

    /* FMQ: do we need to check the sizes instead of clamping them? */

    /* Grab the server while we fiddle with the CRTCs and the screen, so that
     * apps that listen for RANDR notifications will only receive the final
     * status.
     */

    gdk_x11_display_grab (gdk_screen_get_display (assign->screen->priv->gdk_screen));

    /* Turn off all crtcs that are currently displaying outside the new screen,
     * or are not used in the new setup
     */
    for (i = 0; all_crtcs[i] != NULL; ++i)
    {
	GnomeRRCrtc *crtc = all_crtcs[i];
	GnomeRRMode *mode = gnome_rr_crtc_get_current_mode (crtc);
	int x, y;

	if (mode)
    {
	    int w, h;
	    gnome_rr_crtc_get_position (crtc, &x, &y);

	    w = gnome_rr_mode_get_width (mode) * (*global_scale);
	    h = gnome_rr_mode_get_height (mode) * (*global_scale);
	    if (crtc_is_rotated (crtc))
	    {
		int tmp = h;
		h = w;
		w = tmp;
	    }
	    
	    if (x + w > width || y + h > height || !g_hash_table_lookup (assign->info, crtc))
	    {
		if (!gnome_rr_crtc_set_config_with_time (crtc,
                                                 timestamp,
                                                 0, 0,
                                                 NULL,
                                                 GNOME_RR_ROTATION_0,
                                                 NULL,
                                                 0,
                                                 1.0f,
                                                 1,
                                                 error))
		{
		    success = FALSE;
		    break;
		}
		
	    }
	}
    }

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    width_mm = (width / (DPI_FALLBACK / average_scale)) * 25.4 + 0.5;
    height_mm = (height / (DPI_FALLBACK / average_scale)) * 25.4 + 0.5;

    if (success)
    {
        ConfigureCrtcState state;

        state.timestamp = timestamp;
        state.has_error = FALSE;
        state.error = error;
        state.global_scale = *global_scale;
        gnome_rr_screen_set_size (assign->screen, width, height, width_mm, height_mm);

        g_hash_table_foreach (assign->info, configure_crtc, &state);

        success = !state.has_error;
    }

    gnome_rr_screen_set_primary_output (assign->screen, assign->primary);

    gdk_x11_display_ungrab (gdk_screen_get_display (assign->screen->priv->gdk_screen));

    return success;
}
