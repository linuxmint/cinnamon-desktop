#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>

static void
clock_changed (GObject    *object,
	       GParamSpec *pspec,
	       gpointer    user_data)
{
	const char *txt;

	txt = gnome_wall_clock_get_clock (GNOME_WALL_CLOCK (object));
	g_print ("%s\n", txt);
}

int main (int argc, char **argv)
{
	GMainLoop *loop;
	GnomeWallClock *clock;

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	g_type_init ();

	loop = g_main_loop_new (NULL, FALSE);

	clock = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
	g_signal_connect (G_OBJECT (clock), "notify::clock",
			  G_CALLBACK (clock_changed), NULL);

	g_main_loop_run (loop);

	return 0;
}
