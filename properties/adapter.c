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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>

#include <bluetooth-client.h>
#include <helper.h>

#include "adapter.h"
#include "general.h"

static BluetoothClient *client;
static GtkTreeModel *adapter_model;

struct adapter_data {
	DBusGProxy *proxy;
	GtkWidget *notebook;
	GtkWidget *child;
	GtkWidget *button_dummy;
	GtkWidget *button_hidden;
	GtkWidget *button_always;
	GtkWidget *button_timeout;
	GtkWidget *timeout_scale;
	GtkWidget *entry;
	GtkWidget *button_delete;
	GtkWidget *button_trusted;
	GtkWidget *button_disconnect;
	GtkTreeSelection *selection;
	GtkTreeRowReference *reference;
	guint signal_hidden;
	guint signal_always;
	guint signal_timeout;
	guint signal_scale;
	gboolean powered;
	gboolean discoverable;
	guint timeout_value;
	guint timeout_remain;
	int name_changed;
};

static void block_signals(struct adapter_data *adapter)
{
	g_signal_handler_block(adapter->button_hidden,
						adapter->signal_hidden);
	g_signal_handler_block(adapter->button_always,
						adapter->signal_always);
	g_signal_handler_block(adapter->button_timeout,
						adapter->signal_timeout);
	g_signal_handler_block(adapter->timeout_scale,
						adapter->signal_scale);
}

static void unblock_signals(struct adapter_data *adapter)
{
	g_signal_handler_unblock(adapter->button_hidden,
						adapter->signal_hidden);
	g_signal_handler_unblock(adapter->button_always,
						adapter->signal_always);
	g_signal_handler_unblock(adapter->button_timeout,
						adapter->signal_timeout);
	g_signal_handler_unblock(adapter->timeout_scale,
						adapter->signal_scale);
}

static void update_tab_label(GtkNotebook *notebook,
					GtkWidget *child, const char *name)
{
	GtkWidget *label;

	gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), child,
				name && name[0] != '\0' ? name : _("Adapter"));

	label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), child);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 20);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
}

static void mode_callback(GtkWidget *button, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GValue discoverable = { 0 };
	GValue timeout = { 0 };

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == FALSE)
		return;

	g_value_init(&discoverable, G_TYPE_BOOLEAN);
	g_value_init(&timeout, G_TYPE_UINT);

	if (button == adapter->button_hidden) {
		g_value_set_boolean(&discoverable, FALSE);
		g_value_set_uint(&timeout, 0);
	} else if (button == adapter->button_always) {
		g_value_set_boolean(&discoverable, TRUE);
		g_value_set_uint(&timeout, 0);
	} else if (button == adapter->button_timeout) {
		g_value_set_boolean(&discoverable, TRUE);
		if (adapter->timeout_value > 0)
			g_value_set_uint(&timeout, adapter->timeout_value);
		else
			g_value_set_uint(&timeout, 3 * 60);
	} else
		return;

	dbus_g_proxy_call(adapter->proxy, "SetProperty", NULL,
					G_TYPE_STRING, "Discoverable",
					G_TYPE_VALUE, &discoverable,
					G_TYPE_INVALID, G_TYPE_INVALID);

	dbus_g_proxy_call(adapter->proxy, "SetProperty", NULL,
					G_TYPE_STRING, "DiscoverableTimeout",
					G_TYPE_VALUE, &timeout,
					G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&discoverable);
	g_value_unset(&timeout);
}

static void scale_callback(GtkWidget *scale, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GValue timeout = { 0 };
	gdouble value;

	value = gtk_range_get_value(GTK_RANGE(scale));
	adapter->timeout_value = value * 60;

	g_value_init(&timeout, G_TYPE_UINT);
	g_value_set_uint(&timeout, adapter->timeout_value);

	dbus_g_proxy_call(adapter->proxy, "SetProperty", NULL,
					G_TYPE_STRING, "DiscoverableTimeout",
					G_TYPE_VALUE, &timeout,
					G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&timeout);
}

static gchar *format_callback(GtkWidget *scale,
				gdouble value, gpointer user_data)
{
	struct adapter_data *adapter = user_data;

	if (value == 0) {
		if (adapter->discoverable == TRUE)
			return g_strdup(_("always"));
		else
			return g_strdup(_("hidden"));
	}

	return g_strdup_printf(ngettext("%'g minute",
					"%'g minutes", value), value);
}

static void name_callback(GtkWidget *editable, gpointer user_data)
{
	struct adapter_data *adapter = user_data;

	adapter->name_changed = 1;
}

