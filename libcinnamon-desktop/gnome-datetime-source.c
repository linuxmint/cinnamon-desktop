/* -*- mode: C; c-file-style: "linux"; indent-tabs-mode: t -*-
 * gdatetime-source.c - copy&paste from https://bugzilla.gnome.org/show_bug.cgi?id=655129
 *
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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-datetime-source.h"

#ifdef HAVE_TIMERFD
#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#endif

typedef struct _GDateTimeSource GDateTimeSource;
struct _GDateTimeSource
{
	GSource     source;
  
	gint64      real_expiration;
	gint64      wakeup_expiration;

	gboolean    cancel_on_set : 1;
	gboolean    initially_expired : 1;

	GPollFD     pollfd;
};

static inline void
g_datetime_source_reschedule (GDateTimeSource *datetime_source,
			      gint64                from_monotonic)
{
	datetime_source->wakeup_expiration = from_monotonic + G_TIME_SPAN_SECOND;
}

static gboolean
g_datetime_source_is_expired (GDateTimeSource *datetime_source)
{
	gint64 real_now;
	gint64 monotonic_now;

	real_now = g_get_real_time ();
	monotonic_now = g_source_get_time ((GSource*)datetime_source);

	if (datetime_source->initially_expired)
		return TRUE;
  
	if (datetime_source->real_expiration <= real_now)
		return TRUE;

	/* We can't really detect without system support when things
	 * change; so just trigger every second (i.e. our wakeup
	 * expiration)
	 */
	if (datetime_source->cancel_on_set && monotonic_now >= datetime_source->wakeup_expiration)
		return TRUE;

	return FALSE;
}

/* In prepare, we're just checking the monotonic time against
 * our projected wakeup.
 */
static gboolean
g_datetime_source_prepare (GSource *source,
			   gint    *timeout)
{
	GDateTimeSource *datetime_source = (GDateTimeSource*)source;
	gint64 monotonic_now;

#ifdef HAVE_TIMERFD
	if (datetime_source->pollfd.fd != -1) {
		*timeout = -1;
		return datetime_source->initially_expired;  /* Should be TRUE at most one time, FALSE forever after */
	}
#endif

	monotonic_now = g_source_get_time (source);

	if (monotonic_now < datetime_source->wakeup_expiration) {
		/* Round up to ensure that we don't try again too early */
		*timeout = (datetime_source->wakeup_expiration - monotonic_now + 999) / 1000;
		return FALSE;
	}

	*timeout = 0;
	return g_datetime_source_is_expired (datetime_source);
}

/* In check, we're looking at the wall clock.
 */
static gboolean 
g_datetime_source_check (GSource  *source)
{
	GDateTimeSource *datetime_source = (GDateTimeSource*)source;

#ifdef HAVE_TIMERFD
	if (datetime_source->pollfd.fd != -1)
		return datetime_source->pollfd.revents != 0;
#endif

	if (g_datetime_source_is_expired (datetime_source))
		return TRUE;

	g_datetime_source_reschedule (datetime_source, g_source_get_time (source));
  
	return FALSE;
}

static gboolean
g_datetime_source_dispatch (GSource    *source, 
			    GSourceFunc callback,
			    gpointer    user_data)
{
	GDateTimeSource *datetime_source = (GDateTimeSource*)source;

	datetime_source->initially_expired = FALSE;

	if (!callback) {
		g_warning ("Timeout source dispatched without callback\n"
			   "You must call g_source_set_callback().");
		return FALSE;
	}
  
	(callback) (user_data);

	/* Always false as this source is documented to run once */
	return FALSE;
}

static void
g_datetime_source_finalize (GSource *source)
{
#ifdef HAVE_TIMERFD
	GDateTimeSource *datetime_source = (GDateTimeSource*)source;
	if (datetime_source->pollfd.fd != -1)
		close (datetime_source->pollfd.fd);
#endif
}

static GSourceFuncs g_datetime_source_funcs = {
	g_datetime_source_prepare,
	g_datetime_source_check,
	g_datetime_source_dispatch,
	g_datetime_source_finalize
};

