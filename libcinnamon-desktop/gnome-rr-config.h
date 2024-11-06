/* gnome-rr-config.h
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
#ifndef GNOME_RR_CONFIG_H
#define GNOME_RR_CONFIG_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error   gnome-rr-config.h is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-rr-config.h
#endif

#include <glib.h>
#include <glib-object.h>
#include <libcinnamon-desktop/gnome-rr.h>

typedef struct _GnomeRROutputInfo GnomeRROutputInfo;
typedef struct _GnomeRROutputInfoClass GnomeRROutputInfoClass;
typedef struct _GnomeRROutputInfoPrivate GnomeRROutputInfoPrivate;

struct _GnomeRROutputInfo
{
    GObject parent;

    /*< private >*/
    GnomeRROutputInfoPrivate *priv;
};

struct _GnomeRROutputInfoClass
{
    GObjectClass parent_class;
};

#define GNOME_TYPE_RR_OUTPUT_INFO                  (gnome_rr_output_info_get_type())
#define GNOME_RR_OUTPUT_INFO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_OUTPUT_INFO, GnomeRROutputInfo))
#define GNOME_IS_RR_OUTPUT_INFO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_OUTPUT_INFO))
#define GNOME_RR_OUTPUT_INFO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_RR_OUTPUT_INFO, GnomeRROutputInfoClass))
#define GNOME_IS_RR_OUTPUT_INFO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_RR_OUTPUT_INFO))
#define GNOME_RR_OUTPUT_INFO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_RR_OUTPUT_INFO, GnomeRROutputInfoClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GnomeRROutputInfo, g_object_unref)

GType gnome_rr_output_info_get_type (void);

char *gnome_rr_output_info_get_name (GnomeRROutputInfo *self);

gboolean gnome_rr_output_info_is_active  (GnomeRROutputInfo *self);
void     gnome_rr_output_info_set_active (GnomeRROutputInfo *self, gboolean active);

void gnome_rr_output_info_get_geometry (GnomeRROutputInfo *self, int *x, int *y, int *width, int *height);
void gnome_rr_output_info_set_geometry (GnomeRROutputInfo *self, int  x, int  y, int  width, int  height);

int  gnome_rr_output_info_get_refresh_rate (GnomeRROutputInfo *self);
void gnome_rr_output_info_set_refresh_rate (GnomeRROutputInfo *self, int rate);

GnomeRRRotation gnome_rr_output_info_get_rotation (GnomeRROutputInfo *self);
void            gnome_rr_output_info_set_rotation (GnomeRROutputInfo *self, GnomeRRRotation rotation);
gboolean        gnome_rr_output_info_supports_rotation (GnomeRROutputInfo *self, GnomeRRRotation rotation);

gboolean gnome_rr_output_info_is_connected     (GnomeRROutputInfo *self);
const char *gnome_rr_output_info_get_vendor    (GnomeRROutputInfo *self);
const char *gnome_rr_output_info_get_product   (GnomeRROutputInfo *self);
const char *gnome_rr_output_info_get_serial    (GnomeRROutputInfo *self);
double   gnome_rr_output_info_get_aspect_ratio (GnomeRROutputInfo *self);
char    *gnome_rr_output_info_get_display_name (GnomeRROutputInfo *self);

gboolean gnome_rr_output_info_get_primary (GnomeRROutputInfo *self);
void     gnome_rr_output_info_set_primary (GnomeRROutputInfo *self, gboolean primary);

int gnome_rr_output_info_get_preferred_width  (GnomeRROutputInfo *self);
int gnome_rr_output_info_get_preferred_height (GnomeRROutputInfo *self);

gboolean gnome_rr_output_info_get_underscanning (GnomeRROutputInfo *self);
void     gnome_rr_output_info_set_underscanning (GnomeRROutputInfo *self, gboolean underscanning);

gboolean gnome_rr_output_info_is_primary_tile (GnomeRROutputInfo *self);

typedef struct _GnomeRRConfig GnomeRRConfig;
typedef struct _GnomeRRConfigClass GnomeRRConfigClass;
typedef struct _GnomeRRConfigPrivate GnomeRRConfigPrivate;

struct _GnomeRRConfig
{
    GObject parent;

    /*< private >*/
    GnomeRRConfigPrivate *priv;
};

struct _GnomeRRConfigClass
{
    GObjectClass parent_class;
};

#define GNOME_TYPE_RR_CONFIG                  (gnome_rr_config_get_type())
#define GNOME_RR_CONFIG(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_CONFIG, GnomeRRConfig))
#define GNOME_IS_RR_CONFIG(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_CONFIG))
#define GNOME_RR_CONFIG_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_RR_CONFIG, GnomeRRConfigClass))
#define GNOME_IS_RR_CONFIG_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_RR_CONFIG))
#define GNOME_RR_CONFIG_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_RR_CONFIG, GnomeRRConfigClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GnomeRRConfig, g_object_unref)

GType               gnome_rr_config_get_type     (void);

GnomeRRConfig      *gnome_rr_config_new_current  (GnomeRRScreen  *screen,
						  GError        **error);
gboolean            gnome_rr_config_load_current (GnomeRRConfig  *self,
						  GError        **error);
gboolean            gnome_rr_config_match        (GnomeRRConfig  *config1,
						  GnomeRRConfig  *config2);
gboolean            gnome_rr_config_equal	 (GnomeRRConfig  *config1,
						  GnomeRRConfig  *config2);
void                gnome_rr_config_sanitize     (GnomeRRConfig  *configuration);
gboolean            gnome_rr_config_ensure_primary (GnomeRRConfig  *configuration);

gboolean	    gnome_rr_config_apply  (GnomeRRConfig  *configuration,
					    GnomeRRScreen  *screen,
					    GError        **error);
gboolean	    gnome_rr_config_apply_persistent  (GnomeRRConfig  *configuration,
						       GnomeRRScreen  *screen,
						       GError        **error);

gboolean            gnome_rr_config_applicable   (GnomeRRConfig  *configuration,
						  GnomeRRScreen  *screen,
						  GError        **error);

gboolean            gnome_rr_config_get_clone    (GnomeRRConfig  *configuration);
void                gnome_rr_config_set_clone    (GnomeRRConfig  *configuration, gboolean clone);
GnomeRROutputInfo **gnome_rr_config_get_outputs  (GnomeRRConfig  *configuration);

#endif
