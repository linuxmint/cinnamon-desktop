/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-ditem.h - Utilities for the GNOME Desktop

   Copyright (C) 1998 Tom Tromey
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */
/*
  @NOTATION@
 */

#ifndef GNOME_DESKTOP_UTILS_H
#define GNOME_DESKTOP_UTILS_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    gnome-desktop-utils is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-desktop-utils.h
#endif

#include <glib.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* prepend the terminal command to a vector */
void gnome_desktop_prepend_terminal_to_vector (int *argc, char ***argv);

const char *gnome_desktop_get_media_key_string (gint type);

G_END_DECLS

#endif /* GNOME_DITEM_H */
