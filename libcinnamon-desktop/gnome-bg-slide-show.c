/* gnome-bg-slide-show.h
 *
 * Copyright (C) 2008, 2013 Red Hat, Inc.
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
 */

#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <gio/gio.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-bg-slide-show.h"

struct _GnomeBGSlideShowPrivate
{
        char *filename;

        double start_time;
        double total_duration;

        GQueue *slides;

        gboolean has_multiple_sizes;

        /* used during parsing */
        struct tm start_tm;
        GQueue *stack;
};

typedef struct _Slide Slide;

struct _Slide
{
       double   duration;              /* in seconds */
       gboolean fixed;

       GSList  *file1;
       GSList  *file2;         /* NULL if fixed is TRUE */
};

typedef struct _FileSize FileSize;
struct _FileSize
{
       gint width;
       gint height;

       char *file;
};

enum {
        PROP_0,
        PROP_FILENAME,
        PROP_START_TIME,
        PROP_TOTAL_DURATION,
        PROP_HAS_MULTIPLE_SIZES,
};

G_DEFINE_TYPE (GnomeBGSlideShow, gnome_bg_slide_show, G_TYPE_OBJECT)
#define GNOME_BG_SLIDE_SHOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o),\
                                            GNOME_BG_TYPE_SLIDE_SHOW,\
                                            GnomeBGSlideShowPrivate))

