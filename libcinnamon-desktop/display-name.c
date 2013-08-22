/*
 * Copyright 2007 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Author: Soren Sandmann <sandmann@redhat.com> */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "gnome-pnp-ids.h"
#include "edid.h"

static const char *
find_vendor (const char *code)
{
    const char *vendor_name;
    GnomePnpIds *pnp_ids;

    pnp_ids = gnome_pnp_ids_new ();
    vendor_name = gnome_pnp_ids_get_pnp_id (pnp_ids, code);
    g_object_unref (pnp_ids);

    if (vendor_name)
        return vendor_name;

    return code;
}

static const double known_diagonals[] = {
    12.1,
    13.3,
    15.6
};

static char *
diagonal_to_str (double d)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
        double delta;

        delta = fabs(known_diagonals[i] - d);
        if (delta < 0.1)
            return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

    return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

char *
make_display_name (const MonitorInfo *info)
{
    const char *vendor;
    int width_mm, height_mm;
    char *inches, *ret;

    if (info)
    {
	vendor = find_vendor (info->manufacturer_code);
    }
    else
    {
        /* Translators: "Unknown" here is used to identify a monitor for which
         * we don't know the vendor. When a vendor is known, the name of the
         * vendor is used. */
	vendor = C_("Monitor vendor", "Unknown");
    }

    if (info && info->width_mm != -1 && info->height_mm)
    {
	width_mm = info->width_mm;
	height_mm = info->height_mm;
    }
    else if (info && info->n_detailed_timings)
    {
	width_mm = info->detailed_timings[0].width_mm;
	height_mm = info->detailed_timings[0].height_mm;
    }
    else
    {
	width_mm = -1;
	height_mm = -1;
    }
    
    if (width_mm != -1 && height_mm != -1)
    {
	double d = sqrt (width_mm * width_mm + height_mm * height_mm);

	inches = diagonal_to_str (d / 25.4);
    }
    else
    {
	inches = NULL;
    }

    if (!inches)
	return g_strdup (vendor);

    ret = g_strdup_printf ("%s %s", vendor, inches);
    g_free (inches);

    return ret;
}
