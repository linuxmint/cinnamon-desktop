/*
 * Copyright (C) 2002, 2017 Red Hat, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This file is part of the Cinnamon Desktop Library.
 *
 * The Cinnamon Desktop Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * The Cinnamon Desktop Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Cinnamon Desktop Library; see the file COPYING.LIB.
 * If not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

GBytes *_gnome_desktop_thumbnail_script_exec (const char  *cmd,
                                              int          size,
                                              const char  *uri,
                                              const char  *mime_type,
                                              GError     **error);

G_END_DECLS
