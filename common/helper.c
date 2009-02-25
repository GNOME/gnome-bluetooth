/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <bluetooth-client.h>
#include "helper.h"

static void connected_to_icon(GtkTreeViewColumn *column, GtkCellRenderer *cell,
			GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean connected;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_CONNECTED, &connected, -1);

	if (connected == TRUE)
		g_object_set(cell, "icon-name", GTK_STOCK_CONNECT, NULL);

	g_object_set(cell, "visible", connected, NULL);
}

static void trusted_to_icon(GtkTreeViewColumn *column, GtkCellRenderer *cell,
			GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean trusted;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if (trusted == TRUE)
		g_object_set(cell, "icon-name", GTK_STOCK_ABOUT, NULL);

	g_object_set(cell, "visible", trusted, NULL);
}

static void paired_to_icon(GtkTreeViewColumn *column, GtkCellRenderer *cell,
			GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PAIRED, &paired, -1);

	if (paired == TRUE)
		g_object_set(cell, "icon-name",
				GTK_STOCK_DIALOG_AUTHENTICATION, NULL);

	g_object_set(cell, "visible", paired, NULL);
}

GtkWidget *create_tree(GtkTreeModel *model, gboolean icons)
{
	GtkWidget *tree;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	tree = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_widget_set_size_request(tree, -1, 100);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), model);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Device");
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	if (icons == TRUE) {
		renderer = gtk_cell_renderer_pixbuf_new();
		gtk_tree_view_column_set_spacing(column, 4);
		gtk_tree_view_column_pack_start(column, renderer, FALSE);
		gtk_tree_view_column_add_attribute(column, renderer,
					"icon-name", BLUETOOTH_COLUMN_ICON);
	}

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
					"text", BLUETOOTH_COLUMN_ALIAS);

	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree), -1,
				"Connected", gtk_cell_renderer_pixbuf_new(),
					connected_to_icon, NULL, NULL);

	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree), -1,
				"Trusted", gtk_cell_renderer_pixbuf_new(),
					trusted_to_icon, NULL, NULL);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree), -1,
				"Paired", gtk_cell_renderer_pixbuf_new(),
					paired_to_icon, NULL, NULL);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_insert_column(GTK_TREE_VIEW(tree), column, -1);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(tree), column);

	return tree;
}

static void select_callback(GtkTreeSelection *selection, gpointer user_data)
{
	GtkWidget *dialog = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean selected;

	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (selected == TRUE) {
		gchar *address;

		address = g_object_get_data(G_OBJECT(dialog), "address");
		g_free(address);

		gtk_tree_model_get(model, &iter,
				BLUETOOTH_COLUMN_ADDRESS, &address, -1);

		g_object_set_data(G_OBJECT(dialog), "address", address);
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
						GTK_RESPONSE_ACCEPT, selected);
}

gchar *show_browse_dialog(void)
{
	BluetoothClient *client;
	GtkWidget *dialog;
	GtkWidget *scrolled;
	GtkWidget *tree;
	GtkTreeModel *model;
	GtkTreeModel *sorted;
	GtkTreeSelection *selection;
	gchar *address = NULL;

	dialog = gtk_dialog_new_with_buttons(_("Select Device"),
				NULL, GTK_DIALOG_NO_SEPARATOR,
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
						GTK_RESPONSE_ACCEPT, FALSE);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 400);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 2);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
							GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), scrolled);

	client = bluetooth_client_new();

	model = bluetooth_client_get_device_model(client, NULL);
	if (model != NULL) {
		sorted = gtk_tree_model_sort_new_with_model(model);
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sorted),
				BLUETOOTH_COLUMN_RSSI, GTK_SORT_DESCENDING);
	} else
		sorted = NULL;

	tree = create_tree(sorted, TRUE);

	if (sorted != NULL)
		g_object_unref(sorted);
	if (model != NULL)
		g_object_unref(model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(select_callback), dialog);

	gtk_container_add(GTK_CONTAINER(scrolled), tree);

	gtk_widget_show_all(scrolled);

	bluetooth_client_start_discovery(client);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		address = g_object_get_data(G_OBJECT(dialog), "address");

	bluetooth_client_stop_discovery(client);

	g_object_unref(client);

	gtk_widget_destroy(dialog);

	return address;
}

gchar **show_select_dialog(void)
{
	GtkWidget *dialog;
	gchar **files = NULL;

	dialog = gtk_file_chooser_dialog_new(_("Choose files to send"), NULL,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GSList *list, *filenames;
		int i;

		filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

		files = g_new(gchar *, g_slist_length(filenames) + 1);

		for (list = filenames, i = 0; list; list = list->next, i++)
			files[i] = list->data;
		files[i] = NULL;

		g_slist_free(filenames);
	}

	gtk_widget_destroy(dialog);

	return files;
}
