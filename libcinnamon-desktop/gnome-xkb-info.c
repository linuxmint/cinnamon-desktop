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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#include <gdk/gdkx.h>

#include <glib/gi18n-lib.h>
#define XKEYBOARD_CONFIG_(String) ((char *) g_dgettext ("xkeyboard-config", String))

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-xkb-info.h"

#ifndef XKB_RULES_FILE
#define XKB_RULES_FILE "evdev"
#endif
#ifndef XKB_LAYOUT
#define XKB_LAYOUT "us"
#endif
#ifndef XKB_MODEL
#define XKB_MODEL "pc105+inet"
#endif

typedef struct _Layout Layout;
struct _Layout
{
  gchar *id;
  gchar *xkb_name;
  gchar *short_desc;
  gchar *description;
  gboolean is_variant;
  const Layout *main_layout;
};

typedef struct _XkbOption XkbOption;
struct _XkbOption
{
  gchar *id;
  gchar *description;
};

typedef struct _XkbOptionGroup XkbOptionGroup;
struct _XkbOptionGroup
{
  gchar *id;
  gchar *description;
  gboolean allow_multiple_selection;
  GHashTable *options_table;
};

struct _GnomeXkbInfoPrivate
{
  GHashTable *option_groups_table;
  GHashTable *layouts_by_short_desc;
  GHashTable *layouts_by_iso639;
  GHashTable *layouts_table;

  /* Only used while parsing */
  XkbOptionGroup *current_parser_group;
  XkbOption *current_parser_option;
  Layout *current_parser_layout;
  Layout *current_parser_variant;
  gchar  *current_parser_iso639Id;
  gchar **current_parser_text;
};

G_DEFINE_TYPE (GnomeXkbInfo, gnome_xkb_info, G_TYPE_OBJECT);

static void
free_layout (gpointer data)
{
  Layout *layout = data;

  g_return_if_fail (layout != NULL);

  g_free (layout->id);
  g_free (layout->xkb_name);
  g_free (layout->short_desc);
  g_free (layout->description);
  g_slice_free (Layout, layout);
}

static void
free_option (gpointer data)
{
  XkbOption *option = data;

  g_return_if_fail (option != NULL);

  g_free (option->id);
  g_free (option->description);
  g_slice_free (XkbOption, option);
}

static void
free_option_group (gpointer data)
{
  XkbOptionGroup *group = data;

  g_return_if_fail (group != NULL);

  g_free (group->id);
  g_free (group->description);
  g_hash_table_destroy (group->options_table);
  g_slice_free (XkbOptionGroup, group);
}

/**
 * gnome_xkb_info_get_var_defs: (skip)
 * @rules: (out) (transfer full): location to store the rules file
 * path. Use g_free() when it's no longer needed
 * @var_defs: (out) (transfer full): location to store a
 * #XkbRF_VarDefsRec pointer. Use gnome_xkb_info_free_var_defs() to
 * free it
 *
 * Gets both the XKB rules file path and the current XKB parameters in
 * use by the X server.
 *
 * Since: 3.6
 */
void
gnome_xkb_info_get_var_defs (gchar            **rules,
                             XkbRF_VarDefsRec **var_defs)
{
  Display *display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  char *tmp;

  g_return_if_fail (rules != NULL);
  g_return_if_fail (var_defs != NULL);

  *rules = NULL;
  *var_defs = g_new0 (XkbRF_VarDefsRec, 1);

  gdk_error_trap_push ();

  /* Get it from the X property or fallback on defaults */
  if (!XkbRF_GetNamesProp (display, rules, *var_defs) || !*rules)
    {
      *rules = strdup (XKB_RULES_FILE);
      (*var_defs)->model = strdup (XKB_MODEL);
      (*var_defs)->layout = strdup (XKB_LAYOUT);
      (*var_defs)->variant = NULL;
      (*var_defs)->options = NULL;
    }

  gdk_error_trap_pop_ignored ();

  tmp = *rules;

  if (*rules[0] == '/')
    *rules = g_strdup (*rules);
  else
    *rules = g_build_filename (XKB_BASE, "rules", *rules, NULL);

  free (tmp);
}

