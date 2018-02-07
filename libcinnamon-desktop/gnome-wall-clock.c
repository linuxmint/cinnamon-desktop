/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 * gnome-wall-clock.h - monitors TZ setting files and signals changes
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-wall-clock.h"
#include "gnome-datetime-source.h"

typedef enum
{
  INTERVAL_SECOND,
  INTERVAL_MINUTE,
} ClockInterval;

struct _GnomeWallClockPrivate {
	guint clock_update_id;

    char *clock_string;

    const char *default_time_format;
    const char *default_date_format;
    char *format_string;
    gboolean custom_format;

	GFileMonitor *tz_monitor;
	GSettings    *desktop_settings;

    ClockInterval update_interval;
};

enum {
	PROP_0,
	PROP_CLOCK,
	PROP_FORMAT_STRING,
};

G_DEFINE_TYPE (GnomeWallClock, gnome_wall_clock, G_TYPE_OBJECT);

/* Date/Time format defaults - options are stored in org.cinnamon.desktop.interface keys.
 * The wall clock is used variously in Cinnamon applets and desklets, as well as
 * cinnamon-screensaver's default lock screen. */

/* Default date format (typically matching date portion of WITH_DATE_* defaults.)
 * Currently used by cinnamon-screensaver default clock */
#define DATE_ONLY             (_("%A, %B %e"))

/* Defaut date/time format when show-date, show-seconds, use-24h are set */
#define WITH_DATE_24H_SECONDS (_("%A %B %e, %R:%S"))

/* Default date/time format when show-date, show-seconds are set */
#define WITH_DATE_12H_SECONDS (_("%A %B %e, %l:%M:%S %p"))

/* Default date/time format when show-date, use-24h are set */
#define WITH_DATE_24H         (_("%A %B %e, %R"))

/* Default date/time format when just show-date is set */
#define WITH_DATE_12H         (_("%A %B %e, %l:%M %p"))

/* Default date/time format when show-seconds, use-24h are set */
#define NO_DATE_24H_SECONDS   (_("%R:%S"))

/* Default date/time format when just show-seconds is set */
#define NO_DATE_12H_SECONDS   (_("%l:%M:%S %p"))

/* Default date/time format when just use-24h is set */
#define NO_DATE_24H           (_("%R"))

/* Default date/time format with no options are set */
#define NO_DATE_12H           (_("%l:%M %p"))

#define NO_DATE               ("")

/************/

static void update_format_string (GnomeWallClock *self, const gchar *format_string);
static gboolean update_clock (gpointer data);
static void on_schema_change (GSettings *schema,
                              const char *key,
                              gpointer user_data);
static void on_tz_changed (GFileMonitor *monitor,
                           GFile        *file,
                           GFile        *other_file,
                           GFileMonitorEvent *event,
                           gpointer      user_data);

static void
gnome_wall_clock_init (GnomeWallClock *self)
{
	GFile *tz;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_WALL_CLOCK, GnomeWallClockPrivate);
	
	self->priv->clock_string = NULL;
	
	tz = g_file_new_for_path ("/etc/localtime");
	self->priv->tz_monitor = g_file_monitor_file (tz, 0, NULL, NULL);
	g_object_unref (tz);
	
	g_signal_connect (self->priv->tz_monitor, "changed", G_CALLBACK (on_tz_changed), self);
	
	self->priv->desktop_settings = g_settings_new ("org.cinnamon.desktop.interface");
	g_signal_connect (self->priv->desktop_settings, "changed", G_CALLBACK (on_schema_change), self);

     /* A format string provided for construction will be set after gnome_wall_clock_init()
      * finishes.  If not provided, our internal format and interval will still be set to
      * some default by this. */
    gnome_wall_clock_set_format_string (self, NULL);
}

static void
gnome_wall_clock_dispose (GObject *object)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (object);

	if (self->priv->clock_update_id) {
		g_source_remove (self->priv->clock_update_id);
		self->priv->clock_update_id = 0;
	}

	if (self->priv->tz_monitor != NULL) {
		g_object_unref (self->priv->tz_monitor);
		self->priv->tz_monitor = NULL;
	}

	if (self->priv->desktop_settings != NULL) {
		g_object_unref (self->priv->desktop_settings);
		self->priv->desktop_settings = NULL;
	}

	G_OBJECT_CLASS (gnome_wall_clock_parent_class)->dispose (object);
}

static void
gnome_wall_clock_finalize (GObject *object)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (object);

    g_clear_pointer (&self->priv->clock_string, g_free);
    g_clear_pointer (&self->priv->format_string, g_free);

	G_OBJECT_CLASS (gnome_wall_clock_parent_class)->finalize (object);
}

static void
gnome_wall_clock_get_property (GObject    *gobject,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (gobject);

	switch (prop_id)
	{
	case PROP_CLOCK:
		g_value_set_string (value, self->priv->clock_string);
		break;
    case PROP_FORMAT_STRING:
        g_value_set_string (value, self->priv->format_string);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
		break;
	}
}

