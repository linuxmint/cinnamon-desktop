#ifndef GNOME_RR_PRIVATE_H
#define GNOME_RR_PRIVATE_H

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
enum wl_output_transform {
  WL_OUTPUT_TRANSFORM_NORMAL,
  WL_OUTPUT_TRANSFORM_90,
  WL_OUTPUT_TRANSFORM_180,
  WL_OUTPUT_TRANSFORM_270,
  WL_OUTPUT_TRANSFORM_FLIPPED,
  WL_OUTPUT_TRANSFORM_FLIPPED_90,
  WL_OUTPUT_TRANSFORM_FLIPPED_180,
  WL_OUTPUT_TRANSFORM_FLIPPED_270
};
#endif

#include "meta-xrandr-shared.h"
#include "meta-dbus-xrandr.h"

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    guint               serial;
    
    GnomeRROutput **	outputs;
    GnomeRRCrtc **	crtcs;
    GnomeRRMode **	modes;
    
    GnomeRRScreen *	screen;

    GnomeRRMode **	clone_modes;

    GnomeRROutput *     primary;
};

struct GnomeRRScreenPrivate
{
    GdkScreen *			gdk_screen;
    GdkWindow *			gdk_root;
    ScreenInfo *		info;

    int                         init_name_watch_id;
    MetaDBusDisplayConfig      *proxy;
};

#define UNDEFINED_GROUP_ID 0
struct GnomeRRTile {
  guint group_id;
  guint flags;
  guint max_horiz_tiles;
  guint max_vert_tiles;
  guint loc_horiz;
  guint loc_vert;
  guint width;
  guint height;
};

typedef struct GnomeRRTile GnomeRRTile;

struct _GnomeRROutputInfoPrivate
{
    char *		name;

    gboolean		on;
    int			width;
    int			height;
    int			rate;
    int			x;
    int			y;
    GnomeRRRotation	rotation;
    GnomeRRRotation	available_rotations;

    gboolean		connected;
    char *		vendor;
    char *		product;
    char *		serial;
    double		aspect;
    int			pref_width;
    int			pref_height;
    char *		display_name;
    char *		connector_type;
    gboolean            primary;
    gboolean            underscanning;

    gboolean            is_tiled;
    GnomeRRTile         tile;

    int                 total_tiled_width;
    int                 total_tiled_height;
    /* ptr back to info */
    GnomeRRConfig       *config;
};

struct _GnomeRRConfigPrivate
{
  gboolean clone;
  GnomeRRScreen *screen;
  GnomeRROutputInfo **outputs;
};

gboolean _gnome_rr_output_connector_type_is_builtin_display (const char *connector_type);

gboolean _gnome_rr_screen_apply_configuration (GnomeRRScreen  *screen,
					       gboolean        persistent,
					       GVariant       *crtcs,
					       GVariant       *outputs,
					       GError        **error);

const char * _gnome_rr_output_get_connector_type  (GnomeRROutput         *output);
gboolean    _gnome_rr_output_get_tile_info      (GnomeRROutput         *output,
						GnomeRRTile *tile);
gboolean    _gnome_rr_output_get_tiled_display_size (GnomeRROutput *output,
						     int *tile_w, int *tile_h,
						     int *width, int *height);

#endif