/**
 * gnome_xkb_info_free_var_defs: (skip)
 * @var_defs: #XkbRF_VarDefsRec instance to free
 *
 * Frees an #XkbRF_VarDefsRec instance allocated by
 * gnome_xkb_info_get_var_defs().
 *
 * Since: 3.6
 */
void
gnome_xkb_info_free_var_defs (XkbRF_VarDefsRec *var_defs)
{
  g_return_if_fail (var_defs != NULL);

  free (var_defs->model);
  free (var_defs->layout);
  free (var_defs->variant);
  free (var_defs->options);

  g_free (var_defs);
}

static gchar *
get_xml_rules_file_path (const gchar *suffix)
{
  XkbRF_VarDefsRec *xkb_var_defs;
  gchar *rules_file;
  gchar *xml_rules_file;

  gnome_xkb_info_get_var_defs (&rules_file, &xkb_var_defs);
  gnome_xkb_info_free_var_defs (xkb_var_defs);

  xml_rules_file = g_strdup_printf ("%s%s", rules_file, suffix);
  g_free (rules_file);

  return xml_rules_file;
}

static void
parse_start_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     gpointer              data,
                     GError              **error)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (data)->priv;

  if (priv->current_parser_text)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected character data but got element '%s'", element_name);
      return;
    }

  if (strcmp (element_name, "name") == 0)
    {
      if (priv->current_parser_variant)
        priv->current_parser_text = &priv->current_parser_variant->xkb_name;
      else if (priv->current_parser_layout)
        priv->current_parser_text = &priv->current_parser_layout->xkb_name;
      else if (priv->current_parser_option)
        priv->current_parser_text = &priv->current_parser_option->id;
      else if (priv->current_parser_group)
        priv->current_parser_text = &priv->current_parser_group->id;
    }
  else if (strcmp (element_name, "description") == 0)
    {
      if (priv->current_parser_variant)
        priv->current_parser_text = &priv->current_parser_variant->description;
      else if (priv->current_parser_layout)
        priv->current_parser_text = &priv->current_parser_layout->description;
      else if (priv->current_parser_option)
        priv->current_parser_text = &priv->current_parser_option->description;
      else if (priv->current_parser_group)
        priv->current_parser_text = &priv->current_parser_group->description;
    }
  else if (strcmp (element_name, "shortDescription") == 0)
    {
      if (priv->current_parser_variant)
        priv->current_parser_text = &priv->current_parser_variant->short_desc;
      else if (priv->current_parser_layout)
        priv->current_parser_text = &priv->current_parser_layout->short_desc;
    }
  else if (strcmp (element_name, "iso639Id") == 0)
    {
      priv->current_parser_text = &priv->current_parser_iso639Id;
    }
  else if (strcmp (element_name, "layout") == 0)
    {
      if (priv->current_parser_layout)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'layout' elements can't be nested");
          return;
        }

      priv->current_parser_layout = g_slice_new0 (Layout);
    }
  else if (strcmp (element_name, "variant") == 0)
    {
      Layout *layout;

      if (priv->current_parser_variant)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements can't be nested");
          return;
        }

      if (!priv->current_parser_layout)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements must be inside 'layout' elements");
          return;
        }

      if (!priv->current_parser_layout->xkb_name)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements must be inside named 'layout' elements");
          return;
        }

      layout = g_hash_table_lookup (priv->layouts_table, priv->current_parser_layout->xkb_name);
      if (!layout)
        layout = priv->current_parser_layout;

      priv->current_parser_variant = g_slice_new0 (Layout);
      priv->current_parser_variant->is_variant = TRUE;
      priv->current_parser_variant->main_layout = layout;
    }
  else if (strcmp (element_name, "group") == 0)
    {
      if (priv->current_parser_group)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'group' elements can't be nested");
          return;
        }

      priv->current_parser_group = g_slice_new0 (XkbOptionGroup);
      /* Maps option ids to XkbOption structs. Owns the XkbOption structs. */
      priv->current_parser_group->options_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                         NULL, free_option);
      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
                                   G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL,
                                   "allowMultipleSelection",
                                   &priv->current_parser_group->allow_multiple_selection,
                                   G_MARKUP_COLLECT_INVALID);
    }
  else if (strcmp (element_name, "option") == 0)
    {
      if (priv->current_parser_option)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'option' elements can't be nested");
          return;
        }

      if (!priv->current_parser_group)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'option' elements must be inside 'group' elements");
          return;
        }

      priv->current_parser_option = g_slice_new0 (XkbOption);
    }
}

