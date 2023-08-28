/* gnome-systemd.h
 *
 * Copyright 2019 Benjamin Berg <bberg@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef _GNOME_SYSTEMD_H
#define _GNOME_SYSTEMD_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeDesktopSystemd is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-systemd.h
#endif

#include <gio/gio.h>

void gnome_start_systemd_scope (const char           *name,
                                gint32                pid,
                                const char           *description,
                                GDBusConnection      *connection,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data);

gboolean gnome_start_systemd_scope_finish (GAsyncResult  *res,
                                           GError       **error);

#endif /* _GNOME_SYSTEMD_H */
