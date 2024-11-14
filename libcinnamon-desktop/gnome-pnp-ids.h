/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GNOME_PNP_IDS_H
#define __GNOME_PNP_IDS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PNP_IDS                 (gnome_pnp_ids_get_type ())
#define GNOME_PNP_IDS(o)                   (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PNP_IDS, GnomePnpIds))
#define GNOME_PNP_IDS_CLASS(k)             (G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_PNP_IDS, GnomePnpIdsClass))
#define GNOME_IS_PNP_IDS(o)                (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PNP_IDS))
#define GNOME_IS_PNP_IDS_CLASS(k)          (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PNP_IDS))
#define GNOME_PNP_IDS_GET_CLASS(o)         (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PNP_IDS, GnomePnpIdsClass))
#define GNOME_PNP_IDS_ERROR                (gnome_pnp_ids_error_quark ())

typedef struct _GnomePnpIdsPrivate        GnomePnpIdsPrivate;
typedef struct _GnomePnpIds               GnomePnpIds;
typedef struct _GnomePnpIdsClass          GnomePnpIdsClass;

struct _GnomePnpIds
{
         GObject                         parent;
         GnomePnpIdsPrivate             *priv;
};

struct _GnomePnpIdsClass
{
        GObjectClass    parent_class;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GnomePnpIds, g_object_unref)

GType            gnome_pnp_ids_get_type                    (void);
GnomePnpIds     *gnome_pnp_ids_new                         (void);
gchar           *gnome_pnp_ids_get_pnp_id                  (GnomePnpIds            *pnp_ids,
                                                            const gchar            *pnp_id);

G_END_DECLS

#endif /* __GNOME_PNP_IDS_H */

