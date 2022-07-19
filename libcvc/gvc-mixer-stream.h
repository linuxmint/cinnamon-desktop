/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GVC_MIXER_STREAM_H
#define __GVC_MIXER_STREAM_H

#include <glib-object.h>
#include "gvc-channel-map.h"
#include <gio/gio.h>
#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define GVC_TYPE_MIXER_STREAM         (gvc_mixer_stream_get_type ())
#define GVC_MIXER_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_MIXER_STREAM, GvcMixerStream))
#define GVC_MIXER_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GVC_TYPE_MIXER_STREAM, GvcMixerStreamClass))
#define GVC_IS_MIXER_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_MIXER_STREAM))
#define GVC_IS_MIXER_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_MIXER_STREAM))
#define GVC_MIXER_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_MIXER_STREAM, GvcMixerStreamClass))

typedef struct GvcMixerStreamPrivate GvcMixerStreamPrivate;

typedef struct
{
        GObject                parent;
        GvcMixerStreamPrivate *priv;
} GvcMixerStream;

typedef struct
{
        GObjectClass           parent_class;

        /* vtable */
        gboolean (*push_volume)     (GvcMixerStream *stream,
                                     gpointer *operation);
        gboolean (*change_is_muted) (GvcMixerStream *stream,
                                     gboolean        is_muted);
        gboolean (*change_port)     (GvcMixerStream *stream,
                                     const char     *port);
        void (*monitor_update)      (GvcMixerStream *stream,
                                     gdouble        v);
        void (*monitor_suspend)     (GvcMixerStream *stream);
} GvcMixerStreamClass;

typedef struct
{
        char *port;
        char *human_port;
        guint priority;
        gboolean available;
} GvcMixerStreamPort;

typedef enum
{
        GVC_STREAM_STATE_INVALID,
        GVC_STREAM_STATE_RUNNING,
        GVC_STREAM_STATE_IDLE,
        GVC_STREAM_STATE_SUSPENDED
} GvcMixerStreamState;

GType               gvc_mixer_stream_port_get_type   (void) G_GNUC_CONST;
GType               gvc_mixer_stream_get_type        (void) G_GNUC_CONST;

guint               gvc_mixer_stream_get_index       (GvcMixerStream *stream);
guint               gvc_mixer_stream_get_id          (GvcMixerStream *stream);
const GvcChannelMap *gvc_mixer_stream_get_channel_map(GvcMixerStream *stream);
const GvcMixerStreamPort *gvc_mixer_stream_get_port  (GvcMixerStream *stream);
const GList *       gvc_mixer_stream_get_ports       (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_change_port     (GvcMixerStream *stream,
                                                      const char     *port);

guint32             gvc_mixer_stream_get_volume      (GvcMixerStream *stream);
gdouble             gvc_mixer_stream_get_decibel     (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_push_volume     (GvcMixerStream *stream);
guint32             gvc_mixer_stream_get_base_volume (GvcMixerStream *stream);

gboolean            gvc_mixer_stream_get_is_muted    (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_get_can_decibel (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_change_is_muted (GvcMixerStream *stream,
                                                      gboolean        is_muted);
gboolean            gvc_mixer_stream_is_running      (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_name        (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_icon_name   (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_form_factor (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_sysfs_path  (GvcMixerStream *stream);
GIcon *             gvc_mixer_stream_get_gicon       (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_description (GvcMixerStream *stream);
const char *        gvc_mixer_stream_get_application_id (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_is_event_stream (GvcMixerStream *stream);
gboolean            gvc_mixer_stream_is_virtual      (GvcMixerStream *stream);
guint               gvc_mixer_stream_get_card_index  (GvcMixerStream *stream);
GvcMixerStreamState gvc_mixer_stream_get_state       (GvcMixerStream *stream);
void                gvc_mixer_stream_create_monitor  (GvcMixerStream *stream);
void                gvc_mixer_stream_remove_monitor  (GvcMixerStream *stream);
/* private */
gboolean            gvc_mixer_stream_set_volume      (GvcMixerStream *stream,
                                                      guint32     volume);
gboolean            gvc_mixer_stream_set_decibel     (GvcMixerStream *stream,
                                                      gdouble         db);
gboolean            gvc_mixer_stream_set_is_muted    (GvcMixerStream *stream,
                                                      gboolean        is_muted);
gboolean            gvc_mixer_stream_set_can_decibel (GvcMixerStream *stream,
                                                      gboolean        can_decibel);
gboolean            gvc_mixer_stream_set_name        (GvcMixerStream *stream,
                                                      const char     *name);
gboolean            gvc_mixer_stream_set_description (GvcMixerStream *stream,
                                                      const char     *description);
gboolean            gvc_mixer_stream_set_icon_name   (GvcMixerStream *stream,
                                                      const char     *name);
gboolean            gvc_mixer_stream_set_form_factor (GvcMixerStream *stream,
                                                      const char     *form_factor);
gboolean            gvc_mixer_stream_set_sysfs_path  (GvcMixerStream *stream,
                                                      const char     *sysfs_path);
gboolean            gvc_mixer_stream_set_is_event_stream (GvcMixerStream *stream,
                                                          gboolean is_event_stream);
gboolean            gvc_mixer_stream_set_is_virtual  (GvcMixerStream *stream,
                                                      gboolean is_event_stream);
gboolean            gvc_mixer_stream_set_application_id (GvcMixerStream *stream,
                                                         const char *application_id);
gboolean            gvc_mixer_stream_set_base_volume (GvcMixerStream *stream,
                                                      guint32     base_volume);
gboolean            gvc_mixer_stream_set_port        (GvcMixerStream *stream,
                                                      const char     *port);
gboolean            gvc_mixer_stream_set_ports       (GvcMixerStream *stream,
                                                      GList          *ports);
gboolean            gvc_mixer_stream_set_card_index  (GvcMixerStream *stream,
                                                      guint           card_index);
gboolean            gvc_mixer_stream_set_state       (GvcMixerStream      *stream,
                                                      GvcMixerStreamState  state);

G_END_DECLS

#endif /* __GVC_MIXER_STREAM_H */
