/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>

#include "bluetooth-chooser.h"
#include "bluetooth-client.h"

static void device_selected_cb(GObject *object,
			       GParamSpec *spec, gpointer user_data)
{
	g_message ("Property \"device-selected\" changed");
}

static void device_type_filter_selected_cb(GObject *object,
					   GParamSpec *spec, gpointer user_data)
{
	g_message ("Property \"device-type-filter\" changed");
}

static void device_category_filter_selected_cb(GObject *object,
				GParamSpec *spec, gpointer user_data)
{
	g_message ("Property \"device-category-filter\" changed");
}

static void select_device_changed(BluetoothChooser *sel,
				  gchar *address, gpointer user_data)
{
	GtkDialog *dialog = user_data;

	gtk_dialog_set_response_sensitive(dialog,
				GTK_RESPONSE_ACCEPT, address != NULL);
}

static GtkWidget *
create_dialogue (const char *title)
{
	GtkWidget *dialog;

	dialog = gtk_dialog_new_with_buttons(title, NULL,
				GTK_DIALOG_NO_SEPARATOR,
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
					GTK_RESPONSE_ACCEPT, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 400);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	return dialog;
}

static GtkWidget *
create_phone_dialogue (void)
{
	GtkWidget *dialog, *selector;

	dialog = create_dialogue ("Browse Devices");

	selector = bluetooth_chooser_new("Select device to use");
	gtk_container_set_border_width(GTK_CONTAINER(selector), 5);
	gtk_widget_show(selector);
	g_object_set(selector,
		     "show-search", TRUE,
		     "device-type-filter", BLUETOOTH_TYPE_PHONE,
		     NULL);
	g_object_set_data (G_OBJECT (dialog), "selector", selector);

	return dialog;
}

static GtkWidget *
create_wizard_dialogue (void)
{
	GtkWidget *dialog, *selector;

	dialog = create_dialogue ("Add a Device");

	selector = bluetooth_chooser_new("Select new device to setup");
	gtk_container_set_border_width(GTK_CONTAINER(selector), 5);
	gtk_widget_show(selector);
	g_object_set(selector,
		     "show-search", TRUE,
		     "show-device-category", FALSE,
		     "device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
		     NULL);
	g_object_set_data (G_OBJECT (dialog), "selector", selector);

	return dialog;
}


int main(int argc, char **argv)
{
	GtkWidget *dialog, *selector;
	const char *selection;
	int response;

	gtk_init(&argc, &argv);
	if (argc < 2)
		selection = "wizard";
	else
		selection = argv[1];

	if (g_str_equal (selection, "phone")) {
		dialog = create_phone_dialogue ();
	} else if (g_str_equal (selection, "wizard")) {
		dialog = create_wizard_dialogue ();
	} else {
		g_warning ("Unknown dialogue type, try either \"phone\" or \"wizard\"");
		return 1;
	}

	selector = g_object_get_data (G_OBJECT (dialog), "selector");

	g_signal_connect(selector, "selected-device-changed",
			 G_CALLBACK(select_device_changed), dialog);
	g_signal_connect(selector, "notify::device-selected",
			 G_CALLBACK(device_selected_cb), dialog);
	g_signal_connect(selector, "notify::device-type-filter",
			 G_CALLBACK(device_type_filter_selected_cb), dialog);
	g_signal_connect(selector, "notify::device-category-filter",
			 G_CALLBACK(device_category_filter_selected_cb), dialog);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), selector);
	bluetooth_chooser_start_discovery (BLUETOOTH_CHOOSER (selector));
	gtk_widget_show(dialog);

	response = gtk_dialog_run(GTK_DIALOG(dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		char *address;

		g_object_get(selector, "device-selected", &address, NULL);
		g_message("Selected device is: %s", address);
		g_free(address);
	}

	gtk_widget_destroy(dialog);

	return 0;
}
