/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2010  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <libgnome-control-center/cc-shell.h>

#include "cc-bluetooth-panel.h"

#include "bluetooth-plugin-manager.h"
#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "general.h"
#include "adapter.h"

G_DEFINE_DYNAMIC_TYPE (CcBluetoothPanel, cc_bluetooth_panel, CC_TYPE_PANEL)

static void cc_bluetooth_panel_finalize (GObject *object);

#define SCHEMA_NAME		"org.gnome.Bluetooth"
#define PREF_SHOW_ICON		"show-icon"

static void
receive_callback (GtkWidget *item, GtkWindow *window)
{
	GtkWidget *dialog;
	const char *command = "gnome-file-share-properties";

	if (!g_spawn_command_line_async(command, NULL)) {
		/* translators:
		 * This is the name of the preferences dialogue for gnome-user-share */
		dialog = gtk_message_dialog_new (window,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Cannot start \"Personal File Sharing\" Preferences"));

		/* translators:
		 * This is the name of the preferences dialogue for gnome-user-share */
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			_("Please verify that the \"Personal File Sharing\" program is correctly installed."));

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static void help_callback(GtkWidget *item)
{
	GError *error = NULL;

	if(!gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET(item)),
		"ghelp:gnome-bluetooth",  gtk_get_current_event_time (), &error)) {

		g_printerr("Unable to launch help: %s", error->message);
		g_error_free(error);
	}
}

static GtkWidget *
create_window (GtkWidget *notebook,
	       CcPanel   *panel)
{
	GtkWidget *vbox;
	GtkWidget *buttonbox;
	GtkWidget *button;
	GtkWidget *image;
	CcShell *shell;
	GtkWidget *toplevel;
	GSettings *settings;

	shell = cc_panel_get_shell (CC_PANEL (panel));
	toplevel = cc_shell_get_toplevel (shell);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	button = gtk_check_button_new_with_mnemonic (_("_Show Bluetooth icon"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	settings = g_settings_new (SCHEMA_NAME);
	g_settings_bind (settings, PREF_SHOW_ICON, G_OBJECT (button), "active",
			 G_SETTINGS_BIND_DEFAULT);

	buttonbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_HELP);
	gtk_container_add(GTK_CONTAINER(buttonbox), button);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(buttonbox),
					   button, TRUE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(help_callback), toplevel);

	image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO,
					  GTK_ICON_SIZE_BUTTON);
	button = gtk_button_new_with_label (_("Receive Files"));
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_container_add(GTK_CONTAINER(buttonbox), button);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(buttonbox),
					   button, TRUE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(receive_callback), toplevel);

	gtk_widget_show_all(vbox);

	return vbox;
}

static void
cc_bluetooth_panel_class_init (CcBluetoothPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cc_bluetooth_panel_finalize;
}

static void
cc_bluetooth_panel_class_finalize (CcBluetoothPanelClass *klass)
{
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	bluetooth_plugin_manager_cleanup ();

	cleanup_adapter();

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	GtkWidget *widget;
	GtkWidget *notebook;

	bluetooth_plugin_manager_init ();

	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	setup_adapter(GTK_NOTEBOOK(notebook));
	widget = create_window(notebook, CC_PANEL (self));

	gtk_container_add (GTK_CONTAINER (self), widget);
}

void
cc_bluetooth_panel_register (GIOModule *module)
{
	cc_bluetooth_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_BLUETOOTH_PANEL,
					"bluetooth", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_bluetooth_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