static gboolean focus_callback(GtkWidget *editable,
				GdkEventFocus *event, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GValue value = { 0 };
	const gchar *text;

	if (!adapter->name_changed)
		return FALSE;

	g_value_init(&value, G_TYPE_STRING);

	text = gtk_entry_get_text(GTK_ENTRY(editable));

	g_value_set_string(&value, text);

	dbus_g_proxy_call(adapter->proxy, "SetProperty", NULL,
			G_TYPE_STRING, "Name",
			G_TYPE_VALUE, &value, G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&value);

	adapter->name_changed = 0;

	return FALSE;
}

static void update_buttons(struct adapter_data *adapter, gboolean bonded,
					gboolean trusted, gboolean connected)
{
	if (trusted) {
		gtk_button_set_label(GTK_BUTTON(adapter->button_trusted), _("Distrust"));
	} else {
		gtk_button_set_label(GTK_BUTTON(adapter->button_trusted), _("Trust"));
	}
	gtk_widget_set_sensitive(adapter->button_delete, bonded);
	gtk_widget_set_sensitive(adapter->button_disconnect, connected);
}

static void select_callback(GtkTreeSelection *selection, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	DBusGProxy *proxy;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean selected;
	gboolean paired = FALSE, trusted = FALSE, connected = FALSE;

	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (selected == TRUE) {
		gtk_tree_model_get(model, &iter,
				BLUETOOTH_COLUMN_PROXY, &proxy,
				BLUETOOTH_COLUMN_PAIRED, &paired,
				BLUETOOTH_COLUMN_TRUSTED, &trusted,
				BLUETOOTH_COLUMN_CONNECTED, &connected, -1);

		if (proxy != NULL) {
			paired = TRUE;
			g_object_unref(proxy);
		}
	}

	update_buttons(adapter, paired, trusted, connected);

	gtk_widget_set_sensitive(adapter->button_trusted, selected);
	gtk_widget_set_sensitive(adapter->button_delete, selected);
}

static void row_callback(GtkTreeModel *model, GtkTreePath  *path,
					GtkTreeIter *iter, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	gboolean bonded = FALSE, trusted = FALSE, connected = FALSE;

	if (gtk_tree_selection_iter_is_selected(adapter->selection,
							iter) == FALSE)
		return;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PAIRED, &bonded,
					BLUETOOTH_COLUMN_TRUSTED, &trusted,
					BLUETOOTH_COLUMN_CONNECTED, &connected, -1);

	update_buttons(adapter, bonded, trusted, connected);
}

static void wizard_callback(GtkWidget *button, gpointer user_data)
{
	const char *command = "bluetooth-wizard";

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

static gboolean show_confirm_dialog(void)
{
	GtkWidget *dialog;
	gint response;
	gchar *text;

	text = g_strdup_printf("<big><b>%s</b></big>\n\n%s",
				_("Remove from list of known devices?"),
				_("If you delete the device, you have to "
					"set it up again before next use."));
	dialog = gtk_message_dialog_new_with_markup(NULL, GTK_DIALOG_MODAL,
				GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, text);
	g_free(text);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
							GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_DELETE,
							GTK_RESPONSE_ACCEPT);

	response = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;

	return FALSE;
}

static void delete_callback(GtkWidget *button, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	DBusGProxy *device;
	const char *path;

	if (gtk_tree_selection_get_selected(adapter->selection,
						&model, &iter) == FALSE)
		return;

	gtk_tree_model_get(model, &iter, BLUETOOTH_COLUMN_PROXY, &device, -1);

	if (device == NULL)
		return;

	path = dbus_g_proxy_get_path(device);

	if (show_confirm_dialog() == TRUE) {
		dbus_g_proxy_call(adapter->proxy, "RemoveDevice", NULL,
					DBUS_TYPE_G_OBJECT_PATH, path,
					G_TYPE_INVALID, G_TYPE_INVALID);
	}

	g_object_unref(device);
}

