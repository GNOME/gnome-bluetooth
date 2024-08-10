/*
 * Copyright (C) 2021 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gtk/gtk.h>
#include <adwaita.h>

#include "bluetooth-client.h"
#include "bluetooth-device.h"

static void
device_notify_cb (BluetoothDevice *device,
		  GParamSpec      *pspec,
		  gpointer         user_data)
{
	GtkWidget *label = user_data;
	g_autofree char *label_str = NULL;

	label_str = bluetooth_device_to_string (device);
	gtk_label_set_text (GTK_LABEL (label), label_str);
}

static GtkWidget *
create_device_cb (gpointer item, gpointer user_data)
{
	BluetoothDevice *device = BLUETOOTH_DEVICE (item);
	g_autofree char *label_str = NULL;
	GtkWidget *row, *label;

	label_str = bluetooth_device_to_string (device);
	label = gtk_label_new (label_str);
	row = gtk_list_box_row_new ();
	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
	g_signal_connect (G_OBJECT (device), "notify",
			  G_CALLBACK (device_notify_cb), label);
	return row;
}

int main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *listbox;
	GListStore *model;
	g_autoptr(BluetoothClient) client = NULL;

	gtk_init();
	adw_init ();

	window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(window), "Test client");
	gtk_window_set_icon_name(GTK_WINDOW(window), "bluetooth");
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

	client = bluetooth_client_new ();
	model = bluetooth_client_get_devices (client);
	listbox = gtk_list_box_new ();
	gtk_list_box_bind_model (GTK_LIST_BOX (listbox), G_LIST_MODEL (model), create_device_cb, NULL, NULL);
	gtk_window_set_child (GTK_WINDOW (window), listbox);
	gtk_window_present (GTK_WINDOW (window));

	while (g_list_model_get_n_items (gtk_window_get_toplevels()) > 0)
		g_main_context_iteration (NULL, TRUE);

	return 0;
}
