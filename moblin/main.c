/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <nbtk/nbtk-gtk.h>
#include <moblin-panel/mpl-panel-common.h>
#include <moblin-panel/mpl-panel-gtk.h>

#include "moblin-panel.h"

#define PKGTHEMEDIR PKGDATADIR"/theme"

static void
make_window_content (GtkWidget *panel)
{
	GtkWidget *content;

	content = moblin_panel_new ();
	gtk_widget_set_size_request (content, 800, -1);
	gtk_widget_show (content);

	gtk_container_add (GTK_CONTAINER (panel), content);
	gtk_widget_show (panel);
}

int
main (int argc, char *argv[])
{
	MplPanelClient *panel;
	GtkWidget      *window;
	gboolean        standalone = FALSE;
	GtkSettings    *settings;
	GError         *error = NULL;
	GOptionEntry    entries[] = {
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

	/* Force to correct theme */
	settings = gtk_settings_get_default ();
	gtk_settings_set_string_property (settings, "gtk-theme-name",
					"Moblin-Netbook", NULL);

	if (standalone) {
		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		g_signal_connect (window, "delete-event", (GCallback) gtk_main_quit,
				NULL);
		gtk_widget_set_size_request (window, 1000, -1);
		make_window_content (window);
	}  else {
		panel = mpl_panel_gtk_new (MPL_PANEL_BLUETOOTH, _("bluetooth"),
					PKGTHEMEDIR "/bluetooth-panel.css",
					"state-dile", TRUE);
		mpl_panel_client_set_height_request (panel, 450);
		window  = mpl_panel_gtk_get_window (MPL_PANEL_GTK (panel));

		make_window_content (GTK_WIDGET (panel));
	}

	gtk_main ();

	return 0;
}