static gboolean
maybe_replace (GHashTable *table,
               gchar      *key,
               Layout     *new_layout)
{
  /* There might be multiple layouts for the same language. In that
   * case considering the "canonical" layout to be the one with the
   * shorter description seems to be good enough. */
  Layout *layout;
  gboolean exists;
  gboolean replace = TRUE;

  exists = g_hash_table_lookup_extended (table, key, NULL, (gpointer *)&layout);
  if (exists)
    replace = strlen (new_layout->description) < strlen (layout->description);
  if (replace)
    g_hash_table_replace (table, key, new_layout);

  return replace;
}

static void
parse_end_element (GMarkupParseContext  *context,
                   const gchar          *element_name,
                   gpointer              data,
                   GError              **error)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (data)->priv;

  if (strcmp (element_name, "layout") == 0)
    {
      if (!priv->current_parser_layout->description || !priv->current_parser_layout->xkb_name)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'layout' elements must enclose 'description' and 'name' elements");
          return;
        }

      priv->current_parser_layout->id = g_strdup (priv->current_parser_layout->xkb_name);

      if (g_hash_table_contains (priv->layouts_table, priv->current_parser_layout->id))
        {
          if (priv->current_parser_layout != NULL) {
            free_layout (priv->current_parser_layout);
            priv->current_parser_layout = NULL;
          }
          return;
        }

      if (priv->current_parser_layout->short_desc)
        maybe_replace (priv->layouts_by_short_desc,
                       priv->current_parser_layout->short_desc, priv->current_parser_layout);

      g_hash_table_replace (priv->layouts_table,
                            priv->current_parser_layout->id,
                            priv->current_parser_layout);
      priv->current_parser_layout = NULL;
    }
  else if (strcmp (element_name, "variant") == 0)
    {
      if (!priv->current_parser_variant->description || !priv->current_parser_variant->xkb_name)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements must enclose 'description' and 'name' elements");
          return;
        }

      priv->current_parser_variant->id = g_strjoin ("+",
                                                    priv->current_parser_layout->xkb_name,
                                                    priv->current_parser_variant->xkb_name,
                                                    NULL);

      if (priv->current_parser_variant->short_desc)
        maybe_replace (priv->layouts_by_short_desc,
                       priv->current_parser_variant->short_desc, priv->current_parser_variant);

      g_hash_table_replace (priv->layouts_table,
                            priv->current_parser_variant->id,
                            priv->current_parser_variant);
      priv->current_parser_variant = NULL;
    }
  else if (strcmp (element_name, "iso639Id") == 0)
    {
      gboolean replaced = FALSE;

      if (!priv->current_parser_iso639Id)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'iso639Id' elements must enclose text");
          return;
        }

      if (priv->current_parser_layout)
        replaced = maybe_replace (priv->layouts_by_iso639,
                                  priv->current_parser_iso639Id, priv->current_parser_layout);
      else if (priv->current_parser_variant)
        replaced = maybe_replace (priv->layouts_by_iso639,
                                  priv->current_parser_iso639Id, priv->current_parser_variant);

      if (!replaced)
        g_free (priv->current_parser_iso639Id);

      priv->current_parser_iso639Id = NULL;
    }
  else if (strcmp (element_name, "group") == 0)
    {
      if (!priv->current_parser_group->description || !priv->current_parser_group->id)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'group' elements must enclose 'description' and 'name' elements");
          return;
        }

      g_hash_table_replace (priv->option_groups_table,
                            priv->current_parser_group->id,
                            priv->current_parser_group);
      priv->current_parser_group = NULL;
    }
  else if (strcmp (element_name, "option") == 0)
    {
      if (!priv->current_parser_option->description || !priv->current_parser_option->id)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'option' elements must enclose 'description' and 'name' elements");
          return;
        }

      g_hash_table_replace (priv->current_parser_group->options_table,
                            priv->current_parser_option->id,
                            priv->current_parser_option);
      priv->current_parser_option = NULL;
    }
}

