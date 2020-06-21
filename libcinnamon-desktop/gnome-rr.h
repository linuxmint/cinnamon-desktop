/* gnome-rr.h
 *
 * Copyright 2007, 2008, Red Hat, Inc.
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
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#ifndef GNOME_RR_H
#define GNOME_RR_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnomerr.h
#endif

#include <glib.h>
#include <gdk/gdk.h>

typedef struct GnomeRRScreenPrivate GnomeRRScreenPrivate;
typedef struct GnomeRROutput GnomeRROutput;
typedef struct GnomeRRCrtc GnomeRRCrtc;
typedef struct GnomeRRMode GnomeRRMode;

typedef struct {
	GObject parent;

	GnomeRRScreenPrivate* priv;
} GnomeRRScreen;

typedef struct {
	GObjectClass parent_class;

        void (* changed) (void);
        void (* output_connected)       (GnomeRROutput *output);
        void (* output_disconnected)    (GnomeRROutput *output);
} GnomeRRScreenClass;

typedef enum
{
    GNOME_RR_ROTATION_NEXT =	0,
    GNOME_RR_ROTATION_0 =	(1 << 0),
    GNOME_RR_ROTATION_90 =	(1 << 1),
    GNOME_RR_ROTATION_180 =	(1 << 2),
    GNOME_RR_ROTATION_270 =	(1 << 3),
    GNOME_RR_REFLECT_X =	(1 << 4),
    GNOME_RR_REFLECT_Y =	(1 << 5)
} GnomeRRRotation;

typedef enum {
	GNOME_RR_DPMS_ON,
	GNOME_RR_DPMS_STANDBY,
	GNOME_RR_DPMS_SUSPEND,
	GNOME_RR_DPMS_OFF,
	GNOME_RR_DPMS_DISABLED,
	GNOME_RR_DPMS_UNKNOWN
} GnomeRRDpmsMode;

/* Error codes */

#define GNOME_RR_ERROR (gnome_rr_error_quark ())

GQuark gnome_rr_error_quark (void);

typedef enum {
    GNOME_RR_ERROR_UNKNOWN,		/* generic "fail" */
    GNOME_RR_ERROR_NO_RANDR_EXTENSION,	/* RANDR extension is not present */
    GNOME_RR_ERROR_RANDR_ERROR,		/* generic/undescribed error from the underlying XRR API */
    GNOME_RR_ERROR_BOUNDS_ERROR,	/* requested bounds of a CRTC are outside the maximum size */
    GNOME_RR_ERROR_CRTC_ASSIGNMENT,	/* could not assign CRTCs to outputs */
    GNOME_RR_ERROR_NO_MATCHING_CONFIG,	/* none of the saved configurations matched the current configuration */
    GNOME_RR_ERROR_NO_DPMS_EXTENSION	/* DPMS extension is not present */
} GnomeRRError;

#define GNOME_RR_CONNECTOR_TYPE_PANEL "Panel"  /* This is a laptop's built-in LCD */

#define GNOME_TYPE_RR_SCREEN                  (gnome_rr_screen_get_type())
#define GNOME_RR_SCREEN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_SCREEN, GnomeRRScreen))
#define GNOME_IS_RR_SCREEN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_SCREEN))
#define GNOME_RR_SCREEN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_RR_SCREEN, GnomeRRScreenClass))
#define GNOME_IS_RR_SCREEN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_RR_SCREEN))
#define GNOME_RR_SCREEN_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_RR_SCREEN, GnomeRRScreenClass))

#define GNOME_TYPE_RR_OUTPUT (gnome_rr_output_get_type())
#define GNOME_TYPE_RR_CRTC   (gnome_rr_crtc_get_type())
#define GNOME_TYPE_RR_MODE   (gnome_rr_mode_get_type())

GType gnome_rr_screen_get_type (void);
GType gnome_rr_output_get_type (void);
GType gnome_rr_crtc_get_type (void);
GType gnome_rr_mode_get_type (void);

/* GnomeRRScreen */
GnomeRRScreen * gnome_rr_screen_new                (GdkScreen             *screen,
						    GError               **error);
GnomeRROutput **gnome_rr_screen_list_outputs       (GnomeRRScreen         *screen);
GnomeRRCrtc **  gnome_rr_screen_list_crtcs         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_modes         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_clone_modes   (GnomeRRScreen	  *screen);
void            gnome_rr_screen_set_size           (GnomeRRScreen         *screen,
						    int                    width,
						    int                    height,
						    int                    mm_width,
						    int                    mm_height);
GnomeRRCrtc *   gnome_rr_screen_get_crtc_by_id     (GnomeRRScreen         *screen,
						    guint32                id);
gboolean        gnome_rr_screen_refresh            (GnomeRRScreen         *screen,
						    GError               **error);
GnomeRROutput * gnome_rr_screen_get_output_by_id   (GnomeRRScreen         *screen,
						    guint32                id);
GnomeRROutput * gnome_rr_screen_get_output_by_name (GnomeRRScreen         *screen,
						    const char            *name);
void            gnome_rr_screen_get_ranges         (GnomeRRScreen         *screen,
						    int                   *min_width,
						    int                   *max_width,
						    int                   *min_height,
						    int                   *max_height);
void            gnome_rr_screen_get_timestamps     (GnomeRRScreen         *screen,
						    guint32               *change_timestamp_ret,
						    guint32               *config_timestamp_ret);

void            gnome_rr_screen_set_primary_output (GnomeRRScreen         *screen,
                                                    GnomeRROutput         *output);

GnomeRRMode   **gnome_rr_screen_create_clone_modes (GnomeRRScreen *screen);

