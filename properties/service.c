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

#include <dbus/dbus-glib.h>

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "general.h"
#include "service.h"
#include "network.h"
#include "input.h"
#include "audio.h"

static DBusGConnection *connection = NULL;
static DBusGProxy *manager = NULL;

static GtkWidget *notebook;
static GtkWidget *page_network;
static GtkWidget *page_input;
static GtkWidget *page_audio;

static GtkListStore *service_store;

enum {
	COLUMN_PATH,
	COLUMN_IDENT,
};

static void show_service(const gchar *identifier)
{
	GtkWidget *widget = NULL;
	gint page = -1;

	if (g_ascii_strcasecmp(identifier, "network") == 0)
		widget = page_network;

	if (g_ascii_strcasecmp(identifier, "input") == 0)
		widget = page_input;

	if (g_ascii_strcasecmp(identifier, "audio") == 0)
		widget = page_audio;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), widget);
	if (page < 0) {
		gtk_widget_hide(notebook);
		return;
	}

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page);
	gtk_widget_show(notebook);
}

static void select_callback(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean selected;
	gchar *identifier;

	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (selected == TRUE) {
		gtk_tree_model_get(model, &iter, COLUMN_IDENT, &identifier, -1);
		show_service(identifier);
		g_free(identifier);
	} else
		gtk_widget_hide(notebook);
}

static void ident_to_text(GtkTreeViewColumn *column, GtkCellRenderer *cell,
			GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gchar *identifier, *text;

	gtk_tree_model_get(model, iter, COLUMN_IDENT, &identifier, -1);
	if (identifier == NULL)
		text = "unknown";

	if (g_ascii_strcasecmp(identifier, "network") == 0)
		text = _("Network service");

	if (g_ascii_strcasecmp(identifier, "input") == 0)
		text = _("Input service");

	if (g_ascii_strcasecmp(identifier, "audio") == 0)
		text = _("Audio service");

	if (g_ascii_strcasecmp(identifier, "serial") == 0)
		text = _("Serial service");

	g_object_set(cell, "text", text, NULL);

	g_free(identifier);
}

GtkWidget *create_service(void)
{
	GtkWidget *mainbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *scrolled;
	GtkWidget *tree;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	mainbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 12);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, FALSE, 0);

	label = create_label(_("Available services"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
							GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(vbox), scrolled);

	tree = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_widget_set_size_request(tree, -1, 130);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree),
					GTK_TREE_MODEL(service_store));

	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(tree), 0,
					"Name", gtk_cell_renderer_text_new(),
						ident_to_text, NULL, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree), 0);
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), TRUE);

	gtk_container_add(GTK_CONTAINER(scrolled), tree);

	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
	gtk_widget_set_no_show_all(notebook, TRUE);
	gtk_container_add(GTK_CONTAINER(mainbox), notebook);

	page_network = create_network();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page_network, NULL);

	page_input = create_input();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page_input, NULL);

	page_audio = create_audio();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page_audio, NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(select_callback), NULL);

	return mainbox;
}

static GList *service_list = NULL;

struct service_data {
	DBusGProxy *proxy;
	gchar *path;
	gchar *identifier;
	gboolean present;
	GtkTreeIter iter;
};

static void service_free(gpointer data, gpointer user_data)
{
	struct service_data *service = data;

	service_list = g_list_remove(service_list, service);

	g_free(service->identifier);
	g_free(service->path);
	g_free(service);
}

static void service_disable(gpointer data, gpointer user_data)
{
	struct service_data *service = data;

	if (service->present) {
		service->present = 0;
		g_object_unref(service->proxy);
		service->proxy = NULL;
		gtk_list_store_remove(service_store, &service->iter);
	}
}

static gint service_compare(gconstpointer a, gconstpointer b)
{
	const struct service_data *service = a;
	const char *path = b;

	return g_ascii_strcasecmp(service->path, path);
}

