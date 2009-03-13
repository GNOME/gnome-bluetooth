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
				"bluetooth-paired", NULL);

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