static void
gnome_bg_slide_show_set_property (GObject       *object,
                                  guint          property_id,
                                  const GValue  *value,
                                  GParamSpec    *pspec)
{
        GnomeBGSlideShow *self;

        g_assert (GNOME_BG_IS_SLIDE_SHOW (object));

        self = GNOME_BG_SLIDE_SHOW (object);

        switch (property_id)
        {
        case PROP_FILENAME:
                self->priv->filename = g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gnome_bg_slide_show_get_property (GObject     *object,
                                  guint        property_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
        GnomeBGSlideShow *self;

        g_assert (GNOME_BG_IS_SLIDE_SHOW (object));

        self = GNOME_BG_SLIDE_SHOW (object);

        switch (property_id)
        {
        case PROP_START_TIME:
                g_value_set_int (value, self->priv->start_time);
                break;
        case PROP_TOTAL_DURATION:
                g_value_set_int (value, self->priv->total_duration);
                break;
        case PROP_HAS_MULTIPLE_SIZES:
                g_value_set_boolean (value, self->priv->has_multiple_sizes);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gnome_bg_slide_show_finalize (GObject *object)
{
        GnomeBGSlideShow *self;

        GList *list;
        GSList *slist;
        FileSize *size;

        self = GNOME_BG_SLIDE_SHOW (object);

        for (list = self->priv->slides->head; list != NULL; list = list->next) {
                Slide *slide = list->data;

                for (slist = slide->file1; slist != NULL; slist = slist->next) {
                        size = slist->data;
                        g_free (size->file);
                        g_free (size);
                }
                g_slist_free (slide->file1);

                for (slist = slide->file2; slist != NULL; slist = slist->next) {
                        size = slist->data;
                        g_free (size->file);
                        g_free (size);
                }
                g_slist_free (slide->file2);

                g_free (slide);
        }

        g_queue_free (self->priv->slides);

        g_list_foreach (self->priv->stack->head, (GFunc) g_free, NULL);
        g_queue_free (self->priv->stack);

        g_free (self->priv->filename);
}

static void
gnome_bg_slide_show_class_init (GnomeBGSlideShowClass *self_class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (self_class);

        gobject_class->get_property = gnome_bg_slide_show_get_property;
        gobject_class->set_property = gnome_bg_slide_show_set_property;
        gobject_class->finalize = gnome_bg_slide_show_finalize;

        g_object_class_install_property (gobject_class,
                                         PROP_FILENAME,
                                         g_param_spec_string ("filename",
                                                              "Filename",
                                                              "Filename",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (gobject_class,
                                         PROP_START_TIME,
                                         g_param_spec_double ("start-time",
                                                              "Start time",
                                                              "start time",
                                                              0.0, G_MAXDOUBLE, 0.0,
                                                              G_PARAM_READABLE));

        g_object_class_install_property (gobject_class,
                                         PROP_TOTAL_DURATION,
                                         g_param_spec_double ("total-duration",
                                                              "Start duration",
                                                              "total duration",
                                                              0.0, G_MAXDOUBLE, 0.0,
                                                              G_PARAM_READABLE));

        g_object_class_install_property (gobject_class,
                                         PROP_HAS_MULTIPLE_SIZES,
                                         g_param_spec_boolean ("has-multiple-sizes",
                                                               "Has multiple sizes",
                                                               "Has multiple sizes",
                                                               FALSE,
                                                               G_PARAM_READABLE));

        g_type_class_add_private (gobject_class, sizeof (GnomeBGSlideShowPrivate));
}

static void
gnome_bg_slide_show_init (GnomeBGSlideShow *self)
{
        self->priv = GNOME_BG_SLIDE_SHOW_GET_PRIVATE (self);

        self->priv->stack = g_queue_new ();
        self->priv->slides = g_queue_new ();
}

/**
 * gnome_bg_slide_show_new:
 * @filename: The name of the slide show file
 *
 * Creates a new object to manage a slide show.
 * window background between two #cairo_surface_ts.
 *
 * Return value: the new #GnomeBGSlideShow
 **/
GnomeBGSlideShow *
gnome_bg_slide_show_new (const char *filename)
{
        return GNOME_BG_SLIDE_SHOW (g_object_new (GNOME_BG_TYPE_SLIDE_SHOW,
                                                  "filename", filename,
                                                  NULL));
}

static void
threadsafe_localtime (time_t time, struct tm *tm)
{
    struct tm *res;

    G_LOCK_DEFINE_STATIC (localtime_mutex);

    G_LOCK (localtime_mutex);

    res = localtime (&time);
    if (tm) {
        *tm = *res;
    }

    G_UNLOCK (localtime_mutex);
}
static gboolean stack_is (GnomeBGSlideShow *self, const char *s1, ...);

/* Parser for fading background */
static void
handle_start_element (GMarkupParseContext *context,
                      const gchar         *name,
                      const gchar        **attr_names,
                      const gchar        **attr_values,
                      gpointer             user_data,
                      GError             **err)
{
        GnomeBGSlideShow *self = user_data;
        gint i;

        if (strcmp (name, "static") == 0 || strcmp (name, "transition") == 0) {
                Slide *slide = g_new0 (Slide, 1);

                if (strcmp (name, "static") == 0)
                        slide->fixed = TRUE;

                g_queue_push_tail (self->priv->slides, slide);
        }
        else if (strcmp (name, "size") == 0) {
                Slide *slide = self->priv->slides->tail->data;
                FileSize *size = g_new0 (FileSize, 1);
                for (i = 0; attr_names[i]; i++) {
                        if (strcmp (attr_names[i], "width") == 0)
                                size->width = atoi (attr_values[i]);
                        else if (strcmp (attr_names[i], "height") == 0)
                                size->height = atoi (attr_values[i]);
                }
                if (self->priv->stack->tail &&
                    (strcmp (self->priv->stack->tail->data, "file") == 0 ||
                     strcmp (self->priv->stack->tail->data, "from") == 0)) {
                        slide->file1 = g_slist_prepend (slide->file1, size);
                }
                else if (self->priv->stack->tail &&
                         strcmp (self->priv->stack->tail->data, "to") == 0) {
                        slide->file2 = g_slist_prepend (slide->file2, size);
                }
        }
        g_queue_push_tail (self->priv->stack, g_strdup (name));
}

static void
handle_end_element (GMarkupParseContext *context,
                    const gchar         *name,
                    gpointer             user_data,
                    GError             **err)
{
        GnomeBGSlideShow *self = user_data;

        g_free (g_queue_pop_tail (self->priv->stack));
}

static gboolean
stack_is (GnomeBGSlideShow *self,
          const char *s1,
          ...)
{
        GList *stack = NULL;
        const char *s;
        GList *l1, *l2;
        va_list args;

        stack = g_list_prepend (stack, (gpointer)s1);

        va_start (args, s1);

        s = va_arg (args, const char *);
        while (s) {
                stack = g_list_prepend (stack, (gpointer)s);
                s = va_arg (args, const char *);
        }

        l1 = stack;
        l2 = self->priv->stack->head;

        while (l1 && l2) {
                if (strcmp (l1->data, l2->data) != 0) {
                        g_list_free (stack);
                        return FALSE;
                }

                l1 = l1->next;
                l2 = l2->next;
        }

        g_list_free (stack);

        return (!l1 && !l2);
}

static int
parse_int (const char *text)
{
        return strtol (text, NULL, 0);
}

static void
handle_text (GMarkupParseContext *context,
             const gchar         *text,
             gsize                text_len,
             gpointer             user_data,
             GError             **err)
{
        GnomeBGSlideShow *self = user_data;
        Slide *slide = self->priv->slides->tail? self->priv->slides->tail->data : NULL;
        FileSize *fs;
        gint i;

        if (stack_is (self, "year", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_year = parse_int (text) - 1900;
        }
        else if (stack_is (self, "month", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_mon = parse_int (text) - 1;
        }
        else if (stack_is (self, "day", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_mday = parse_int (text);
        }
        else if (stack_is (self, "hour", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_hour = parse_int (text) - 1;
        }
        else if (stack_is (self, "minute", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_min = parse_int (text);
        }
        else if (stack_is (self, "second", "starttime", "background", NULL)) {
                self->priv->start_tm.tm_sec = parse_int (text);
        }
        else if (stack_is (self, "duration", "static", "background", NULL) ||
                 stack_is (self, "duration", "transition", "background", NULL)) {
                slide->duration = g_strtod (text, NULL);
                self->priv->total_duration += slide->duration;
        }
        else if (stack_is (self, "file", "static", "background", NULL) ||
                 stack_is (self, "from", "transition", "background", NULL)) {
                for (i = 0; text[i]; i++) {
                        if (!g_ascii_isspace (text[i]))
                                break;
                }
                if (text[i] == 0)
                        return;
                fs = g_new (FileSize, 1);
                fs->width = -1;
                fs->height = -1;
                fs->file = g_strdup (text);
                slide->file1 = g_slist_prepend (slide->file1, fs);
                if (slide->file1->next != NULL)
                        self->priv->has_multiple_sizes = TRUE;
        }
        else if (stack_is (self, "size", "file", "static", "background", NULL) ||
                 stack_is (self, "size", "from", "transition", "background", NULL)) {
                fs = slide->file1->data;
                fs->file = g_strdup (text);
                if (slide->file1->next != NULL)
                        self->priv->has_multiple_sizes = TRUE;
        }
        else if (stack_is (self, "to", "transition", "background", NULL)) {
                for (i = 0; text[i]; i++) {
                        if (!g_ascii_isspace (text[i]))
                                break;
                }
                if (text[i] == 0)
                        return;
                fs = g_new (FileSize, 1);
                fs->width = -1;
                fs->height = -1;
                fs->file = g_strdup (text);
                slide->file2 = g_slist_prepend (slide->file2, fs);
                if (slide->file2->next != NULL)
                        self->priv->has_multiple_sizes = TRUE;
        }
        else if (stack_is (self, "size", "to", "transition", "background", NULL)) {
                fs = slide->file2->data;
                fs->file = g_strdup (text);
                if (slide->file2->next != NULL)
                        self->priv->has_multiple_sizes = TRUE;
        }
}

/*
 * Find the FileSize that best matches the given size.
 * Do two passes; the first pass only considers FileSizes
 * that are larger than the given size.
 * We are looking for the image that best matches the aspect ratio.
 * When two images have the same aspect ratio, prefer the one whose
 * width is closer to the given width.
 */
static const char *
find_best_size (GSList *sizes, gint width, gint height)
{
    GSList *s;
    gdouble a, d, distance;
    FileSize *best = NULL;
    gint pass;

    a = width/(gdouble)height;
    distance = 10000.0;

    for (pass = 0; pass < 2; pass++) {
        for (s = sizes; s; s = s->next) {
            FileSize *size = s->data;

            if (pass == 0 && (size->width < width || size->height < height))
                continue;

            d = fabs (a - size->width/(gdouble)size->height);
            if (d < distance) {
                distance = d;
                best = size;
                        }
            else if (d == distance) {
                if (abs (size->width - width) < abs (best->width - width)) {
                    best = size;
                }
            }
        }

        if (best)
            break;
    }

    return best->file;
}

static double
now (void)
{
    GTimeVal tv;

    g_get_current_time (&tv);

    return (double)tv.tv_sec + (tv.tv_usec / 1000000.0);
}

/**
 * gnome_bg_slide_show_get_current_slide:
 * @self: a #GnomeBGSlideShow
 * @width: monitor width
 * @height: monitor height
 * @progress: (out) (allow-none): slide progress
 * @duration: (out) (allow-none): slide duration
 * @is_fixed: (out) (allow-none): if slide is fixed
 * @file1: (out) (allow-none) (transfer none): first file in slide
 * @file2: (out) (allow-none) (transfer none): second file in slide
 *
 * Returns the current slides progress
 *
 * Return value: %TRUE if successful
 **/
void
gnome_bg_slide_show_get_current_slide (GnomeBGSlideShow  *self,
                                       int                width,
                                       int                height,
                                       gdouble           *progress,
                                       double            *duration,
                                       gboolean          *is_fixed,
                                       const char       **file1,
                                       const char       **file2)
{
    double delta = fmod (now() - self->priv->start_time, self->priv->total_duration);
    GList *list;
    double elapsed;
    int i;

    if (delta < 0)
        delta += self->priv->total_duration;

    elapsed = 0;
    i = 0;
    for (list = self->priv->slides->head; list != NULL; list = list->next) {
        Slide *slide = list->data;

        if (elapsed + slide->duration > delta) {
                        if (progress)
                            *progress = (delta - elapsed) / (double)slide->duration;
                        if (duration)
                            *duration = slide->duration;

                        if (is_fixed)
                            *is_fixed = slide->fixed;

                        if (file1)
                            *file1 = find_best_size (slide->file1, width, height);

                        if (file2 && slide->file2)
                            *file2 = find_best_size (slide->file2, width, height);

                        return;
        }

        i++;
        elapsed += slide->duration;
    }

    /* this should never happen since we have slides and we should always
     * find a current slide for the elapsed time since beginning -- we're
     * looping with fmod() */
    g_assert_not_reached ();
}

/**
 * gnome_bg_slide_show_get_slide:
 * @self: a #GnomeBGSlideShow
 * @frame_number: frame number
 * @width: monitor width
 * @height: monitor height
 * @duration: (out) (allow-none): slide duration
 * @is_fixed: (out) (allow-none): if slide is fixed
 * @file1: (out) (allow-none) (transfer none): first file in slide
 * @file2: (out) (allow-none) (transfer none): second file in slide
 *
 * Retrieves slide by frame number
 *
 * Return value: %TRUE if successful
 **/
gboolean
gnome_bg_slide_show_get_slide (GnomeBGSlideShow  *self,
                               int                frame_number,
                               int                width,
                               int                height,
                               double            *progress,
                               double            *duration,
                               gboolean          *is_fixed,
                               const char       **file1,
                               const char       **file2)
{
    double delta = fmod (now() - self->priv->start_time, self->priv->total_duration);
        GList *l;
        int i, skipped;
        gboolean found;
        double elapsed;
        Slide *slide;

    if (delta < 0)
        delta += self->priv->total_duration;

        elapsed = 0;
    i = 0;
    skipped = 0;
    found = FALSE;
    for (l = self->priv->slides->head; l; l = l->next) {
        slide = l->data;

        if (!slide->fixed) {
                        elapsed += slide->duration;

            skipped++;
            continue;
        }
        if (i == frame_number) {
            found = TRUE;
            break;
        }
        i++;
                elapsed += slide->duration;
    }
    if (!found)
        return FALSE;

        if (progress) {
            if (elapsed + slide->duration > delta) {
                    *progress = (delta - elapsed) / (double)slide->duration;
            } else {
                    *progress = 0.0;
            }
        }

        if (duration)
                *duration = slide->duration;

        if (is_fixed)
                *is_fixed = slide->fixed;

        if (file1)
                *file1 = find_best_size (slide->file1, width, height);

        if (file2 && slide->file2)
                *file2 = find_best_size (slide->file2, width, height);

        return TRUE;
}

static gboolean
parse_file_contents (GnomeBGSlideShow  *self,
                     const char        *contents,
                     gsize              len,
                     GError           **error)
{
        GMarkupParser parser = {
                handle_start_element,
                handle_end_element,
                handle_text,
                NULL, /* passthrough */
                NULL, /* error */
        };

        GMarkupParseContext *context = NULL;
        time_t t;
        gboolean failed = FALSE;

        threadsafe_localtime ((time_t)0, &self->priv->start_tm);

        context = g_markup_parse_context_new (&parser, 0, self, NULL);

        if (!g_markup_parse_context_parse (context, contents, len, error)) {
                failed = TRUE;
        }

        if (!failed && !g_markup_parse_context_end_parse (context, error)) {
                failed = TRUE;
        }

        g_markup_parse_context_free (context);

        if (!failed) {
                int len;

                t = mktime (&self->priv->start_tm);

                self->priv->start_time = (double)t;

                len = g_queue_get_length (self->priv->slides);

                /* no slides, that's not a slideshow */
                if (len == 0) {
                        g_set_error_literal (error,
                                             G_MARKUP_ERROR,
                                             G_MARKUP_ERROR_INVALID_CONTENT,
                                             "file is not a slide show since it has no slides");
                        failed = TRUE;
                /* one slide, there's no transition */
                } else if (len == 1) {
                        Slide *slide = self->priv->slides->head->data;
                        slide->duration = self->priv->total_duration = G_MAXUINT;
                }
        }

        return !failed;
}

/**
 * gnome_bg_slide_show_load:
 * @self: a #GnomeBGSlideShow
 * @error: a #GError
 *
 * Tries to load the slide show.
 *
 * Return value: %TRUE if successful
 **/
gboolean
gnome_bg_slide_show_load (GnomeBGSlideShow  *self,
                          GError           **error)
{
        GFile *file;
        char  *contents;
        gsize  length;
        gboolean parsed;

        file = g_file_new_for_path (self->priv->filename);
        if (!g_file_load_contents (file, NULL, &contents, &length, NULL, NULL)) {
                return FALSE;
        }
        g_object_unref (file);

        parsed = parse_file_contents (self, contents, length, error);
        g_free (contents);

        return parsed;
}

static void
on_file_loaded (GFile        *file,
                GAsyncResult *result,
                GTask        *task)
{
    gboolean  loaded;
    char     *contents;
    gsize    length;
    GError   *error = NULL;

    loaded = g_file_load_contents_finish (file, result, &contents, &length, NULL, &error);

    if (!loaded) {
            g_task_return_error (task, error);
            return;
    }

    if (!parse_file_contents (g_task_get_source_object (task), contents, length, &error)) {
            g_task_return_error (task, error);
            g_free (contents);
            return;
    }
    g_free (contents);

    g_task_return_boolean (task, TRUE);
}

/**
 * gnome_bg_slide_show_load_async:
 * @self: a #GnomeBGSlideShow
 * @cancellable: a #GCancellable
 * @callback: the callback
 * @user_data: user data
 *
 * Tries to load the slide show asynchronously.
 **/
void
gnome_bg_slide_show_load_async (GnomeBGSlideShow    *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    GTask *task;
    GFile *file;

    task = g_task_new (self, cancellable, callback, user_data);

    file = g_file_new_for_path (self->priv->filename);
    g_file_load_contents_async (file, cancellable, (GAsyncReadyCallback) on_file_loaded, task);
    g_object_unref (file);
}

/**
 * gnome_bg_slide_show_get_start_time:
 * @self: a #GnomeBGSlideShow
 *
 * gets the start time of the slide show
 *
 * Return value: a timestamp
 **/
double
gnome_bg_slide_show_get_start_time (GnomeBGSlideShow *self)
{
        return self->priv->start_time;
}

/**
 * gnome_bg_slide_show_get_total_duration:
 * @self: a #GnomeBGSlideShow
 *
 * gets the total duration of the slide show
 *
 * Return value: a timestamp
 **/
double
gnome_bg_slide_show_get_total_duration (GnomeBGSlideShow *self)
{
        return self->priv->total_duration;
}

/**
 * gnome_bg_slide_show_get_has_multiple_sizes:
 * @self: a #GnomeBGSlideShow
 *
 * gets whether or not the slide show has multiple sizes for different monitors
 *
 * Return value: %TRUE if multiple sizes
 **/
gboolean
gnome_bg_slide_show_get_has_multiple_sizes (GnomeBGSlideShow *self)
{
        return self->priv->has_multiple_sizes;
}

/**
 * gnome_bg_slide_show_get_num_slides:
 * @self: a #GnomeBGSlideShow
 *
 * Returns number of slides in slide show
 **/
int
gnome_bg_slide_show_get_num_slides (GnomeBGSlideShow *self)
{
        return g_queue_get_length (self->priv->slides);
}