gboolean        gnome_rr_screen_get_dpms_mode      (GnomeRRScreen        *screen,
                                                    GnomeRRDpmsMode       *mode,
                                                    GError               **error);
gboolean        gnome_rr_screen_set_dpms_mode      (GnomeRRScreen         *screen,
                                                    GnomeRRDpmsMode        mode,
                                                    GError              **error);
guint             gnome_rr_screen_get_global_scale (GnomeRRScreen   *screen);
guint             gnome_rr_screen_get_global_scale_setting (GnomeRRScreen   *screen);
void            gnome_rr_screen_set_global_scale_setting (GnomeRRScreen   *screen,
                                                    guint            scale_factor);
gboolean        gnome_rr_screen_get_use_upscaling  (GnomeRRScreen        *screen);
float *         gnome_rr_screen_calculate_supported_scales (GnomeRRScreen     *screen,
                                                            int                width,
                                                            int                height,
                                                            int               *n_supported_scales);
/* screen class method, used in csd-xsettings and here */
guint             gnome_rr_screen_calculate_best_global_scale (GnomeRRScreen *screen, gint index);

/* GnomeRROutput */
guint32         gnome_rr_output_get_id             (GnomeRROutput         *output);
const char *    gnome_rr_output_get_name           (GnomeRROutput         *output);
gboolean        gnome_rr_output_is_connected       (GnomeRROutput         *output);
int             gnome_rr_output_get_size_inches    (GnomeRROutput         *output);
int             gnome_rr_output_get_width_mm       (GnomeRROutput         *outout);
int             gnome_rr_output_get_height_mm      (GnomeRROutput         *output);
const guint8 *  gnome_rr_output_get_edid_data      (GnomeRROutput         *output,
                                                    gsize                 *size);
gboolean        gnome_rr_output_get_ids_from_edid  (GnomeRROutput         *output,
                                                    char                 **vendor,
                                                    int                   *product,
                                                    int                   *serial);

gint            gnome_rr_output_get_backlight_min  (GnomeRROutput         *output);
gint            gnome_rr_output_get_backlight_max  (GnomeRROutput         *output);
gint            gnome_rr_output_get_backlight      (GnomeRROutput         *output,
                                                    GError                **error);
gboolean        gnome_rr_output_set_backlight      (GnomeRROutput         *output,
                                                    gint                   value,
                                                    GError                **error);

GnomeRRCrtc **  gnome_rr_output_get_possible_crtcs (GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_output_get_current_mode   (GnomeRROutput         *output);
GnomeRRCrtc *   gnome_rr_output_get_crtc           (GnomeRROutput         *output);
const char *    gnome_rr_output_get_connector_type (GnomeRROutput         *output);
gboolean        gnome_rr_output_is_laptop          (GnomeRROutput         *output);
void            gnome_rr_output_get_position       (GnomeRROutput         *output,
						    int                   *x,
						    int                   *y);
gboolean        gnome_rr_output_can_clone          (GnomeRROutput         *output,
						    GnomeRROutput         *clone);
GnomeRRMode **  gnome_rr_output_list_modes         (GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_output_get_preferred_mode (GnomeRROutput         *output);
gboolean        gnome_rr_output_supports_mode      (GnomeRROutput         *output,
						    GnomeRRMode           *mode);
gboolean        gnome_rr_output_get_is_primary     (GnomeRROutput         *output);

/* GnomeRRMode */
guint32         gnome_rr_mode_get_id               (GnomeRRMode           *mode);
guint           gnome_rr_mode_get_width            (GnomeRRMode           *mode);
guint           gnome_rr_mode_get_height           (GnomeRRMode           *mode);
int             gnome_rr_mode_get_freq             (GnomeRRMode           *mode);
double          gnome_rr_mode_get_freq_f           (GnomeRRMode           *mode);
void            gnome_rr_mode_get_flags            (GnomeRRMode           *mode,
                                                    gboolean              *doublescan,
                                                    gboolean              *interlaced,
                                                    gboolean              *vsync);
/* GnomeRRCrtc */
guint32         gnome_rr_crtc_get_id               (GnomeRRCrtc           *crtc);

gboolean        gnome_rr_crtc_set_config_with_time (GnomeRRCrtc           *crtc,
						    guint32                timestamp,
						    int                    x,
						    int                    y,
						    GnomeRRMode           *mode,
						    GnomeRRRotation        rotation,
						    GnomeRROutput        **outputs,
						    int                    n_outputs,
                            float                  scale,
                            guint                  global_scale,
						    GError               **error);
gboolean        gnome_rr_crtc_can_drive_output     (GnomeRRCrtc           *crtc,
						    GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_crtc_get_current_mode     (GnomeRRCrtc           *crtc);
void            gnome_rr_crtc_get_position         (GnomeRRCrtc           *crtc,
						    int                   *x,
						    int                   *y);
float           gnome_rr_crtc_get_scale            (GnomeRRCrtc           *crtc);
GnomeRRRotation gnome_rr_crtc_get_current_rotation (GnomeRRCrtc           *crtc);
GnomeRRRotation gnome_rr_crtc_get_rotations        (GnomeRRCrtc           *crtc);
gboolean        gnome_rr_crtc_supports_rotation    (GnomeRRCrtc           *crtc,
						    GnomeRRRotation        rotation);

gboolean        gnome_rr_crtc_get_gamma            (GnomeRRCrtc           *crtc,
						    int                   *size,
						    unsigned short       **red,
						    unsigned short       **green,
						    unsigned short       **blue);
void            gnome_rr_crtc_set_gamma            (GnomeRRCrtc           *crtc,
						    int                    size,
						    unsigned short        *red,
						    unsigned short        *green,
						    unsigned short        *blue);
#endif /* GNOME_RR_H */
