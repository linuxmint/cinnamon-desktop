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

#ifndef __GVC_MIXER_CONTROL_H
#define __GVC_MIXER_CONTROL_H

#include <glib-object.h>
#include "gvc-mixer-stream.h"
#include "gvc-mixer-card.h"
#include "gvc-mixer-ui-device.h"

G_BEGIN_DECLS

typedef enum
{
        GVC_STATE_CLOSED,
        GVC_STATE_READY,
        GVC_STATE_CONNECTING,
        GVC_STATE_FAILED
} GvcMixerControlState;

typedef enum
{
        GVC_HEADSET_PORT_CHOICE_NONE        = 0,
        GVC_HEADSET_PORT_CHOICE_HEADPHONES  = 1 << 0,
        GVC_HEADSET_PORT_CHOICE_HEADSET     = 1 << 1,
        GVC_HEADSET_PORT_CHOICE_MIC         = 1 << 2
} GvcHeadsetPortChoice;

#define GVC_TYPE_MIXER_CONTROL         (gvc_mixer_control_get_type ())
#define GVC_MIXER_CONTROL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_MIXER_CONTROL, GvcMixerControl))
#define GVC_MIXER_CONTROL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GVC_TYPE_MIXER_CONTROL, GvcMixerControlClass))
#define GVC_IS_MIXER_CONTROL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_MIXER_CONTROL))
#define GVC_IS_MIXER_CONTROL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_MIXER_CONTROL))
#define GVC_MIXER_CONTROL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_MIXER_CONTROL, GvcMixerControlClass))

typedef struct GvcMixerControlPrivate GvcMixerControlPrivate;

typedef struct
{
        GObject                 parent;
        GvcMixerControlPrivate *priv;
} GvcMixerControl;

typedef struct
{
        GObjectClass            parent_class;

        void (*state_changed)          (GvcMixerControl      *control,
                                        GvcMixerControlState  new_state);
        void (*stream_added)           (GvcMixerControl *control,
                                        guint            id);
        void (*stream_changed)         (GvcMixerControl *control,
                                        guint            id);
        void (*stream_removed)         (GvcMixerControl *control,
                                        guint            id);
        void (*card_added)             (GvcMixerControl *control,
                                        guint            id);
        void (*card_removed)           (GvcMixerControl *control,
                                        guint            id);
        void (*default_sink_changed)   (GvcMixerControl *control,
                                        guint            id);
        void (*default_source_changed) (GvcMixerControl *control,
                                        guint            id);
        void (*active_output_update)   (GvcMixerControl *control,
                                        guint            id);
        void (*active_input_update)    (GvcMixerControl *control,
                                        guint            id);
        void (*output_added)           (GvcMixerControl *control,
                                        guint            id);
        void (*input_added)            (GvcMixerControl *control,
                                        guint            id);
        void (*output_removed)         (GvcMixerControl *control,
                                        guint            id);
        void (*input_removed)          (GvcMixerControl *control,
                                        guint            id);
        void (*audio_device_selection_needed)
                                       (GvcMixerControl      *control,
                                        guint                 id,
                                        gboolean              show_dialog,
                                        GvcHeadsetPortChoice  choices);
} GvcMixerControlClass;

GType               gvc_mixer_control_get_type            (void);

GvcMixerControl *   gvc_mixer_control_new                 (const char *name);

gboolean            gvc_mixer_control_open                (GvcMixerControl *control);
gboolean            gvc_mixer_control_close               (GvcMixerControl *control);

GSList *            gvc_mixer_control_get_cards           (GvcMixerControl *control);
GSList *            gvc_mixer_control_get_streams         (GvcMixerControl *control);
GSList *            gvc_mixer_control_get_sinks           (GvcMixerControl *control);
GSList *            gvc_mixer_control_get_sources         (GvcMixerControl *control);
GSList *            gvc_mixer_control_get_sink_inputs     (GvcMixerControl *control);
GSList *            gvc_mixer_control_get_source_outputs  (GvcMixerControl *control);

GvcMixerStream *        gvc_mixer_control_lookup_stream_id          (GvcMixerControl *control,
                                                                     guint            id);
GvcMixerCard   *        gvc_mixer_control_lookup_card_id            (GvcMixerControl *control,
                                                                     guint            id);
GvcMixerUIDevice *      gvc_mixer_control_lookup_output_id          (GvcMixerControl *control,
                                                                     guint            id);
GvcMixerUIDevice *      gvc_mixer_control_lookup_input_id           (GvcMixerControl *control,
                                                                     guint            id);
GvcMixerUIDevice *      gvc_mixer_control_lookup_device_from_stream (GvcMixerControl *control,
                                                                     GvcMixerStream *stream);

GvcMixerStream *        gvc_mixer_control_get_default_sink     (GvcMixerControl *control);
GvcMixerStream *        gvc_mixer_control_get_default_source   (GvcMixerControl *control);
GvcMixerStream *        gvc_mixer_control_get_event_sink_input (GvcMixerControl *control);

gboolean                gvc_mixer_control_set_default_sink     (GvcMixerControl *control,
                                                                GvcMixerStream  *stream);
gboolean                gvc_mixer_control_set_default_source   (GvcMixerControl *control,
                                                                GvcMixerStream  *stream);

gdouble                 gvc_mixer_control_get_vol_max_norm                  (GvcMixerControl *control);
gdouble                 gvc_mixer_control_get_vol_max_amplified             (GvcMixerControl *control);
void                    gvc_mixer_control_change_output                     (GvcMixerControl *control,
                                                                             GvcMixerUIDevice* output);
void                    gvc_mixer_control_change_input                      (GvcMixerControl *control,
                                                                             GvcMixerUIDevice* input);
GvcMixerStream*         gvc_mixer_control_get_stream_from_device            (GvcMixerControl *control,
                                                                             GvcMixerUIDevice *device);
gboolean                gvc_mixer_control_change_profile_on_selected_device (GvcMixerControl *control,
                                                                             GvcMixerUIDevice *device,
                                                                             const gchar* profile);

void                    gvc_mixer_control_set_headset_port                  (GvcMixerControl      *control,
                                                                             guint                 id,
                                                                             GvcHeadsetPortChoice  choices);

GvcMixerControlState gvc_mixer_control_get_state            (GvcMixerControl *control);

G_END_DECLS

#endif /* __GVC_MIXER_CONTROL_H */
