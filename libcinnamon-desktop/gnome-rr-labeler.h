/* gnome-rr-labeler.h - Utility to label monitors to identify them
 * while they are being configured.
 *
 * Copyright 2008, Novell, Inc.
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
 * Author: Federico Mena-Quintero <federico@novell.com>
 */

#ifndef GNOME_RR_LABELER_H
#define GNOME_RR_LABELER_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnomerr.h
#endif

#include <libgnome-desktop/gnome-rr-config.h>

#define GNOME_TYPE_RR_LABELER            (gnome_rr_labeler_get_type ())
#define GNOME_RR_LABELER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_LABELER, GnomeRRLabeler))
#define GNOME_RR_LABELER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_RR_LABELER, GnomeRRLabelerClass))
#define GNOME_IS_RR_LABELER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_LABELER))
#define GNOME_IS_RR_LABELER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_RR_LABELER))
#define GNOME_RR_LABELER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_RR_LABELER, GnomeRRLabelerClass))

typedef struct _GnomeRRLabeler GnomeRRLabeler;
typedef struct _GnomeRRLabelerClass GnomeRRLabelerClass;
typedef struct _GnomeRRLabelerPrivate GnomeRRLabelerPrivate;

struct _GnomeRRLabeler {
	GObject parent;

	/*< private >*/
	GnomeRRLabelerPrivate *priv;
};

struct _GnomeRRLabelerClass {
	GObjectClass parent_class;
};

GType gnome_rr_labeler_get_type (void);

GnomeRRLabeler *gnome_rr_labeler_new (GnomeRRConfig *config);

void gnome_rr_labeler_show (GnomeRRLabeler *labeler);

void gnome_rr_labeler_hide (GnomeRRLabeler *labeler);

void gnome_rr_labeler_get_rgba_for_output (GnomeRRLabeler *labeler, GnomeRROutputInfo *output, GdkRGBA *rgba_out);

#endif
