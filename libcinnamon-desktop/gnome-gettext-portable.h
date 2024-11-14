/*-
 * Copyright (c) 2021 Dan Cîrnaț <dan@alt.md>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <glib.h>

#include <locale.h>
#include <string.h>

#ifndef GETTEXT_PACKAGE
#error "You must define GETTEXT_PACKAGE before including gnome-gettext-portable.h; did you forget to include config.h?"
#endif

G_BEGIN_DECLS

const char * 
dgettext_l (locale_t    locale,
            const char *domain,
            const char *msgid);

const gchar *
g_dgettext_l (locale_t    locale,
              const char *domain,
              const char *msgid);

const gchar *
g_dpgettext_l (locale_t    locale,
               const char *domain,
               const char *msgctxtid,
               gsize       msgidoffset);

#define L_(Locale,String) ((char *) g_dgettext_l (Locale, GETTEXT_PACKAGE, String))

G_END_DECLS