static void
parse_text (GMarkupParseContext  *context,
            const gchar          *text,
            gsize                 text_len,
            gpointer              data,
            GError              **error)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (data)->priv;

  if (priv->current_parser_text)
    {
      *priv->current_parser_text = g_strndup (text, text_len);
      priv->current_parser_text = NULL;
    }
}

static void
parse_error (GMarkupParseContext *context,
             GError              *error,
             gpointer             data)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (data)->priv;

  free_option_group (priv->current_parser_group);
  free_option (priv->current_parser_option);
  free_layout (priv->current_parser_layout);
  free_layout (priv->current_parser_variant);
  g_free (priv->current_parser_iso639Id);
}

static const GMarkupParser markup_parser = {
  parse_start_element,
  parse_end_element,
  parse_text,
  NULL,
  parse_error
};

static void
parse_rules_file (GnomeXkbInfo  *self,
                  const gchar   *path,
                  GError       **error)
{
  gchar *buffer;
  gsize length;
  GMarkupParseContext *context;
  GError *sub_error = NULL;

  g_file_get_contents (path, &buffer, &length, &sub_error);
  if (sub_error)
    {
      g_propagate_error (error, sub_error);
      return;
    }

  context = g_markup_parse_context_new (&markup_parser, 0, self, NULL);
  g_markup_parse_context_parse (context, buffer, length, &sub_error);
  g_markup_parse_context_free (context);
  g_free (buffer);
  if (sub_error)
    g_propagate_error (error, sub_error);
}

static void
parse_rules (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv = self->priv;
  GSettings *settings;
  gboolean show_all_sources;
  gchar *file_path;
  GError *error = NULL;

  /* Maps option group ids to XkbOptionGroup structs. Owns the
     XkbOptionGroup structs. */
  priv->option_groups_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL, free_option_group);
  priv->layouts_by_short_desc = g_hash_table_new (g_str_hash, g_str_equal);
  priv->layouts_by_iso639 = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* Maps layout ids to Layout structs. Owns the Layout structs. */
  priv->layouts_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_layout);

  file_path = get_xml_rules_file_path (".xml");
  parse_rules_file (self, file_path, &error);
  if (error)
    goto cleanup;
  g_free (file_path);

  settings = g_settings_new ("org.gnome.desktop.input-sources");
  show_all_sources = g_settings_get_boolean (settings, "show-all-sources");
  g_object_unref (settings);

  if (!show_all_sources)
    return;

  file_path = get_xml_rules_file_path (".extras.xml");
  parse_rules_file (self, file_path, &error);
  if (error)
    goto cleanup;
  g_free (file_path);

  return;

 cleanup:
  g_warning ("Failed to load XKB rules file %s: %s", file_path, error->message);

  if (error != NULL) {
    g_error_free (error);
    error = NULL;
  }

  if (file_path != NULL) {
    g_free (file_path);
    file_path = NULL;
  }

  if (priv->option_groups_table != NULL) {
    g_hash_table_destroy (priv->option_groups_table);
    priv->option_groups_table = NULL;
  }

  if (priv->layouts_by_short_desc != NULL) {
    g_hash_table_destroy (priv->layouts_by_short_desc);
    priv->layouts_by_short_desc = NULL;
  }

  if (priv->layouts_by_iso639 != NULL) {
    g_hash_table_destroy (priv->layouts_by_iso639);
    priv->layouts_by_iso639 = NULL;
  }

  if (priv->layouts_table != NULL) {
    g_hash_table_destroy (priv->layouts_table);
    priv->layouts_table = NULL;
  }
}

static gboolean
ensure_rules_are_parsed (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv = self->priv;

  if (!priv->layouts_table)
    parse_rules (self);

  return !!priv->layouts_table;
}

static void
gnome_xkb_info_init (GnomeXkbInfo *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_XKB_INFO, GnomeXkbInfoPrivate);
}

