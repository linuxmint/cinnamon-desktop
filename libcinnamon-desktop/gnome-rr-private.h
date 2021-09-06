#ifndef GNOME_RR_PRIVATE_H
#define GNOME_RR_PRIVATE_H

#include <X11/Xlib.h>

#include <X11/extensions/Xrandr.h>

#define MINIMUM_LOGICAL_SCALE_FACTOR 0.74f
#define MAXIMUM_LOGICAL_SCALE_FACTOR 3.0f
#define MINIMUM_GLOBAL_SCALE_FACTOR 1
#define MAXIMUM_GLOBAL_SCALE_FACTOR 3

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    XRRScreenResources *resources;
    
    GnomeRROutput **	outputs;
    GnomeRRCrtc **	crtcs;
    GnomeRRMode **	modes;
    
    GnomeRRScreen *	screen;

    GnomeRRMode **	clone_modes;

    RROutput            primary;
};

struct GnomeRRScreenPrivate
{
    GdkScreen *			gdk_screen;
    GdkWindow *			gdk_root;
    Display *			xdisplay;
    Screen *			xscreen;
    Window			xroot;
    ScreenInfo *		info;
    GSettings *         interface_settings;
    
    int				randr_event_base;
    int				rr_major_version;
    int				rr_minor_version;
    
    Atom                        connector_type_atom;
    gboolean                    dpms_capable;
};

struct GnomeRROutputInfoPrivate
{
    char *		name;

    gboolean		on;
    int			width;
    int			height;
    double		rate;
    int			x;
    int			y;
    GnomeRRRotation	rotation;

    gboolean		connected;
    gchar		vendor[4];
    guint		product;
    guint		serial;
    double		aspect;
    int			pref_width;
    int			pref_height;
    char *		display_name;
    gboolean            primary;
    float       scale;
    gboolean    doublescan;
    gboolean    interlaced;
    gboolean    vsync;
};

struct GnomeRRConfigPrivate
{
  gboolean clone;
  GnomeRRScreen *screen;
  GnomeRROutputInfo **outputs;
  guint base_scale;
  gboolean auto_scale;
};

gboolean _gnome_rr_output_name_is_laptop (const char *name);

#endif
