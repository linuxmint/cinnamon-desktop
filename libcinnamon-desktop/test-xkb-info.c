#include <gtk/gtk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>
int
main (int argc, char **argv)
{
	GnomeXkbInfo *info;
	GList *layouts, *l;
	GList *option_groups, *g;
	GList *options, *o;

	gtk_init (&argc, &argv);

	info = gnome_xkb_info_new ();

	layouts = gnome_xkb_info_get_all_layouts (info);
	for (l = layouts; l != NULL; l = l->next) {
		const char *id = l->data;
		const char *display_name;
		const char *short_name;
		const char *xkb_layout;
		const char *xkb_variant;

		if (gnome_xkb_info_get_layout_info (info, id,
						    &display_name,
						    &short_name,
						    &xkb_layout,
						    &xkb_variant) == FALSE) {
			g_warning ("Failed to get info for layout '%s'", id);
		} else {
			g_print ("id: %s\n", id);
			g_print ("\tdisplay name: %s\n", display_name);
			g_print ("\tshort name: %s\n", short_name);
			g_print ("\txkb layout: %s\n", xkb_layout);
			g_print ("\txkb variant: %s\n", xkb_variant);
		}
	}
	g_list_free (layouts);

	option_groups = gnome_xkb_info_get_all_option_groups (info);
	for (g = option_groups; g != NULL; g = g->next) {
		const char *group_id = g->data;

		g_print ("\noption group: %s\n", group_id);

		options = gnome_xkb_info_get_options_for_group (info, group_id);
		for (o = options; o != NULL; o = o->next) {
			const char *id = o->data;

			g_print ("id: %s\n", id);
			g_print ("\tdescription: %s\n",
				 gnome_xkb_info_description_for_option (info,
									group_id,
									id));
		}
		g_list_free (options);
	}
	g_list_free (option_groups);

	g_object_unref (info);

	return 0;
}
