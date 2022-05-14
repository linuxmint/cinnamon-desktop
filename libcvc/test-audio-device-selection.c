
#include <stdio.h>
#include <locale.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"

#define MAX_ATTEMPTS 3

typedef struct {
	GvcHeadsetPortChoice choice;
	const char *name;
} AudioSelectionChoice;

static AudioSelectionChoice audio_selection_choices[] = {
	{ GVC_HEADSET_PORT_CHOICE_HEADPHONES,   "headphones" },
	{ GVC_HEADSET_PORT_CHOICE_HEADSET,      "headset" },
	{ GVC_HEADSET_PORT_CHOICE_MIC,          "microphone" },
};

static void
audio_selection_needed (GvcMixerControl      *volume,
			guint                 id,
			gboolean              show_dialog,
			GvcHeadsetPortChoice  choices,
			gpointer              user_data)
{
	const char *args[G_N_ELEMENTS (audio_selection_choices) + 1];
	guint i, n;
	int response = -1;

	if (!show_dialog) {
		g_print ("--- Audio selection not needed anymore for id %d\n", id);
		return;
	}

	n = 0;
	for (i = 0; i < G_N_ELEMENTS (audio_selection_choices); ++i) {
		if (choices & audio_selection_choices[i].choice)
			args[n++] = audio_selection_choices[i].name;
	}
	args[n] = NULL;

	g_print ("+++ Audio selection needed for id %d\n", id);
	g_print ("    Choices are:\n");
	for (i = 0; args[i] != NULL; i++)
		g_print ("    %d. %s\n", i + 1, args[i]);

	for (i = 0; response < 0 && i < MAX_ATTEMPTS; i++) {
		int res;

		g_print ("What is your choice?\n");
		if (scanf ("%d", &res) == 1 &&
		    res > 0 &&
		    res < (int) g_strv_length ((char **)  args)) {
			response = res;
			break;
		}
	}

	gvc_mixer_control_set_headset_port (volume,
					    id,
					    audio_selection_choices[response - 1].choice);
}

int main (int argc, char **argv)
{
	GMainLoop *loop;
	GvcMixerControl *volume;

	setlocale (LC_ALL, "");

	loop = g_main_loop_new (NULL, FALSE);

	volume = gvc_mixer_control_new ("GNOME Volume Control test");
	g_signal_connect (volume,
			  "audio-device-selection-needed",
			  G_CALLBACK (audio_selection_needed),
			  NULL);
	gvc_mixer_control_open (volume);

	g_main_loop_run (loop);

	return 0;
}
