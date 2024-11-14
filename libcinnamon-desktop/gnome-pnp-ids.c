/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>
#include <libcinnamon-desktop/gnome-pnp-ids.h>
#include <libudev.h>

static void     gnome_pnp_ids_finalize     (GObject     *object);

struct _GnomePnpIdsPrivate
{
        struct udev *udev;
        struct udev_hwdb *hwdb;
};

G_DEFINE_TYPE_WITH_PRIVATE (GnomePnpIds, gnome_pnp_ids, G_TYPE_OBJECT)

/**
 * gnome_pnp_ids_get_pnp_id:
 * @pnp_ids: a #GnomePnpIds object
 * @pnp_id: the PNP ID to look for
 *
 * Find the full manufacturer name for the given PNP ID.
 *
 * Returns: (transfer full): a new string representing the manufacturer name,
 * or %NULL when not found.
 */
gchar *
gnome_pnp_ids_get_pnp_id (GnomePnpIds *pnp_ids, const gchar *pnp_id)
{
        GnomePnpIdsPrivate *priv = pnp_ids->priv;
        struct udev_list_entry *list_entry, *l;
        char *modalias;
        char *ret = NULL;

        modalias = g_strdup_printf ("acpi:%s:", pnp_id);
        list_entry = udev_hwdb_get_properties_list_entry(priv->hwdb, modalias, 0);
        g_free (modalias);
        if (list_entry == NULL)
                return ret;

        /* Try to get the model specific string */
        l = udev_list_entry_get_by_name (list_entry, "ID_MODEL_FROM_DATABASE");
        if (l == NULL)
                l = udev_list_entry_get_by_name (list_entry, "ID_VENDOR_FROM_DATABASE");

        if (l == NULL)
                return ret;

        ret = g_strdup (udev_list_entry_get_value (l));

        return ret;
}

static void
gnome_pnp_ids_class_init (GnomePnpIdsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gnome_pnp_ids_finalize;
}

static void
gnome_pnp_ids_init (GnomePnpIds *pnp_ids)
{
        pnp_ids->priv = gnome_pnp_ids_get_instance_private (pnp_ids);
        pnp_ids->priv->udev = udev_new();
        pnp_ids->priv->hwdb = udev_hwdb_new (pnp_ids->priv->udev);
}

static void
gnome_pnp_ids_finalize (GObject *object)
{
        GnomePnpIds *pnp_ids = GNOME_PNP_IDS (object);
        GnomePnpIdsPrivate *priv = pnp_ids->priv;

        g_clear_pointer (&priv->udev, udev_unref);
        g_clear_pointer (&priv->hwdb, udev_hwdb_unref);

        G_OBJECT_CLASS (gnome_pnp_ids_parent_class)->finalize (object);
}

/**
 * gnome_pnp_ids_new:
 *
 * Returns a reference to a #GnomePnpIds object, or creates
 * a new one if none have been created.
 *
 * Returns: (transfer full): a #GnomePnpIds object.
 */
GnomePnpIds *
gnome_pnp_ids_new (void)
{
        return g_object_new (GNOME_TYPE_PNP_IDS, NULL);
}

