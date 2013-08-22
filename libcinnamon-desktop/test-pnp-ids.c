/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <libgnome-desktop/gnome-pnp-ids.h>

#define NUM_ITERS 500 * 1000

static void
show_vendor (GnomePnpIds *ids, const char *id)
{
	char *vendor;
	vendor = gnome_pnp_ids_get_pnp_id (ids, id);
	g_message ("%s => %s", id, vendor);
	g_free (vendor);
}

int
main (int argc, char *argv[])
{
	GnomePnpIds *ids;
	guint i;

	g_type_init ();

	ids = gnome_pnp_ids_new ();

	if (argc == 2 &&
	    g_str_equal (argv[1], "--timed")) {
		GTimer *timer;
		gdouble elapsed;

		timer = g_timer_new ();

		for (i = 0; i < NUM_ITERS; i++) {
			char *vendor;
			vendor = gnome_pnp_ids_get_pnp_id (ids, "ZZZ");
			g_free (vendor);
		}

		elapsed = g_timer_elapsed (timer, NULL);
		g_timer_destroy (timer);

		g_message ("%d iterations took %lf seconds", NUM_ITERS, elapsed);
		g_object_unref (ids);

		return 0;
	}


	for (i = 1; i < argc; i++)
		show_vendor (ids, argv[i]);
	if (argc < 2) {
		show_vendor (ids, "ELO");
		show_vendor (ids, "IBM");
	}

	g_object_unref (ids);
	return 0;
}
