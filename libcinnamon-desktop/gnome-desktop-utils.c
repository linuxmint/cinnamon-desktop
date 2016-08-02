/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-desktop-utils.c - Utilities for the GNOME Desktop

   Copyright (C) 1998 Tom Tromey
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */
/*
  @NOTATION@
 */

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-desktop-utils.h"
#include "cdesktop-enums.h"

#include "private.h"

/**
 * gnome_desktop_prepend_terminal_to_vector:
 * @argc: a pointer to the vector size
 * @argv: a pointer to the vector
 *
 * Description:  Prepends a terminal (either the one configured as default in
 * the user's GNOME setup, or one of the common xterm emulators) to the passed
 * in vector, modifying it in the process.  The vector should be allocated with
 * #g_malloc, as this will #g_free the original vector.  Also all elements must
 * have been allocated separately.  That is the standard glib/GNOME way of
 * doing vectors however.  If the integer that @argc points to is negative, the
 * size will first be computed.  Also note that passing in pointers to a vector
 * that is empty, will just create a new vector for you.
 **/
void
gnome_desktop_prepend_terminal_to_vector (int *argc, char ***argv)
{
#ifndef G_OS_WIN32
        char **real_argv;
        int real_argc;
        int i, j;
	char **term_argv = NULL;
	int term_argc = 0;
	GSettings *settings;

	gchar *terminal = NULL;

	char **the_argv;

        g_return_if_fail (argc != NULL);
        g_return_if_fail (argv != NULL);

        _gnome_desktop_init_i18n ();

	/* sanity */
        if(*argv == NULL)
                *argc = 0;

	the_argv = *argv;

	/* compute size if not given */
	if (*argc < 0) {
		for (i = 0; the_argv[i] != NULL; i++)
			;
		*argc = i;
	}

	settings = g_settings_new ("org.cinnamon.desktop.default-applications.terminal");
	terminal = g_settings_get_string (settings, "exec");

	if (terminal) {
		gchar *command_line;
		gchar *exec_flag;

		exec_flag = g_settings_get_string (settings, "exec-arg");

		if (exec_flag == NULL)
			command_line = g_strdup (terminal);
		else
			command_line = g_strdup_printf ("%s %s", terminal,
							exec_flag);

		g_shell_parse_argv (command_line,
				    &term_argc,
				    &term_argv,
				    NULL /* error */);

		g_free (command_line);
		g_free (exec_flag);
		g_free (terminal);
	}

	g_object_unref (settings);

	if (term_argv == NULL) {
		char *check;

		term_argc = 2;
		term_argv = g_new0 (char *, 3);

		check = g_find_program_in_path ("gnome-terminal");
		if (check != NULL) {
			term_argv[0] = check;
			/* Note that gnome-terminal takes -x and
			 * as -e in gnome-terminal is broken we use that. */
			term_argv[1] = g_strdup ("-x");
		} else {
			if (check == NULL)
				check = g_find_program_in_path ("nxterm");
			if (check == NULL)
				check = g_find_program_in_path ("color-xterm");
			if (check == NULL)
				check = g_find_program_in_path ("rxvt");
			if (check == NULL)
				check = g_find_program_in_path ("xterm");
			if (check == NULL)
				check = g_find_program_in_path ("dtterm");
			if (check == NULL) {
				g_warning (_("Cannot find a terminal, using "
					     "xterm, even if it may not work"));
				check = g_strdup ("xterm");
			}
			term_argv[0] = check;
			term_argv[1] = g_strdup ("-e");
		}
	}

        real_argc = term_argc + *argc;
        real_argv = g_new (char *, real_argc + 1);

        for (i = 0; i < term_argc; i++)
                real_argv[i] = term_argv[i];

        for (j = 0; j < *argc; j++, i++)
                real_argv[i] = (char *)the_argv[j];

	real_argv[i] = NULL;

	g_free (*argv);
	*argv = real_argv;
	*argc = real_argc;

	/* we use g_free here as we sucked all the inner strings
	 * out from it into real_argv */
	g_free (term_argv);
#else
	/* FIXME: Implement when needed */
	g_warning ("gnome_prepend_terminal_to_vector: Not implemented");
#endif
}

void
_gnome_desktop_init_i18n (void) {
	static gboolean initialized = FALSE;
	
	if (!initialized) {
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
		initialized = TRUE;
	}
}

/**
 * gnome_desktop_get_media_key_string:
 * @type: The CDesktopMediaKeyType
 *
 * Description:  Returns the GSettings key string of the
 * given media key type.
 *
 * Returns: (transfer none): the string corresponding to the
 * provided media key type or %NULL
 **/
const gchar *
gnome_desktop_get_media_key_string (gint type)
{
    g_return_val_if_fail (type >= 0 && type < G_N_ELEMENTS (media_keys), NULL);

    return media_keys[type];
}

/**
 * gnome_desktop_get_session_user_pwent: (skip)
 *
 * Description: Makes a best effort to retrieve the currently
 * logged-in user's passwd struct (containing uid, gid, home, etc...)
 * based on the process uid and various environment variables.
 *
 * Returns: (transfer none): the passwd struct corresponding to the
 * session user (or, as a last resort, the user returned by getuid())
 **/