static void trusted_callback(GtkWidget *button, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GValue value = { 0 };
	GtkTreeModel *model;
	GtkTreeIter iter;
	DBusGProxy *device;
	gboolean trusted = FALSE;

	if (gtk_tree_selection_get_selected(adapter->selection,
						&model, &iter) == FALSE)
		return;

	gtk_tree_model_get(model, &iter, BLUETOOTH_COLUMN_PROXY, &device,
					BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if (device == NULL)
		return;

	g_value_init(&value, G_TYPE_BOOLEAN);

	g_value_set_boolean(&value, !trusted);

	dbus_g_proxy_call(device, "SetProperty", NULL,
			G_TYPE_STRING, "Trusted",
			G_TYPE_VALUE, &value, G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&value);

	g_object_unref(device);
}

static void disconnect_callback(GtkWidget *button, gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	DBusGProxy *device;

	if (gtk_tree_selection_get_selected(adapter->selection,
						&model, &iter) == FALSE)
		return;

	gtk_tree_model_get(model, &iter, BLUETOOTH_COLUMN_PROXY, &device, -1);

	if (device == NULL)
		return;

	dbus_g_proxy_call(device, "Disconnect", NULL,
					G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref(device);

	gtk_widget_set_sensitive(button, FALSE);
}

static gboolean device_filter(GtkTreeModel *model,
					GtkTreeIter *iter, gpointer user_data)
{
	DBusGProxy *proxy;
	gboolean active;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PROXY, &proxy,
					BLUETOOTH_COLUMN_PAIRED, &active, -1);

	if (proxy != NULL) {
		active = TRUE;
		g_object_unref(proxy);
	}

	return active;
}

static gboolean counter_callback(gpointer user_data)
{
	struct adapter_data *adapter = user_data;
	gdouble value;

	if (adapter->timeout_remain == 0)
		return FALSE;

	else if (adapter->timeout_remain < 60)
		value = 1;
	else if (adapter->timeout_remain > 60 * 30)
		value = 30;
	else
		value = adapter->timeout_remain / 60;

	adapter->timeout_remain--;

	gtk_range_set_fill_level(GTK_RANGE(adapter->timeout_scale), value);

	return TRUE;
}

static void create_adapter(struct adapter_data *adapter)
{
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *name;
	gboolean powered, discoverable;
	guint timeout;

	GtkWidget *mainbox;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *scale;
	GtkWidget *entry;
	GtkWidget *buttonbox;
	GtkWidget *scrolled;
	GtkWidget *tree;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GSList *group = NULL;
	gdouble scale_value;

	dbus_g_proxy_call(adapter->proxy, "GetProperties", NULL, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable",
						G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		name = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Powered");
		powered = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Discoverable");
		discoverable = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "DiscoverableTimeout");
		timeout = value ? g_value_get_uint(value) : 0;
	} else {
		address = NULL;
		name = NULL;
		powered = FALSE;
		discoverable = FALSE;
		timeout = 0;
	}

	adapter->powered = powered;
	adapter->discoverable = discoverable;
	adapter->timeout_value = timeout;

	mainbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 12);

	gtk_notebook_prepend_page(GTK_NOTEBOOK(adapter->notebook),
							mainbox, NULL);

	update_tab_label(GTK_NOTEBOOK(adapter->notebook), mainbox, name);

	adapter->child = mainbox;

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, FALSE, 0);

	label = create_label(_("Visibility setting"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(group, NULL);
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	gtk_widget_set_no_show_all(button, TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	adapter->button_dummy = button;

	button = gtk_radio_button_new_with_label(group,
					_("Hidden"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	if (powered == TRUE && discoverable == FALSE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	adapter->button_hidden = button;
	adapter->signal_hidden = g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(mode_callback), adapter);

	button = gtk_radio_button_new_with_label(group,
					_("Always visible"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	if (discoverable == TRUE && timeout == 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	adapter->button_always = button;
	adapter->signal_always = g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(mode_callback), adapter);

	button = gtk_radio_button_new_with_label(group,
					_("Temporary visible"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	if (discoverable == TRUE && timeout > 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	adapter->button_timeout = button;
	adapter->signal_timeout = g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(mode_callback), adapter);

	scale = gtk_hscale_new_with_range(0, 30, 1);
	gtk_scale_set_digits(GTK_SCALE(scale), 0);
	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_BOTTOM);
	gtk_range_set_restrict_to_fill_level(GTK_RANGE(scale), FALSE);
	gtk_range_set_show_fill_level(GTK_RANGE(scale), TRUE);

	if (discoverable == TRUE) {
		if (timeout == 0)
			scale_value = 0;
		else if (timeout < 60)
			scale_value = 1;
		else if (timeout > 60 * 30)
			scale_value = 30;
		else
			scale_value = timeout / 60;
	} else
		scale_value = 0.0;

	gtk_range_set_value(GTK_RANGE(scale), scale_value);
	gtk_range_set_fill_level(GTK_RANGE(scale), scale_value);
	gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
	gtk_box_pack_start(GTK_BOX(vbox), scale, FALSE, FALSE, 0);

	if (discoverable == TRUE && timeout > 0) {
		adapter->timeout_remain = timeout;
		g_timeout_add_seconds(1, counter_callback, adapter);
	}

	adapter->timeout_scale = scale;
	adapter->signal_scale = g_signal_connect(G_OBJECT(scale), "value-changed",
					G_CALLBACK(scale_callback), adapter);

	g_signal_connect(G_OBJECT(scale), "format-value",
					G_CALLBACK(format_callback), adapter);

	if (discoverable == FALSE || timeout == 0)
		gtk_widget_set_sensitive(GTK_WIDGET(scale), FALSE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(mainbox), vbox, TRUE, TRUE, 0);

	label = create_label(_("Friendly name"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
	gtk_widget_set_size_request(entry, 240, -1);
	gtk_box_pack_start(GTK_BOX(vbox), entry, TRUE, TRUE, 0);

	if (name != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), name);

	adapter->entry = entry;

	g_signal_connect(G_OBJECT(entry), "changed",
					G_CALLBACK(name_callback), adapter);
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
					G_CALLBACK(focus_callback), adapter);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(mainbox), table, TRUE, TRUE, 0);

	label = create_label(_("Known devices"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
							GTK_SHADOW_OUT);
	gtk_table_attach(GTK_TABLE(table), scrolled, 0, 1, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 6);

	model = bluetooth_client_get_device_filter_model(client,
				adapter->proxy, device_filter, NULL, NULL);
	g_signal_connect(G_OBJECT(model), "row-changed",
				G_CALLBACK(row_callback), adapter);
	tree = create_tree(model, FALSE);
	g_object_unref(model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(select_callback), adapter);

	adapter->selection = selection;

	gtk_container_add(GTK_CONTAINER(scrolled), tree);

	buttonbox = gtk_vbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonbox),
						GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(buttonbox), 6);
	gtk_box_set_homogeneous(GTK_BOX(buttonbox), FALSE);
	gtk_table_attach(GTK_TABLE(table), buttonbox, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 6, 6);

	button = gtk_button_new_with_label(_("Setup new device..."));
	image = gtk_image_new_from_stock(GTK_STOCK_ADD,
						GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_box_pack_start(GTK_BOX(buttonbox), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(wizard_callback), adapter);

	button = gtk_button_new_with_label(_("Disconnect"));
	image = gtk_image_new_from_stock(GTK_STOCK_DISCONNECT,
						GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_box_pack_end(GTK_BOX(buttonbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(buttonbox),
								button, TRUE);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(disconnect_callback), adapter);

	adapter->button_disconnect = button;

	button = gtk_button_new_with_label(_("Trust"));
	image = gtk_image_new_from_stock(GTK_STOCK_ABOUT,
						GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_box_pack_end(GTK_BOX(buttonbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(buttonbox),
								button, TRUE);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(trusted_callback), adapter);

	adapter->button_trusted = button;

	button = gtk_button_new_with_label(_("Delete"));
	image = gtk_image_new_from_stock(GTK_STOCK_DELETE,
						GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_box_pack_end(GTK_BOX(buttonbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(buttonbox),
								button, TRUE);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(delete_callback), adapter);

	adapter->button_delete = button;

	g_object_set_data(G_OBJECT(mainbox), "adapter", adapter);

	gtk_widget_show_all(mainbox);
}

static void update_visibility(struct adapter_data *adapter)
{
	GtkWidget *scale = adapter->timeout_scale;
	GtkWidget *button;
	gboolean sensitive;
	gdouble value;

	if (adapter->discoverable == TRUE) {
		if (adapter->timeout_value == 0)
			value = 0;
		else if (adapter->timeout_value < 60)
			value = 1;
		else if (adapter->timeout_value > 60 * 30)
			value = 30;
		else
			value = adapter->timeout_value / 60;

		if (adapter->timeout_value > 0) {
			button = adapter->button_timeout;
			sensitive = TRUE;
		} else {
			button = adapter->button_always;
			sensitive = FALSE;
		}

		if (adapter->timeout_remain == 0)
			g_timeout_add_seconds(1, counter_callback, adapter);

		adapter->timeout_remain = adapter->timeout_value;
	} else {
		value = 0.0;

		if (adapter->powered == FALSE)
			button = adapter->button_dummy;
		else
			button = adapter->button_hidden;

		sensitive = FALSE;
		adapter->timeout_remain = 0;
	}

	block_signals(adapter);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	gtk_range_set_value(GTK_RANGE(scale), value);
	gtk_range_set_fill_level(GTK_RANGE(scale), value);
	if (sensitive == FALSE) {
		gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
		gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(scale), sensitive);
	unblock_signals(adapter);
}

static void property_changed(DBusGProxy *proxy, const char *property,
					GValue *value, gpointer user_data)
{
	struct adapter_data *adapter = user_data;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_value_get_string(value);

		update_tab_label(GTK_NOTEBOOK(adapter->notebook),
							adapter->child, name);

		gtk_entry_set_text(GTK_ENTRY(adapter->entry),
							name ? name : "");

		adapter->name_changed = 0;
	} else if (g_str_equal(property, "Powered") == TRUE) {
		gboolean powered = g_value_get_boolean(value);

		adapter->powered = powered;

		if (powered == FALSE)
			adapter->discoverable = FALSE;

		update_visibility(adapter);
	} else if (g_str_equal(property, "Discoverable") == TRUE) {
		gboolean discoverable = g_value_get_boolean(value);

		adapter->powered = TRUE;
		adapter->discoverable = discoverable;

		update_visibility(adapter);
	} else if (g_str_equal(property, "DiscoverableTimeout") == TRUE) {
		guint timeout = g_value_get_uint(value);

		adapter->timeout_value = timeout;

		if (adapter->timeout_remain == 0)
			g_timeout_add_seconds(1, counter_callback, adapter);

		adapter->timeout_remain = timeout;

		update_visibility(adapter);
	}
}

static struct adapter_data *adapter_alloc(GtkTreeModel *model,
		GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	DBusGProxy *proxy;
	struct adapter_data *adapter;

	adapter = g_try_malloc0(sizeof(*adapter));
	if (adapter == NULL)
		return NULL;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PROXY, &proxy, -1);

	if (proxy == NULL) {
		g_free(adapter);
		return NULL;
	}

	adapter->notebook = user_data;

	adapter->reference = gtk_tree_row_reference_new(model, path);
	adapter->proxy = proxy;

	return adapter;
}

static gboolean adapter_create(gpointer user_data)
{
	struct adapter_data *adapter = user_data;

	dbus_g_proxy_connect_signal(adapter->proxy, "PropertyChanged",
				G_CALLBACK(property_changed), adapter, NULL);

	create_adapter(adapter);

	return FALSE;
}

static gboolean adapter_insert(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	struct adapter_data *adapter;

	adapter = adapter_alloc(model, path, iter, user_data);
	if (adapter == NULL)
		return FALSE;

	adapter_create(adapter);

	return FALSE;
}

static void adapter_added(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	struct adapter_data *adapter;

	adapter = adapter_alloc(model, path, iter, user_data);
	if (adapter == NULL)
		return;

	g_timeout_add(250, adapter_create, adapter);
}

static void adapter_removed(GtkTreeModel *model, GtkTreePath *path,
							gpointer user_data)
{
	GtkNotebook *notebook = user_data;
	int i, count = gtk_notebook_get_n_pages(notebook);

	for (i = 0; i < count; i++) {
		GtkWidget *widget;
		struct adapter_data *adapter;

		widget = gtk_notebook_get_nth_page(notebook, i);
		if (widget == NULL)
			continue;

		adapter = g_object_get_data(G_OBJECT(widget), "adapter");
		if (adapter == NULL)
			continue;

		if (gtk_tree_row_reference_valid(adapter->reference) == TRUE)
			continue;

		gtk_tree_row_reference_free(adapter->reference);
		adapter->reference = NULL;

		gtk_notebook_remove_page(notebook, i);

		g_signal_handlers_disconnect_by_func(adapter->proxy,
						property_changed, adapter);

		g_object_unref(adapter->proxy);
		g_free(adapter);
	}
}

void setup_adapter(GtkNotebook *notebook)
{
	client = bluetooth_client_new();

	adapter_model = bluetooth_client_get_adapter_model(client);

	g_signal_connect_after(G_OBJECT(adapter_model), "row-inserted",
					G_CALLBACK(adapter_added), notebook);

	g_signal_connect_after(G_OBJECT(adapter_model), "row-deleted",
					G_CALLBACK(adapter_removed), notebook);

	gtk_tree_model_foreach(adapter_model, adapter_insert, notebook);
}

void cleanup_adapter(void)
{
	g_object_unref(adapter_model);

	g_object_unref(client);
}