static void
gnome_xkb_info_finalize (GObject *self)
{
  GnomeXkbInfoPrivate *priv = GNOME_XKB_INFO (self)->priv;

  if (priv->option_groups_table)
    g_hash_table_destroy (priv->option_groups_table);
  if (priv->layouts_by_short_desc)
    g_hash_table_destroy (priv->layouts_by_short_desc);
  if (priv->layouts_by_iso639)
    g_hash_table_destroy (priv->layouts_by_iso639);
  if (priv->layouts_table)
    g_hash_table_destroy (priv->layouts_table);

  G_OBJECT_CLASS (gnome_xkb_info_parent_class)->finalize (self);
}

static void
gnome_xkb_info_class_init (GnomeXkbInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gnome_xkb_info_finalize;

  g_type_class_add_private (gobject_class, sizeof (GnomeXkbInfoPrivate));
}

/**
 * gnome_xkb_info_new:
 *
 * Returns: (transfer full): a new #GnomeXkbInfo instance.
 */
GnomeXkbInfo *
gnome_xkb_info_new (void)
{
  return g_object_new (GNOME_TYPE_XKB_INFO, NULL);
}

/**
 * gnome_xkb_info_get_all_layouts:
 * @self: a #GnomeXkbInfo
 *
 * Returns a list of all layout identifiers we know about.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of layout names. The caller takes ownership of the #GList but not
 * of the strings themselves, those are internally allocated and must
 * not be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_all_layouts (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  return g_hash_table_get_keys (priv->layouts_table);
}

/**
 * gnome_xkb_info_get_all_option_groups:
 * @self: a #GnomeXkbInfo
 *
 * Returns a list of all option group identifiers we know about.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of option group ids. The caller takes ownership of the #GList but
 * not of the strings themselves, those are internally allocated and
 * must not be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_all_option_groups (GnomeXkbInfo *self)
{
  GnomeXkbInfoPrivate *priv;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  return g_hash_table_get_keys (priv->option_groups_table);
}

/**
 * gnome_xkb_info_get_options_for_group:
 * @self: a #GnomeXkbInfo
 * @group_id: group's identifier about which to retrieve the options
 *
 * Returns a list of all option identifiers we know about for group
 * @group_id.
 *
 * Return value: (transfer container) (element-type utf8): the list
 * of option ids. The caller takes ownership of the #GList but not of
 * the strings themselves, those are internally allocated and must not
 * be modified.
 *
 * Since: 3.6
 */
GList *
gnome_xkb_info_get_options_for_group (GnomeXkbInfo *self,
                                      const gchar  *group_id)
{
  GnomeXkbInfoPrivate *priv;
  const XkbOptionGroup *group;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  group = g_hash_table_lookup (priv->option_groups_table, group_id);
  if (!group)
    return NULL;

  return g_hash_table_get_keys (group->options_table);
}

/**
 * gnome_xkb_info_description_for_option:
 * @self: a #GnomeXkbInfo
 * @group_id: identifier for group containing the option
 * @id: option identifier
 *
 * Return value: the translated description for the option @id.
 *
 * Since: 3.6
 */
const gchar *
gnome_xkb_info_description_for_option (GnomeXkbInfo *self,
                                       const gchar  *group_id,
                                       const gchar  *id)
{
  GnomeXkbInfoPrivate *priv;
  const XkbOptionGroup *group;
  const XkbOption *option;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), NULL);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return NULL;

  group = g_hash_table_lookup (priv->option_groups_table, group_id);
  if (!group)
    return NULL;

  option = g_hash_table_lookup (group->options_table, id);
  if (!option)
    return NULL;

  return XKEYBOARD_CONFIG_(option->description);
}

/**
 * gnome_xkb_info_get_layout_info:
 * @self: a #GnomeXkbInfo
 * @id: layout's identifier about which to retrieve the info
 * @display_name: (out) (allow-none) (transfer none): location to store
 * the layout's display name, or %NULL
 * @short_name: (out) (allow-none) (transfer none): location to store
 * the layout's short name, or %NULL
 * @xkb_layout: (out) (allow-none) (transfer none): location to store
 * the layout's XKB name, or %NULL
 * @xkb_variant: (out) (allow-none) (transfer none): location to store
 * the layout's XKB variant, or %NULL
 *
 * Retrieves information about a layout. Both @display_name and
 * @short_name are suitable to show in UIs and might be localized if
 * translations are available.
 *
 * Some layouts don't provide a short name (2 or 3 letters) or don't
 * specify a XKB variant, in those cases @short_name or @xkb_variant
 * are empty strings, i.e. "".
 *
 * If the given layout doesn't exist the return value is %FALSE and
 * all the (out) parameters are set to %NULL.
 *
 * Return value: %TRUE if the layout exists or %FALSE otherwise.
 *
 * Since: 3.6
 */