struct passwd *
gnome_desktop_get_session_user_pwent (void)
{
    struct passwd *pwent = NULL;

    if (getuid () != geteuid ()) {
        gint uid = getuid ();
        pwent = getpwuid (uid);
    } else if (g_getenv ("SUDO_UID") != NULL) {
        gint uid = (int) g_ascii_strtoll (g_getenv ("SUDO_UID"), NULL, 10);
        pwent = getpwuid (uid);
    } else if (g_getenv ("PKEXEC_UID") != NULL) {
        gint uid = (int) g_ascii_strtoll (g_getenv ("PKEXEC_UID"), NULL, 10);
        pwent = getpwuid (uid);
    } else if (g_getenv ("USERNAME") != NULL) {
        pwent = getpwnam (g_getenv ("USERNAME"));
    } else if (g_getenv ("USER") != NULL) {
        pwent = getpwnam (g_getenv ("USER"));
    }

    if (!pwent) {
        return getpwuid (getuid ());
    }

    return pwent;
}

#define _pam_overwrite(x)        \
do {                             \
     register char *__xx__;      \
     if ((__xx__=(x)))           \
          while (*__xx__)        \
               *__xx__++ = '\0'; \
} while (0)

typedef struct
{
    GnomeCheckPasswordCallback callback;
    gpointer user_data;
} CheckPasswordCallbackData;

typedef struct
{
    gchar *username;
    gchar *password;
} CheckPasswordStrings;

struct pam_closure {
        const char       *username;
        const char       *password;
};

static void
free_strings (CheckPasswordStrings *strings)
{
    g_clear_pointer (&strings->username, g_free);
    g_clear_pointer (&strings->password, g_free);

    g_slice_free (CheckPasswordStrings, strings);
}

static int
yet_another_pam_conv(int                        num_msg,
                     const struct pam_message **msgm,
                     struct pam_response      **response,
                     void                      *appdata_ptr)
{
    int count = 0;
    struct pam_response *reply = NULL;
    struct pam_closure  *c = (struct pam_closure *) appdata_ptr;

    if (num_msg <= 0)
        return PAM_CONV_ERR;

    reply = (struct pam_response *) calloc(num_msg, sizeof(struct pam_response));

    if (reply == NULL) {
        return PAM_CONV_ERR;
    }

    for (count = 0; count < num_msg; ++count) {
        char *string = NULL;

        switch (msgm[count]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
                string = g_strdup (c->password);
                break;
            case PAM_PROMPT_ECHO_ON:
                string = g_strdup (c->username);
                break;
            case PAM_ERROR_MSG:
                goto failed_conversation;
                break;
            case PAM_TEXT_INFO:
                goto failed_conversation;
                break;
            case PAM_BINARY_PROMPT:
                goto failed_conversation;
                break;
            default:
                goto failed_conversation;
                break;
        }

        if (string) {
            /* add string to list of responses */
            reply[count].resp_retcode = 0;
            reply[count].resp = string;
            string = NULL;
        }
    }

    *response = reply;

    reply = NULL;

    return PAM_SUCCESS;

failed_conversation:

    if (reply) {
        for (count = 0; count < num_msg; ++count) {
            if (reply[count].resp == NULL) {
                continue;
            }

            switch (msgm[count]->msg_style) {
                case PAM_PROMPT_ECHO_ON:
                case PAM_PROMPT_ECHO_OFF:
                    _pam_overwrite(reply[count].resp);
                    free(reply[count].resp);
                    break;
                case PAM_BINARY_PROMPT:
                case PAM_ERROR_MSG:
                case PAM_TEXT_INFO:
                    break;
            }

            reply[count].resp = NULL;
        }

    /* forget reply too */
    free(reply);
    reply = NULL;
    }

    return PAM_CONV_ERR;
}

static void
check_password_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
    CheckPasswordStrings *strings = (CheckPasswordStrings *) task_data;

    pam_handle_t *pamh = NULL;
    gint retval;
    gboolean ret = FALSE;
    struct pam_closure c;
    struct pam_conv conv;

    c.username = strings->username;
    c.password = strings->password;

    conv.conv = &yet_another_pam_conv;
    conv.appdata_ptr = (void *) &c;

    retval = pam_start ("cinnamon-desktop", strings->username, &conv, &pamh);

    if (retval == PAM_SUCCESS)
        retval = pam_authenticate (pamh, 0);

    if (retval == PAM_SUCCESS)
        retval = pam_acct_mgmt (pamh, 0);

    ret = (retval == PAM_SUCCESS);

    if (pam_end (pamh,retval) != PAM_SUCCESS) {
        pamh = NULL;
        g_printerr ("cinnamon-desktop: failed to release authenticator\n");
    }

    g_task_return_boolean (task, ret);
}

static void
check_password_task_finished (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data)
{
    CheckPasswordCallbackData *data = (CheckPasswordCallbackData *) user_data;

    gboolean success = g_task_propagate_boolean (G_TASK (result), NULL);

    if (data->callback)
        (* data->callback) (success, data->user_data);
}

/**
 * gnome_desktop_check_user_password:
 * @username: The username to check
 * @password: The password to check
 * @callback: (scope async): The callback to call when finished
 * @user_data: (closure): data to pass with the callback
 *
 * Checks a username against a password and sends the result to the
 * callback function.
 **/
void gnome_desktop_check_user_password (const gchar *username,
                                        const gchar *password,
                                        GnomeCheckPasswordCallback callback,
                                        gpointer user_data)
{
    CheckPasswordCallbackData *data = g_slice_new (CheckPasswordCallbackData);

    data->callback = callback;
    data->user_data = user_data;

    CheckPasswordStrings *strings = g_slice_new0 (CheckPasswordStrings);

    strings->username = g_strdup (username);
    strings->password = g_strdup (password);

    GTask *task;

    task = g_task_new (NULL,
                       NULL,
                       check_password_task_finished,
                       data);

    g_task_set_task_data (task, strings, (GDestroyNotify) free_strings);
    g_task_run_in_thread (task, check_password_thread);

    g_object_unref (task);
}

