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

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>

#include <bluetooth-client.h>
#include <bluetooth-agent.h>

#include "notify.h"
#include "agent.h"

static BluetoothClient *client;
static GtkTreeModel *adapter_model;

typedef enum {
	AGENT_ERROR_REJECT
} AgentError;

#define AGENT_ERROR (agent_error_quark())

#define AGENT_ERROR_TYPE (agent_error_get_type()) 

static GQuark agent_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GType agent_error_get_type(void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY(AGENT_ERROR_REJECT, "Rejected"),
			{ 0, 0, 0 }
		};

		etype = g_enum_register_static("agent", values);
	}

	return etype;
}

static GList *input_list = NULL;

struct input_data {
	char *path;
	char *uuid;
	gboolean numeric;
	DBusGProxy *device;
	DBusGMethodInvocation *context;
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *entry;
};

static gint input_compare(gconstpointer a, gconstpointer b)
{
	struct input_data *a_data = (struct input_data *) a;
	struct input_data *b_data = (struct input_data *) b;

	return g_ascii_strcasecmp(a_data->path, b_data->path);
}

static void input_free(struct input_data *input)
{
	gtk_widget_destroy(input->dialog);

	input_list = g_list_remove(input_list, input);

	if (input->device != NULL)
		g_object_unref(input->device);

	g_free(input->uuid);
	g_free(input->path);
	g_free(input);

	if (g_list_length(input_list) == 0)
		disable_blinking();
}

static void passkey_callback(GtkWidget *dialog,
				gint response, gpointer user_data)
{
	struct input_data *input = user_data;

	if (response == GTK_RESPONSE_ACCEPT) {
		const char *text;
		text = gtk_entry_get_text(GTK_ENTRY(input->entry));

		if (input->numeric == TRUE) {
			guint passkey = atoi(text);
			dbus_g_method_return(input->context, passkey);
		} else
			dbus_g_method_return(input->context, text);
	} else {
		GError *error;
		error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
						"Pairing request rejected");
		dbus_g_method_return_error(input->context, error);
	}

	input_free(input);
}

static void confirm_callback(GtkWidget *dialog,
				gint response, gpointer user_data)
{
	struct input_data *input = user_data;

	if (response != GTK_RESPONSE_YES) {
		GError *error;
		error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
					"Confirmation request rejected");
		dbus_g_method_return_error(input->context, error);
	} else
		dbus_g_method_return(input->context);

	input_free(input);
}

static void set_trusted(struct input_data *input)
{
	GValue value = { 0 };
	gboolean active;

	if (input->device == NULL)
		return;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(input->button));
	if (active == FALSE)
		return;

	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, TRUE);

	dbus_g_proxy_call(input->device, "SetProperty", NULL,
			G_TYPE_STRING, "Trusted",
			G_TYPE_VALUE, &value, G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&value);
}

static void auth_callback(GtkWidget *dialog,
				gint response, gpointer user_data)
{
	struct input_data *input = user_data;

	if (response == GTK_RESPONSE_YES) {
		set_trusted(input);
		dbus_g_method_return(input->context);
	} else {
		GError *error;
		error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
					"Authorization request rejected");
		dbus_g_method_return_error(input->context, error);
	}

	input_free(input);
}

static void insert_callback(GtkEditable *editable, const gchar *text,
			gint length, gint *position, gpointer user_data)
{
	struct input_data *input = user_data;
	gint i;

	if (input->numeric == FALSE)
		return;

	for (i = 0; i < length; i++) {
		if (g_ascii_isdigit(text[i]) == FALSE) {
			g_signal_stop_emission_by_name(editable,
							"insert-text");
		}
	}
}

static void changed_callback(GtkWidget *editable, gpointer user_data)
{
	struct input_data *input = user_data;
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(input->entry));

	gtk_widget_set_sensitive(input->button, *text != '\0' ? TRUE : FALSE);
}

static void toggled_callback(GtkWidget *button, gpointer user_data)
{
	struct input_data *input = user_data;
	gboolean mode;

	mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

	gtk_entry_set_visibility(GTK_ENTRY(input->entry), mode);
}

