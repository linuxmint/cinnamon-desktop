/* gnome-rr-output-info.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2010 Giovanni Campagna
 *
 * This file is part of the Gnome Desktop Library.
 *
 * The Gnome Desktop Library is free software; you can redistribute it and/or
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
 * License along with the Gnome Desktop Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>

#include "gnome-rr-config.h"

#include "edid.h"
#include "gnome-rr-private.h"

G_DEFINE_TYPE_WITH_PRIVATE (GnomeRROutputInfo, gnome_rr_output_info, G_TYPE_OBJECT)

static void
gnome_rr_output_info_init (GnomeRROutputInfo *self)
{
    self->priv = gnome_rr_output_info_get_instance_private (self);

    self->priv->name = NULL;
    self->priv->on = FALSE;
    self->priv->display_name = NULL;
    self->priv->scale = 1;
}

static void
gnome_rr_output_info_finalize (GObject *gobject)
{
    GnomeRROutputInfo *self = GNOME_RR_OUTPUT_INFO (gobject);

    g_free (self->priv->name);
    g_free (self->priv->display_name);

    G_OBJECT_CLASS (gnome_rr_output_info_parent_class)->finalize (gobject);
}

static void
gnome_rr_output_info_class_init (GnomeRROutputInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = gnome_rr_output_info_finalize;
}

/**
 * gnome_rr_output_info_get_name:
 *
 * Returns: (transfer none): the output name
 */
const char *gnome_rr_output_info_get_name (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->name;
}

/**
 * gnome_rr_output_info_is_active:
 *
 * Returns: whether there is a CRTC assigned to this output (i.e. a signal is being sent to it)
 */
gboolean gnome_rr_output_info_is_active (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->on;
}

void gnome_rr_output_info_set_active (GnomeRROutputInfo *self, gboolean active)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->on = active;
}

/**
 * gnome_rr_output_info_get_geometry:
 * @self: a #GnomeRROutputInfo
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 * @width: (out) (allow-none):
 * @height: (out) (allow-none):
 */
void gnome_rr_output_info_get_geometry (GnomeRROutputInfo *self, int *x, int *y, int *width, int *height)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    if (x)
	*x = self->priv->x;
    if (y)
	*y = self->priv->y;
    if (width)
	*width = self->priv->width;
    if (height)
	*height = self->priv->height;
}

void gnome_rr_output_info_set_geometry (GnomeRROutputInfo *self, int  x, int  y, int  width, int  height)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->x = x;
    self->priv->y = y;
    self->priv->width = width;
    self->priv->height = height;
}

float gnome_rr_output_info_get_scale (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), MINIMUM_LOGICAL_SCALE_FACTOR);

    return self->priv->scale;
}

void gnome_rr_output_info_set_scale (GnomeRROutputInfo *self, float scale)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->scale = scale;
}

int gnome_rr_output_info_get_refresh_rate (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return (int) self->priv->rate;
}

void gnome_rr_output_info_set_refresh_rate (GnomeRROutputInfo *self, int rate)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->rate = rate * 1.0;
}

double gnome_rr_output_info_get_refresh_rate_f (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->rate;
}

void gnome_rr_output_info_set_refresh_rate_f (GnomeRROutputInfo *self, double rate)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->rate = rate;
}

GnomeRRRotation gnome_rr_output_info_get_rotation (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), GNOME_RR_ROTATION_0);

    return self->priv->rotation;
}

void gnome_rr_output_info_set_rotation (GnomeRROutputInfo *self, GnomeRRRotation rotation)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->rotation = rotation;
}

void gnome_rr_output_info_get_flags (GnomeRROutputInfo *self,
                                     gboolean          *doublescan,
                                     gboolean          *interlaced,
                                     gboolean          *vsync)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    if (doublescan)
        *doublescan = self->priv->doublescan;
    if (interlaced)
        *interlaced = self->priv->interlaced;
    if (vsync)
        *vsync = self->priv->vsync;
}

void gnome_rr_output_info_set_flags (GnomeRROutputInfo *self,
                                     gboolean           doublescan,
                                     gboolean           interlaced,
                                     gboolean           vsync)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->doublescan = doublescan;
    self->priv->interlaced = interlaced;
    self->priv->vsync = vsync;
}

/**
 * gnome_rr_output_info_is_connected:
 *
 * Returns: whether the output is physically connected to a monitor
 */
gboolean gnome_rr_output_info_is_connected (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->connected;
}

/**
 * gnome_rr_output_info_get_vendor:
 * @self: a #GnomeRROutputInfo
 * @vendor: (out caller-allocates) (array fixed-size=4):
 */
void gnome_rr_output_info_get_vendor (GnomeRROutputInfo *self, gchar* vendor)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));
    g_return_if_fail (vendor != NULL);

    vendor[0] = self->priv->vendor[0];
    vendor[1] = self->priv->vendor[1];
    vendor[2] = self->priv->vendor[2];
    vendor[3] = self->priv->vendor[3];
}

guint gnome_rr_output_info_get_product (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->product;
}

guint gnome_rr_output_info_get_serial (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->serial;
}

double gnome_rr_output_info_get_aspect_ratio (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->aspect;
}

/**
 * gnome_rr_output_info_get_display_name:
 *
 * Returns: (transfer none): the display name of this output
 */
const char *gnome_rr_output_info_get_display_name (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->display_name;
}

gboolean gnome_rr_output_info_get_primary (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->primary;
}

void gnome_rr_output_info_set_primary (GnomeRROutputInfo *self, gboolean primary)
{
    g_return_if_fail (GNOME_IS_RR_OUTPUT_INFO (self));

    self->priv->primary = primary;
}

int gnome_rr_output_info_get_preferred_width (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_width;
}

int gnome_rr_output_info_get_preferred_height (GnomeRROutputInfo *self)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_height;
}
