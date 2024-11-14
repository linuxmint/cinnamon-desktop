/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __GNOME_XKB_INFO_H__
#define __GNOME_XKB_INFO_H__

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-xkb-info.h
#endif

#include <stdio.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_XKB_INFO            (gnome_xkb_info_get_type ())
#define GNOME_XKB_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_XKB_INFO, GnomeXkbInfo))
#define GNOME_XKB_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_XKB_INFO, GnomeXkbInfoClass))
#define GNOME_IS_XKB_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_XKB_INFO))
#define GNOME_IS_XKB_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_XKB_INFO))
#define GNOME_XKB_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_XKB_INFO, GnomeXkbInfoClass))

typedef struct _GnomeXkbInfoPrivate GnomeXkbInfoPrivate;
typedef struct _GnomeXkbInfo GnomeXkbInfo;
typedef struct _GnomeXkbInfoClass GnomeXkbInfoClass;

struct _GnomeXkbInfo
{
  GObject parent_object;

  GnomeXkbInfoPrivate *priv;
};

struct _GnomeXkbInfoClass
{
  GObjectClass parent_class;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GnomeXkbInfo, g_object_unref)

GType           gnome_xkb_info_get_type                         (void);
GnomeXkbInfo   *gnome_xkb_info_new                              (void);
GList          *gnome_xkb_info_get_all_layouts                  (GnomeXkbInfo *self);
gboolean        gnome_xkb_info_get_layout_info                  (GnomeXkbInfo *self,
                                                                 const gchar  *id,
                                                                 const gchar **display_name,
                                                                 const gchar **short_name,
                                                                 const gchar **xkb_layout,
                                                                 const gchar **xkb_variant);
GList          *gnome_xkb_info_get_all_option_groups            (GnomeXkbInfo *self);
const gchar    *gnome_xkb_info_description_for_group            (GnomeXkbInfo *self,
                                                                 const gchar  *group_id);
GList          *gnome_xkb_info_get_options_for_group            (GnomeXkbInfo *self,
                                                                 const gchar  *group_id);
const gchar    *gnome_xkb_info_description_for_option           (GnomeXkbInfo *self,
                                                                 const gchar  *group_id,
                                                                 const gchar  *id);
GList          *gnome_xkb_info_get_layouts_for_language         (GnomeXkbInfo *self,
                                                                 const gchar  *language_code);
GList          *gnome_xkb_info_get_layouts_for_country          (GnomeXkbInfo *self,
                                                                 const gchar  *country_code);
GList          *gnome_xkb_info_get_languages_for_layout         (GnomeXkbInfo *self,
                                                                 const gchar  *layout_id);

G_END_DECLS

#endif  /* __GNOME_XKB_INFO_H__ */