static void passkey_dialog(DBusGProxy *adapter, DBusGProxy *device,
		const char *address, const char *name, gboolean numeric,
						DBusGMethodInvocation *context)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *table;
	GtkWidget *vbox;
	struct input_data *input;
	gchar *markup;

	input = g_try_malloc0(sizeof(*input));
	if (!input)
		return;

	input->path = g_strdup(dbus_g_proxy_get_path(adapter));
	input->numeric = numeric;
	input->context = context;
	input->device = g_object_ref(device);

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Authentication request"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_urgency_hint(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	input->dialog = dialog;

	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	gtk_widget_grab_default(button);
	gtk_widget_set_sensitive(button, FALSE);
	input->button = button;

	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 20);
	gtk_container_set_border_width(GTK_CONTAINER(table), 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
	image = gtk_image_new_from_icon_name(GTK_STOCK_DIALOG_AUTHENTICATION,
							GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, 0, 5,
						GTK_SHRINK, GTK_FILL, 0, 0);
	vbox = gtk_vbox_new(FALSE, 6);

	label = gtk_label_new(_("Pairing request for device:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<b>%s</b>", name);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_widget_set_size_request(GTK_WIDGET(label), 280, -1);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	vbox = gtk_vbox_new(FALSE, 6);

	label = gtk_label_new(_("Enter passkey for authentication:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 2, 3,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	entry = gtk_entry_new();
	if (numeric == TRUE) {
		gtk_entry_set_max_length(GTK_ENTRY(entry), 6);
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 6);
		g_signal_connect(G_OBJECT(entry), "insert-text",
					G_CALLBACK(insert_callback), input);
	} else {
		gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
		gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	input->entry = entry;
	g_signal_connect(G_OBJECT(entry), "changed",
				G_CALLBACK(changed_callback), input);
	gtk_container_add(GTK_CONTAINER(vbox), entry);

	if (numeric == FALSE) {
		button = gtk_check_button_new_with_label(_("Show input"));
		g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(toggled_callback), input);
		gtk_container_add(GTK_CONTAINER(vbox), button);
	}

	input_list = g_list_append(input_list, input);

	g_signal_connect(G_OBJECT(dialog), "response",
				G_CALLBACK(passkey_callback), input);

	enable_blinking();
}

static void display_dialog(DBusGProxy *adapter, DBusGProxy *device,
		const char *address, const char *name, const char *value,
				guint entered, DBusGMethodInvocation *context)
{
}

static void confirm_dialog(DBusGProxy *adapter, DBusGProxy *device,
		const char *address, const char *name, const char *value,
						DBusGMethodInvocation *context)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *vbox;
	gchar *markup;
	struct input_data *input;

	input = g_try_malloc0(sizeof(*input));
	if (!input)
		return;

	input->path = g_strdup(dbus_g_proxy_get_path(adapter));
	input->device = g_object_ref(device);
	input->context = context;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Confirmation request"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_urgency_hint(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	input->dialog = dialog;

	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
					GTK_STOCK_NO, GTK_RESPONSE_NO);
	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
					GTK_STOCK_YES, GTK_RESPONSE_YES);

	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 20);
	gtk_container_set_border_width(GTK_CONTAINER(table), 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

	image = gtk_image_new_from_icon_name(GTK_STOCK_DIALOG_AUTHENTICATION,
							GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, 0, 5,
						GTK_SHRINK, GTK_FILL, 0, 0);

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new(_("Pairing request for device:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<b>%s</b>", name);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_widget_set_size_request(GTK_WIDGET(label), 280, -1);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new(_("Confirm value for authentication:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 2, 3,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<b>%s</b>\n", value);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	input_list = g_list_append(input_list, input);

	g_signal_connect(G_OBJECT(dialog), "response",
				G_CALLBACK(confirm_callback), input);

	enable_blinking();
}

static void auth_dialog(DBusGProxy *adapter, DBusGProxy *device,
		const char *address, const char *name, const char *uuid,
						DBusGMethodInvocation *context)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *vbox;
	gchar *markup, *text;
	struct input_data *input;

	input = g_try_malloc0(sizeof(*input));
	if (!input)
		return;

	input->path = g_strdup(dbus_g_proxy_get_path(adapter));
	input->uuid = g_strdup(uuid);
	input->device = g_object_ref(device);
	input->context = context;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Authorization request"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_urgency_hint(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	input->dialog = dialog;

	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
					GTK_STOCK_NO, GTK_RESPONSE_NO);
	button = gtk_dialog_add_button(GTK_DIALOG(dialog),
					GTK_STOCK_YES, GTK_RESPONSE_YES);

	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 20);
	gtk_container_set_border_width(GTK_CONTAINER(table), 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

	image = gtk_image_new_from_icon_name(GTK_STOCK_DIALOG_AUTHENTICATION,
							GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, 0, 5,
						GTK_SHRINK, GTK_FILL, 0, 0);

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new(_("Authorization request for device:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<b>%s</b>", name);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_widget_set_size_request(GTK_WIDGET(label), 280, -1);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 2, 3,
				GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	label = gtk_label_new(NULL);
	/* translators: Whether to grant access to a particular service
	 * to the device mentioned */
	markup = g_strdup_printf("<i>%s</i>", uuid);
	text = g_strdup_printf(_("Grant access to %s?"), markup);
	g_free(markup);
	markup = g_strdup_printf("%s\n", text);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	g_free(text);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	button = gtk_check_button_new_with_label(_("Always grant access"));
	input->button = button;
	gtk_container_add(GTK_CONTAINER(vbox), button);

	input_list = g_list_append(input_list, input);

	g_signal_connect(G_OBJECT(dialog), "response",
				G_CALLBACK(auth_callback), input);

	enable_blinking();
}

static void show_dialog(gpointer data, gpointer user_data)
{
	struct input_data *input = data;

	gtk_widget_show_all(input->dialog);

	gtk_window_present(GTK_WINDOW(input->dialog));
}

static void notification_closed(GObject *object, gpointer user_data)
{
	g_list_foreach(input_list, show_dialog, NULL);

	disable_blinking();
}

#ifndef DBUS_TYPE_G_DICTIONARY
#define DBUS_TYPE_G_DICTIONARY \
	(dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#endif

static gboolean device_get_properties(DBusGProxy *proxy,
					GHashTable **hash, GError **error)
{
	return dbus_g_proxy_call(proxy, "GetProperties", error,
		G_TYPE_INVALID, DBUS_TYPE_G_DICTIONARY, hash, G_TYPE_INVALID);
}

static gboolean pincode_request(DBusGMethodInvocation *context,
					DBusGProxy *device, gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *alias;
	gchar *name, *line;

	device_get_properties(device, &hash, NULL);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		alias = value ? g_value_get_string(value) : NULL;
	} else {
		address = NULL;
		alias = NULL;
	}

	if (alias) {
		if (g_strrstr(alias, address))
			name = g_strdup(alias);
		else
			name = g_strdup_printf("%s (%s)", alias, address);
	} else
		name = g_strdup(address);

	passkey_dialog(adapter, device, address, name, FALSE, context);

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Pairing request for %s"), name);

	g_free(name);

	show_notification(_("Bluetooth device"),
					line, _("Enter passkey"), 0,
					G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean passkey_request(DBusGMethodInvocation *context,
					DBusGProxy *device, gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *alias;
	gchar *name, *line;

	device_get_properties(device, &hash, NULL);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		alias = value ? g_value_get_string(value) : NULL;
	} else {
		address = NULL;
		alias = NULL;
	}

	if (alias) {
		if (g_strrstr(alias, address))
			name = g_strdup(alias);
		else
			name = g_strdup_printf("%s (%s)", alias, address);
	} else
		name = g_strdup(address);

	passkey_dialog(adapter, device, address, name, TRUE, context);

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Pairing request for %s"), name);

	g_free(name);

	show_notification(_("Bluetooth device"),
					line, _("Enter passkey"), 0,
					G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean display_request(DBusGMethodInvocation *context,
				DBusGProxy *device, guint passkey,
					guint entered, gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *alias;
	gchar *name, *line, *text;

	device_get_properties(device, &hash, NULL);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		alias = value ? g_value_get_string(value) : NULL;
	} else {
		address = NULL;
		alias = NULL;
	}

	if (alias) {
		if (g_strrstr(alias, address))
			name = g_strdup(alias);
		else
			name = g_strdup_printf("%s (%s)", alias, address);
	} else
		name = g_strdup(address);

	text = g_strdup_printf("%d", passkey);
	display_dialog(adapter, device, address, name, text, entered, context);
	g_free(text);

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Pairing request for %s"), name);

	g_free(name);

	show_notification(_("Bluetooth device"),
					line, _("Enter passkey"), 0,
					G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean confirm_request(DBusGMethodInvocation *context,
			DBusGProxy *device, guint passkey, gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *alias;
	gchar *name, *line, *text;

	device_get_properties(device, &hash, NULL);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		alias = value ? g_value_get_string(value) : NULL;
	} else {
		address = NULL;
		alias = NULL;
	}

	if (alias) {
		if (g_strrstr(alias, address))
			name = g_strdup(alias);
		else
			name = g_strdup_printf("%s (%s)", alias, address);
	} else
		name = g_strdup(address);

	text = g_strdup_printf("%d", passkey);
	confirm_dialog(adapter, device, address, name, text, context);
	g_free(text);

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Confirmation request for %s"), name);

	g_free(name);

	show_notification(_("Bluetooth device"),
					line, _("Confirm passkey"), 0,
					G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean authorize_request(DBusGMethodInvocation *context,
		DBusGProxy *device, const char *uuid, gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *alias;
	gchar *name, *line;

	device_get_properties(device, &hash, NULL);

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		alias = value ? g_value_get_string(value) : NULL;
	} else {
		address = NULL;
		alias = NULL;
	}

	if (alias) {
		if (g_strrstr(alias, address))
			name = g_strdup(alias);
		else
			name = g_strdup_printf("%s (%s)", alias, address);
	} else
		name = g_strdup(address);

	auth_dialog(adapter, device, address, name, uuid, context);

	line = g_strdup_printf(_("Authorization request for %s"), name);

	g_free(name);

	show_notification(_("Bluetooth device"),
					line, _("Check authorization"), 0,
					G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean cancel_request(DBusGMethodInvocation *context,
							gpointer user_data)
{
	DBusGProxy *adapter = user_data;
	GList *list;
	GError *result;
	struct input_data *input;

	input = g_try_malloc0(sizeof(*input));
	if (!input)
		return FALSE;

	input->path = g_strdup(dbus_g_proxy_get_path(adapter));

	list = g_list_find_custom(input_list, input, input_compare);

	g_free(input->path);
	g_free(input);

	if (!list || !list->data)
		return FALSE;

	input = list->data;

	close_notification();

	result = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
						"Agent callback canceled");

	dbus_g_method_return_error(input->context, result);

	input_free(input);

	return TRUE;
}

static gboolean adapter_insert(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	BluetoothAgent *agent;
	DBusGProxy *adapter;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PROXY, &adapter, -1);

	if (adapter == NULL)
		return FALSE;

	agent = bluetooth_agent_new();

	bluetooth_agent_set_pincode_func(agent, pincode_request, adapter);
	bluetooth_agent_set_passkey_func(agent, passkey_request, adapter);
	bluetooth_agent_set_display_func(agent, display_request, adapter);
	bluetooth_agent_set_confirm_func(agent, confirm_request, adapter);
	bluetooth_agent_set_authorize_func(agent, authorize_request, adapter);
	bluetooth_agent_set_cancel_func(agent, cancel_request, adapter);

	bluetooth_agent_register(agent, adapter);

	return FALSE;
}

static void adapter_added(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	adapter_insert(model, path, iter, user_data);
}

int setup_agents(void)
{
	dbus_g_error_domain_register(AGENT_ERROR, "org.bluez.Error",
							AGENT_ERROR_TYPE);

	client = bluetooth_client_new();

	adapter_model = bluetooth_client_get_adapter_model(client);

	g_signal_connect(G_OBJECT(adapter_model), "row-inserted",
					G_CALLBACK(adapter_added), NULL);

	gtk_tree_model_foreach(adapter_model, adapter_insert, NULL);

	return 0;
}

void cleanup_agents(void)
{
	g_object_unref(adapter_model);

	g_object_unref(client);
}

void show_agents(void)
{
	close_notification();

	g_list_foreach(input_list, show_dialog, NULL);

	disable_blinking();
}
