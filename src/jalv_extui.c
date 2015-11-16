/*
  Copyright 2007-2013 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdlib.h>
#include "jalv_internal.h"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

bool
jalv_discover_ui(Jalv* jalv)
{
	return TRUE;
}

void
on_external_ui_closed(void* controller)
{
	Jalv* jalv = (Jalv*) controller;
	jalv_close_ui(jalv);
}


int
jalv_init(int* argc, char*** argv, JalvOptions* opts)
{
#ifdef HAVE_GLIB
	GOptionEntry entries[] = {
		{ "uuid", 'u', 0, G_OPTION_ARG_STRING, &opts->uuid,
			"UUID for Jack session restoration", "UUID" },
		{ "load", 'l', 0, G_OPTION_ARG_STRING, &opts->load,
			"Load state from save directory", "DIR" },
		{ "dump", 'd', 0, G_OPTION_ARG_NONE, &opts->dump,
			"Dump plugin <=> UI communication", NULL },
		{ "show-hidden", 's', 0, G_OPTION_ARG_NONE, &opts->show_hidden,
			"Show controls for ports with notOnGUI property on generic UI", NULL },
		{ "no-menu", 'n', 0, G_OPTION_ARG_NONE, &opts->no_menu,
			"Do not show Jalv menu on window", NULL },
		{ "generic-ui", 'g', 0, G_OPTION_ARG_NONE, &opts->generic_ui,
			"Use Jalv generic UI and not the plugin UI", NULL},
		{ "buffer-size", 'b', 0, G_OPTION_ARG_INT, &opts->buffer_size,
			"Buffer size for plugin <=> UI communication", "SIZE"},
		{ "update-frequency", 'r', 0, G_OPTION_ARG_DOUBLE, &opts->update_rate,
			"UI update frequency", NULL},
		{ "control", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &opts->controls,
			"Set control value (e.g. \"vol=1.4\")", NULL},
		{ "print-controls", 'p', 0, G_OPTION_ARG_NONE, &opts->print_controls,
			"Print control output changes to stdout", NULL},
		{ 0, 0, 0, 0, 0, 0, 0 } };

		GError *error = NULL;
		GOptionContext *context;

		context = g_option_context_new ("PLUGIN_URI - Run an LV2 plugin as a Jack application");
		g_option_context_add_main_entries (context, entries, NULL);
		if (!g_option_context_parse (context, argc, argv, &error)) {
			fprintf(stderr, "%s\n", error->message);
			return -1;
		}
#endif
		return 0;
}

const char*
jalv_native_ui_type(Jalv* jalv)
{
	return NULL;
}

int
jalv_ui_resize(Jalv* jalv, int width, int height)
{
	return 0;
}

void
jalv_ui_port_event(Jalv*       jalv,
                   uint32_t    port_index,
                   uint32_t    buffer_size,
                   uint32_t    protocol,
                   const void* buffer)
{
}

int
jalv_open_ui(Jalv* jalv)
{
	if (!jalv->externalui) {
		return -1;
	}
	if (!jalv->ui || jalv->opts.generic_ui) {
		return -1;
	}
	jalv->has_ui = 1;

	LV2_External_UI_Host extui;
	extui.ui_closed = on_external_ui_closed;
	LilvNode* name = lilv_plugin_get_name(jalv->plugin);
	extui.plugin_human_id = jalv_strdup(lilv_node_as_string(name));
	lilv_node_free(name);
	jalv_ui_instantiate(jalv, lilv_node_as_uri (jalv->ui_type), &extui);
	LV2_EXTERNAL_UI_SHOW(jalv->extuiptr);

	const LV2UI_Idle_Interface* idle_iface = (const LV2UI_Idle_Interface*)
		suil_instance_extension_data(jalv->ui_instance, LV2_UI__idleInterface);

	while (!zix_sem_try_wait(jalv->done)) {
#ifdef _WIN32
		Sleep(1000 / jalv->ui_update_hz);
#else
		usleep(1000000 / jalv->ui_update_hz);
#endif
		jalv_update(jalv);
		if (idle_iface) {
			idle_iface->idle(suil_instance_get_handle(jalv->ui_instance));
		}
	}
	zix_sem_post(jalv->done);
	return 0;
}

int
jalv_close_ui(Jalv* jalv)
{
	zix_sem_post(jalv->done);
}
