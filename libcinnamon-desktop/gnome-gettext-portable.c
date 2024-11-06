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
#include "config.h"

#include <locale.h>
#ifdef HAVE_XLOCALE
#include <xlocale.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#ifndef HAVE_USELOCALE

/*
 * FIXME: This function does nothing if there's no thread-safe
 * alternative to uselocale on some systems (NetBSD). Replace it
 * when an implementation becomes available.
 */
locale_t
uselocale (locale_t newloc)
{
	return (locale_t) 0;
}
#endif

char * 
dgettext_l (locale_t    locale, 
            const char *domain,
            const char *msgid)
{
	locale_t old_locale = uselocale (locale);
	char *ret = dgettext (domain, msgid);
	uselocale (old_locale);
	return ret;
}

const gchar *
g_dgettext_l (locale_t     locale,
              const gchar *domain,
              const gchar *msgid)
{
	locale_t old_locale = uselocale (locale);
	const gchar *ret = g_dgettext (domain, msgid);
	uselocale (old_locale);	
	return ret;
}

const gchar *
g_dpgettext_l (locale_t     locale,
               const gchar *domain,
               const gchar *msgctxtid,
               gsize        msgidoffset)
{
	locale_t old_locale = uselocale (locale);
	const gchar *ret = g_dpgettext (domain, msgctxtid, msgidoffset);
	uselocale (old_locale);
	return ret;
}