gboolean
gnome_xkb_info_get_layout_info (GnomeXkbInfo *self,
                                const gchar  *id,
                                const gchar **display_name,
                                const gchar **short_name,
                                const gchar **xkb_layout,
                                const gchar **xkb_variant)
{
  GnomeXkbInfoPrivate *priv;
  const Layout *layout;

  if (display_name)
    *display_name = NULL;
  if (short_name)
    *short_name = NULL;
  if (xkb_layout)
    *xkb_layout = NULL;
  if (xkb_variant)
    *xkb_variant = NULL;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), FALSE);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return FALSE;

  if (!g_hash_table_lookup_extended (priv->layouts_table, id, NULL, (gpointer *)&layout))
    return FALSE;

  if (display_name)
    *display_name = XKEYBOARD_CONFIG_(layout->description);

  if (!layout->is_variant)
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = "";
    }
  else
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc :
                        layout->main_layout->short_desc ? layout->main_layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->main_layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = layout->xkb_name;
    }

  return TRUE;
}

/**
 * gnome_xkb_info_get_layout_info_for_language:
 * @self: a #GnomeXkbInfo
 * @language: an ISO 639 code
 * @id: (out) (allow-none) (transfer none): location to store the
 * layout's indentifier, or %NULL
 * @display_name: (out) (allow-none) (transfer none): location to store
 * the layout's display name, or %NULL
 * @short_name: (out) (allow-none) (transfer none): location to store
 * the layout's short name, or %NULL
 * @xkb_layout: (out) (allow-none) (transfer none): location to store
 * the layout's XKB name, or %NULL
 * @xkb_variant: (out) (allow-none) (transfer none): location to store
 * the layout's XKB variant, or %NULL
 *
 * Retrieves the layout that better fits @language. It also fetches
 * information about that layout like gnome_xkb_info_get_layout_info().
 *
 * If a layout can't be found the return value is %FALSE and all the
 * (out) parameters are set to %NULL.
 *
 * Return value: %TRUE if a layout exists or %FALSE otherwise.
 *
 * Since: 3.6
 */
gboolean
gnome_xkb_info_get_layout_info_for_language (GnomeXkbInfo *self,
                                             const gchar  *language,
                                             const gchar **id,
                                             const gchar **display_name,
                                             const gchar **short_name,
                                             const gchar **xkb_layout,
                                             const gchar **xkb_variant)
{
  GnomeXkbInfoPrivate *priv;
  const Layout *layout;

  if (id)
    *id = NULL;
  if (display_name)
    *display_name = NULL;
  if (short_name)
    *short_name = NULL;
  if (xkb_layout)
    *xkb_layout = NULL;
  if (xkb_variant)
    *xkb_variant = NULL;

  g_return_val_if_fail (GNOME_IS_XKB_INFO (self), FALSE);

  priv = self->priv;

  if (!ensure_rules_are_parsed (self))
    return FALSE;

  /* First look in the proper language codes index, if we can't find
   * it there try again on the (untranslated) short descriptions since
   * sometimes those will give us a good match. */
  if (!g_hash_table_lookup_extended (priv->layouts_by_iso639, language, NULL, (gpointer *)&layout))
    if (!g_hash_table_lookup_extended (priv->layouts_by_short_desc, language, NULL, (gpointer *)&layout))
      return FALSE;

  if (id)
    *id = layout->id;
  if (display_name)
    *display_name = XKEYBOARD_CONFIG_(layout->description);

  if (!layout->is_variant)
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = "";
    }
  else
    {
      if (short_name)
        *short_name = XKEYBOARD_CONFIG_(layout->short_desc ? layout->short_desc :
                        layout->main_layout->short_desc ? layout->main_layout->short_desc : "");
      if (xkb_layout)
        *xkb_layout = layout->main_layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = layout->xkb_name;
    }

  return TRUE;
}
