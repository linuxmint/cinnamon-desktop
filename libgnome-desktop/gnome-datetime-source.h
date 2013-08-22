/* gnome-rr.h
 *
 * Copyright 2011, Red Hat, Inc.
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
 * Author: Colin Walters <walters@verbum.org>
 */

#ifndef GNOME_DATETIME_SOURCE_H
#define GNOME_DATETIME_SOURCE_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    This is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API
#endif

#include <glib.h>

GSource *_gnome_datetime_source_new (GDateTime *now,
				     GDateTime *expiry,
				     gboolean   cancel_on_set);

#endif /* GNOME_DATETIME_SOURCE_H */