static void service_started(DBusGProxy *object, gpointer user_data)
{
	struct service_data *service = user_data;
	const char *busname;

	dbus_g_proxy_call(manager, "ActivateService", NULL,
			G_TYPE_STRING, service->identifier, G_TYPE_INVALID,
				G_TYPE_STRING, &busname, G_TYPE_INVALID);

	if (g_ascii_strcasecmp(service->identifier, "network") == 0)
		enable_network(connection, busname);

	if (g_ascii_strcasecmp(service->identifier, "input") == 0)
		enable_input(connection, busname);

	if (g_ascii_strcasecmp(service->identifier, "audio") == 0)
		enable_audio(connection, busname);
}

static void service_stopped(DBusGProxy *object, gpointer user_data)
{
	struct service_data *service = user_data;

	if (g_ascii_strcasecmp(service->identifier, "network") == 0)
		disable_network();

	if (g_ascii_strcasecmp(service->identifier, "input") == 0)
		disable_input();

	if (g_ascii_strcasecmp(service->identifier, "audio") == 0)
		disable_audio();
}

static void add_service(const char *path)
{
	DBusGProxy *object;
	GList *list;
	struct service_data *service;
	const char *identifier = NULL;
	gboolean running = FALSE;

	list = g_list_find_custom(service_list, path, service_compare);
	if (list && list->data) {
		service = list->data;
		goto done;
	}

	service = g_try_malloc0(sizeof(*service));
	if (!service)
		return;

	service->path = g_strdup(path);

	service_list = g_list_append(service_list, service);

done:
	service->present = 1;

	object = dbus_g_proxy_new_for_name(connection, "org.bluez",
						path, "org.bluez.Service");

	service->proxy = object;

	dbus_g_proxy_add_signal(object, "Started", G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(object, "Started",
				G_CALLBACK(service_started), service, NULL);

	dbus_g_proxy_add_signal(object, "Stopped", G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(object, "Stopped",
				G_CALLBACK(service_stopped), service, NULL);

	dbus_g_proxy_call(object, "GetIdentifier", NULL, G_TYPE_INVALID,
				G_TYPE_STRING, &identifier, G_TYPE_INVALID);

	if (g_ascii_strcasecmp(identifier, "headset") == 0 ||
				g_ascii_strcasecmp(identifier, "sink") == 0) {
		g_object_unref(object);
		service_free(service, NULL);
		return;
	}

	g_free(service->identifier);
	service->identifier = g_strdup(identifier);
	if (service->identifier == NULL)
		service->identifier = g_strdup("");

	gtk_list_store_insert_with_values(service_store, &service->iter, -1,
			COLUMN_PATH, path, COLUMN_IDENT, identifier, -1);

	if (running == TRUE)
		service_started(object, service);
}

static void service_added(DBusGProxy *object,
				const char *path, gpointer user_data)
{
	add_service(path);
}

static void service_removed(DBusGProxy *object,
				const char *path, gpointer user_data)
{
	GList *list;

	list = g_list_find_custom(service_list, path, service_compare);
	if (list && list->data) {
		struct service_data *service = list->data;

		service->present = 0;

		g_free(service->identifier);
		service->identifier = NULL;

		gtk_list_store_remove(service_store, &service->iter);
	}
}

void setup_service(DBusGConnection *conn)
{
	GError *error = NULL;
	const gchar **array = NULL;

	service_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	if (!service_store)
		return;

	manager = dbus_g_proxy_new_for_name(conn, "org.bluez",
					"/org/bluez", "org.bluez.Manager");
	if (manager == NULL)
		return;

	connection = dbus_g_connection_ref(conn);

	dbus_g_proxy_add_signal(manager, "ServiceAdded",
					G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(manager, "ServiceAdded",
				G_CALLBACK(service_added), conn, NULL);

	dbus_g_proxy_add_signal(manager, "ServiceRemoved",
					G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(manager, "ServiceRemoved",
				G_CALLBACK(service_removed), conn, NULL);

	dbus_g_proxy_call(manager, "ListServices", &error,
			G_TYPE_INVALID, G_TYPE_STRV, &array, G_TYPE_INVALID);

	if (error == NULL) {
		while (*array) {
			add_service(*array);
			array++;
		}
	} else
		g_error_free(error);
}

void cleanup_service(void)
{
	g_list_foreach(service_list, service_free, NULL);

	if (manager != NULL)
		g_object_unref(manager);

	dbus_g_connection_unref(connection);
}

void disable_service(void)
{
	g_list_foreach(service_list, service_disable, NULL);
}
