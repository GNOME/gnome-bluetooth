/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009, Intel Corporation.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Written by: Joshua Lock <josh@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <mx/mx-gtk.h>
#include <moblin-panel/mpl-panel-common.h>
#include <moblin-panel/mpl-panel-gtk.h>
#include <bluetooth-enums.h>

#include "moblin-panel.h"

static void
bluetooth_status_changed (MoblinPanel *panel, gboolean connecting, gpointer user_data)
{
	MplPanelClient *client = MPL_PANEL_CLIENT (user_data);
	gchar *style = NULL;

	if (connecting) {
		style = g_strdup ("state-connecting");
	} else {
		style = g_strdup ("state-idle");
	}

	mpl_panel_client_request_button_style (client, style);
	g_free (style);
}

static void
panel_request_focus (MoblinPanel *panel, gpointer user_data)
{
	MplPanelClient *client = MPL_PANEL_CLIENT (user_data);

	mpl_panel_client_request_focus (client);
}

/*
 * When the panel is hidden we should re-set the MoblinPanel to its default state.
 * i.e. stop any discovery and show the defaults devices view
 */
static void
panel_hidden_cb (MplPanelClient *client, gpointer user_data)
{
	MoblinPanel *panel = MOBLIN_PANEL (user_data);

	moblin_panel_hidden (panel);
}

static void
panel_shown_cb (MplPanelClient *client, gpointer user_data)
{
	MoblinPanel *panel = MOBLIN_PANEL (user_data);

	moblin_panel_shown (panel);
}

int
main (int argc, char *argv[])
{
	MplPanelClient *panel;
	GtkWidget      *window, *content;
	GtkRequisition	req;
	gboolean	standalone = FALSE;
	GtkSettings    *settings;
	GError	       *error = NULL;
	GOptionEntry	entries[] = {
		{ "standalone", 's', 0, G_OPTION_ARG_NONE, &standalone,
		_("Run in standalone mode"), NULL },
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("Moblin Bluetooth Panel"));
	gtk_init_with_args (&argc, &argv, _("- Moblin Bluetooth Panel"),
				entries, GETTEXT_PACKAGE, &error);

	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		return 1;
	}

	gtk_window_set_default_icon_name ("bluetooth");

	/* Force to correct theme */
	settings = gtk_settings_get_default ();
	gtk_settings_set_string_property (settings, "gtk-theme-name",
					"Moblin-Netbook", NULL);

	if (standalone) {
		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		g_signal_connect (window, "delete-event", (GCallback) gtk_main_quit,
				NULL);
		gtk_widget_set_size_request (window, 1000, -1);
		content = moblin_panel_new ();
		gtk_widget_show (content);

		gtk_container_add (GTK_CONTAINER (window), content);
		gtk_widget_show (window);

		moblin_panel_shown (MOBLIN_PANEL (content));
	}  else {
		GtkWidget *box, *label;
		GdkScreen *screen;
		char *s;
		panel = mpl_panel_gtk_new (MPL_PANEL_BLUETOOTH, _("bluetooth"),
					THEME_DIR "/bluetooth-panel.css",
					"state-idle", TRUE);
		window	= mpl_panel_gtk_get_window (MPL_PANEL_GTK (panel));
		box = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (box);
		gtk_container_add (GTK_CONTAINER (window), box);

		label = gtk_label_new (NULL);
		screen = gdk_screen_get_default ();
		s = g_strdup_printf ("<span foreground=\"#31c2ee\" weight=\"bold\" size=\"%d\">%s</span>",
				     (int)(PANGO_SCALE * (22 * 72 / gdk_screen_get_resolution (screen))),
				     _("Bluetooth"));
		g_object_set (label,
			      "label", s,
			      "use-markup", TRUE,
			      "xalign", 0.0f,
			      "xpad", 16,
			      "ypad", 8,
			      NULL);
		g_free (s);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

		content = moblin_panel_new ();
		g_signal_connect (panel, "show", (GCallback) panel_shown_cb, content);
		g_signal_connect (panel, "hide-end", (GCallback) panel_hidden_cb, content);
		g_signal_connect (content, "state-changed",
				G_CALLBACK (bluetooth_status_changed), panel);
		g_signal_connect (content, "request-focus",
				  G_CALLBACK (panel_request_focus), panel);
		gtk_widget_show (content);
		gtk_box_pack_start (GTK_BOX (box), content, TRUE, TRUE, 0);

		gtk_widget_size_request (window, &req);
		mpl_panel_client_set_height_request (panel, req.height);
	}

	gtk_main ();

	return 0;
}