static void
gnome_wall_clock_set_property (GObject      *gobject,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	GnomeWallClock *self = GNOME_WALL_CLOCK (gobject);

	switch (prop_id)
	{
    case PROP_FORMAT_STRING:
        gnome_wall_clock_set_format_string (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
		break;
	}
}


static void
gnome_wall_clock_class_init (GnomeWallClockClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gnome_wall_clock_get_property;
	gobject_class->set_property = gnome_wall_clock_set_property;
	gobject_class->dispose = gnome_wall_clock_dispose;
	gobject_class->finalize = gnome_wall_clock_finalize;

	/**
	 * GnomeWallClock:clock:
	 *
	 * A formatted string representing the current clock display.
	 */
	g_object_class_install_property (gobject_class,
					 PROP_CLOCK,
					 g_param_spec_string ("clock",
							      "",
							      "",
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * GnomeWallClock:format-string:
	 *
	 * If not NULL, the wall clock will format the time/date according to
     * this format string.  If the format string is invalid, the default string
     * will be used instead.
	 */
    g_object_class_install_property (gobject_class,
                                     PROP_FORMAT_STRING,
                                     g_param_spec_string ("format-string",
                                     "The string to format the clock to",
                                     "The string to format the clock to",
                                     NULL,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_type_class_add_private (gobject_class, sizeof (GnomeWallClockPrivate));
}

static void
update_format_string (GnomeWallClock *self, const gchar *format_string)
{
    guint i;
    gchar *old_format;
    gboolean use_24h, show_date, show_seconds;
    const gchar *default_format;

    static const gchar* seconds_tokens[] = {
        "\%s",
        "\%S",
        "\%T",
        "\%X",
        "\%c"
    };

    ClockInterval new_interval = INTERVAL_MINUTE;
    gchar *new_format = NULL;

    /* First parse the settings and fill out our default format strings - 
     * Date-only, Time-only, and combined.
     */

    use_24h = g_settings_get_boolean (self->priv->desktop_settings, "clock-use-24h");
    show_date = g_settings_get_boolean (self->priv->desktop_settings, "clock-show-date");
    show_seconds = g_settings_get_boolean (self->priv->desktop_settings, "clock-show-seconds");

    if (use_24h) {
        if (show_date) {
            /* Translators: This is the time format with full date used
               in 24-hour mode. */
            if (show_seconds) {
                default_format = WITH_DATE_24H_SECONDS;
                self->priv->default_time_format = NO_DATE_24H_SECONDS;
            } else {
                default_format = WITH_DATE_24H;
                self->priv->default_time_format = NO_DATE_24H;
            }

            self->priv->default_date_format = DATE_ONLY;
        } else {
            /* Translators: This is the time format without date used
               in 24-hour mode. */
            if (show_seconds) {
                default_format = NO_DATE_24H_SECONDS;
                self->priv->default_time_format = NO_DATE_24H_SECONDS;
            } else {
                default_format = NO_DATE_24H;
                self->priv->default_time_format = NO_DATE_24H;
            }

            self->priv->default_date_format = NO_DATE;
        }
    } else {
        if (show_date) {
            /* Translators: This is a time format with full date used
               for AM/PM. */
            if (show_seconds) {
                default_format = WITH_DATE_12H_SECONDS;
                self->priv->default_time_format = NO_DATE_12H_SECONDS;
            } else {
                default_format = WITH_DATE_12H;
                self->priv->default_time_format = NO_DATE_12H;
            }

            self->priv->default_date_format = DATE_ONLY;
        } else {
            /* Translators: This is a time format without date used
               for AM/PM. */
            if (show_seconds) {
                default_format = NO_DATE_12H_SECONDS;
                self->priv->default_time_format = NO_DATE_12H_SECONDS;
            } else {
                default_format = NO_DATE_12H;
                self->priv->default_time_format = NO_DATE_12H;
            }

            self->priv->default_date_format = NO_DATE;
        }
    }

    /* Then look at our custom format if we received one, and test it out.
     * If it's ok, it's used, otherwise we use the default format */

    if (format_string != NULL) {
        GDateTime *test_now;
        gchar *str;

        test_now = g_date_time_new_now_local ();
        str = g_date_time_format (test_now, format_string);

        if (str != NULL) {
            new_format = g_strdup (format_string);
        }

        g_clear_pointer (&str, g_free);
    }

    if (new_format == NULL) {
        new_format = g_strdup (default_format);
    }

    /* Now determine whether we need seconds ticking or not */

    for (i = 0; i < G_N_ELEMENTS (seconds_tokens); i++) {
        if (g_strstr_len (new_format, -1, seconds_tokens[i])) {
            new_interval = INTERVAL_SECOND;
            break;
        }
    }

    old_format = self->priv->format_string;

    self->priv->format_string = new_format;
    self->priv->update_interval = new_interval;

    g_free (old_format);

    g_debug ("Updated format string and interval.  '%s', update every %s.",
             new_format,
             new_interval == 1 ? "minute" : "second");
}

static gboolean
update_clock (gpointer data)
{
	GnomeWallClock   *self = data;

	GSource *source;
	GDateTime *now;
	GDateTime *expiry;

	now = g_date_time_new_now_local ();

    /* Setup the next update */

    if (self->priv->update_interval == INTERVAL_SECOND) {
        expiry = g_date_time_add_seconds (now, 1);
    } else {
        expiry = g_date_time_add_seconds (now, 60 - g_date_time_get_second (now));
    }

	if (self->priv->clock_update_id) {
		g_source_remove (self->priv->clock_update_id);
		self->priv->clock_update_id = 0;
	}

	source = _gnome_datetime_source_new (now, expiry, TRUE);
	g_source_set_priority (source, G_PRIORITY_HIGH);
	g_source_set_callback (source, update_clock, self, NULL);
	self->priv->clock_update_id = g_source_attach (source, NULL);
	g_source_unref (source);

    /* Update the clock and notify */

    g_free (self->priv->clock_string);

    self->priv->clock_string = g_date_time_format (now, self->priv->format_string);

    g_date_time_unref (now);
    g_date_time_unref (expiry);

    g_debug ("Sending clock notify: '%s'", self->priv->clock_string);

    g_object_notify ((GObject *) self, "clock");

    return FALSE;
}

static void
on_schema_change (GSettings *schema,
                  const char *key,
                  gpointer user_data)
{
    GnomeWallClock *self = GNOME_WALL_CLOCK (user_data);

    g_debug ("Updating clock because schema changed");

    update_format_string (self, self->priv->custom_format ? self->priv->format_string : NULL);
    update_clock (self);
}

static void
on_tz_changed (GFileMonitor      *monitor,
               GFile             *file,
               GFile             *other_file,
               GFileMonitorEvent *event,
               gpointer           user_data)
{
    GnomeWallClock *self = GNOME_WALL_CLOCK (user_data);

    g_debug ("Updating clock because timezone changed");

    update_format_string (self, self->priv->custom_format ? self->priv->format_string : NULL);
    update_clock (self);
}

/**
 * gnome_wall_clock_get_clock:
 * @clock: The GnomeWallClock
 *
 * Returns a formatted date and time based on either default format
 * settings, or via a custom-set format string.
 *
 * The returned string should be ready to be set on a label.
 *
 * Returns: (transfer none): The formatted date/time string.
 **/

const char *
gnome_wall_clock_get_clock (GnomeWallClock *clock)
{
	return clock->priv->clock_string;
}

/**
 * gnome_wall_clock_get_default_time_format:
 * @clock: The GnomeWallClock
 *
 * Returns the current time-only format based on current locale
 * defaults and clock settings.
 *
 * Returns: (transfer none): The default time format string.
 **/
const gchar *
gnome_wall_clock_get_default_time_format (GnomeWallClock *clock)
{
    return clock->priv->default_time_format;
}

/**
 * gnome_wall_clock_get_default_date_format:
 * @clock: The GnomeWallClock
 *
 * Returns the current date-only format based on current locale
 * defaults and clock settings.
 *
 * Returns: (transfer none): The default date format string.
 **/
const gchar *
gnome_wall_clock_get_default_date_format (GnomeWallClock *clock)
{
    return clock->priv->default_date_format;
}

/**
 * gnome_wall_clock_get_clock_for_format:
 * @clock: The GnomeWallClock
 * @format_string: (not nullable)
 *
 * Returns a formatted date and time based on the provided format string.
 * The returned string should be ready to be set on a label.
 *
 * Returns: (transfer full): The formatted date/time string, or NULL
 * if there was a problem with the format string.
 **/
gchar *
gnome_wall_clock_get_clock_for_format (GnomeWallClock *clock,
                                       const gchar    *format_string)
{
    gchar *ret;
    GDateTime *now;

    g_return_val_if_fail (format_string != NULL, NULL);

    now = g_date_time_new_now_local ();
    ret = g_date_time_format (now, format_string);

    g_date_time_unref (now);

    return ret;
}

/**
 * gnome_wall_clock_new:
 *
 * Returns a new GnomeWallClock instance
 *
 * Returns: A pointer to a new GnomeWallClock instance.
 **/
GnomeWallClock *
gnome_wall_clock_new (void)
{
    return g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
}

/**
 * gnome_wall_clock_set_format_string:
 * @clock: The GnomeWallClock
 * @format_string: (nullable)
 *
 * Sets the wall clock to use the provided format string for any
 * subsequent updates.  Passing NULL will un-set any custom format,
 * and rely on a default locale format.
 *
 * Any invalid format string passed will cause it to be ignored,
 * and the default locale format used instead.
 *
 * Returns: Whether or not the format string was valid and accepted.
 **/
gboolean
gnome_wall_clock_set_format_string (GnomeWallClock *clock,
                                    const gchar    *format_string)
{
    gboolean ret = FALSE;
    update_format_string (clock, format_string);

    if (format_string != NULL) {
        clock->priv->custom_format = g_strcmp0 (format_string, clock->priv->format_string) == 0;
        ret = clock->priv->custom_format;
    } else {
        clock->priv->custom_format = FALSE;
        ret = TRUE;
    }

    update_clock (clock);

    return ret;
}