#ifdef HAVE_TIMERFD
static gboolean
g_datetime_source_init_timerfd (GDateTimeSource *datetime_source,
				gint64           expected_now_seconds,
				gint64           unix_seconds)
{
	struct itimerspec its;
	int settime_flags;

	datetime_source->pollfd.fd = timerfd_create (CLOCK_REALTIME, TFD_CLOEXEC);
	if (datetime_source->pollfd.fd == -1)
		return FALSE;

	memset (&its, 0, sizeof (its));
	its.it_value.tv_sec = (time_t) unix_seconds;

	/* http://article.gmane.org/gmane.linux.kernel/1132138 */
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

	settime_flags = TFD_TIMER_ABSTIME;
	if (datetime_source->cancel_on_set)
		settime_flags |= TFD_TIMER_CANCEL_ON_SET;

	if (timerfd_settime (datetime_source->pollfd.fd, settime_flags, &its, NULL) < 0) {
		close (datetime_source->pollfd.fd);
		datetime_source->pollfd.fd = -1;
		return FALSE;
	}

	/* Now we need to check that the clock didn't go backwards before we
	 * had the timerfd set up.  See
	 * https://bugzilla.gnome.org/show_bug.cgi?id=655129
	 */
	clock_gettime (CLOCK_REALTIME, &its.it_value);
	if (its.it_value.tv_sec < expected_now_seconds)
		datetime_source->initially_expired = TRUE;

	datetime_source->pollfd.events = G_IO_IN;

	g_source_add_poll ((GSource*) datetime_source, &datetime_source->pollfd);

	return TRUE;
}
#endif

/**
 * _gnome_date_time_source_new:
 * @now: The expected current time
 * @expiry: Time to await
 * @cancel_on_set: Also invoke callback if the system clock changes discontiguously
 *
 * This function is designed for programs that want to schedule an
 * event based on real (wall clock) time, as returned by
 * g_get_real_time().  For example, HOUR:MINUTE wall-clock displays
 * and calendaring software.  The callback will be invoked when the
 * specified wall clock time @expiry is reached.  This includes
 * events such as the system clock being set past the given time.
 *
 * Compare versus g_timeout_source_new() which is defined to use
 * monotonic time as returned by g_get_monotonic_time().
 *
 * The parameter @now is necessary to avoid a race condition in
 * between getting the current time and calling this function.
 *
 * If @cancel_on_set is given, the callback will also be invoked at
 * most a second after the system clock is changed.  This includes
 * being set backwards or forwards, and system
 * resume from suspend.  Not all operating systems allow detecting all
 * relevant events efficiently - this function may cause the process
 * to wake up once a second in those cases.
 *
 * A wall clock display should use @cancel_on_set; a calendaring
 * program shouldn't need to.
 *
 * Note that the return value from the associated callback will be
 * ignored; this is a one time watch.
 *
 * <note><para>This function currently does not detect time zone
 * changes.  On Linux, your program should also monitor the
 * <literal>/etc/timezone</literal> file using
 * #GFileMonitor.</para></note>
 *
 * <example id="gdatetime-example-watch"><title>Clock example</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../glib/tests/glib-clock.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 *
 * Return value: A newly-constructed #GSource
 *
 * Since: 2.30
 **/
GSource *
_gnome_datetime_source_new (GDateTime  *now,
			    GDateTime  *expiry,
			    gboolean    cancel_on_set)
{
	GDateTimeSource *datetime_source;
	gint64 unix_seconds;

	unix_seconds = g_date_time_to_unix (expiry);

	datetime_source = (GDateTimeSource*) g_source_new (&g_datetime_source_funcs, sizeof (GDateTimeSource));

	datetime_source->cancel_on_set = cancel_on_set;

#ifdef HAVE_TIMERFD
	{
		gint64 expected_now_seconds = g_date_time_to_unix (now);
		if (g_datetime_source_init_timerfd (datetime_source, expected_now_seconds, unix_seconds))
			return (GSource*)datetime_source;
		/* Fall through to non-timerfd code */
	}
#endif

	datetime_source->real_expiration = unix_seconds * 1000000;
	g_datetime_source_reschedule (datetime_source, g_get_monotonic_time ());

	return (GSource*)datetime_source;
}

