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

#include <sys/time.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <obex-agent.h>

#include "helper.h"
#include "marshal.h"

#define AGENT_PATH "/org/bluez/agent/sendto"

static DBusGConnection *conn = NULL;

static GtkWidget *dialog;
static GtkWidget *label_from;
static GtkWidget *label_status;
static GtkWidget *progress;

static gchar *option_device = NULL;
static gchar **option_files = NULL;

static gchar *device_name = NULL;

static guint64 current_size = 0;
static guint64 total_size = 0;
static guint64 total_sent = 0;

static int file_count = 0;
static int file_index = 0;

static gint64 first_update = 0;
static gint64 last_update = 0;

static gchar *filename_to_path(const gchar *filename)
{
	GFile *file;
	gchar *path;

	file = g_file_new_for_commandline_arg(filename);
	path = g_file_get_path(file);
	g_object_unref(file);

	return path;
}

static gint64 get_system_time(void)
{
	struct timeval tmp;

	gettimeofday(&tmp, NULL);

	return (gint64) tmp.tv_usec +
			(gint64) tmp.tv_sec * G_GINT64_CONSTANT(1000000);
}

static gchar *format_time(gint seconds)
{
	gint hours, minutes;

	if (seconds < 0)
		seconds = 0;

	if (seconds < 60)
		return g_strdup_printf(ngettext("%'d second",
					"%'d seconds", seconds), seconds);

	if (seconds < 60 * 60) {
		minutes = (seconds + 30) / 60;
		return g_strdup_printf(ngettext("%'d minute",
					"%'d minutes", minutes), minutes);
	}

	hours = seconds / (60 * 60);

	if (seconds < 60 * 60 * 4) {
		gchar *res, *h, *m;

		minutes = (seconds - hours * 60 * 60 + 30) / 60;

		h = g_strdup_printf(ngettext("%'d hour",
					"%'d hours", hours), hours);
		m = g_strdup_printf(ngettext("%'d minute",
					"%'d minutes", minutes), minutes);
		res = g_strconcat(h, ", ", m, NULL);
		g_free(h);
		g_free(m);
		return res;
	}

	return g_strdup_printf(ngettext("approximately %'d hour",
				"approximately %'d hours", hours), hours);
}

static void response_callback(GtkWidget *dialog,
					gint response, gpointer user_data)
{
	gtk_widget_destroy(dialog);

	gtk_main_quit();
}

static gboolean is_palm_device(const gchar *bdaddr)
{
	return (g_str_has_prefix(bdaddr, "00:04:6B") ||
			g_str_has_prefix(bdaddr, "00:07:E0") ||
				g_str_has_prefix(bdaddr, "00:0E:20"));
}

static void create_window(void)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	gchar *text;

	dialog = gtk_dialog_new_with_buttons(_("File Transfer"), NULL,
				GTK_DIALOG_NO_SEPARATOR,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_window_set_type_hint(GTK_WINDOW(dialog),
						GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	text = g_markup_printf_escaped("<span size=\"larger\"><b>%s</b></span>",
					_("Sending files via Bluetooth"));
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_box_pack_start_defaults(GTK_BOX(vbox), label);

	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 9);

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	text = g_markup_printf_escaped("<b>%s</b>", _("From:"));
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
							GTK_FILL, 0, 0, 0);

	label_from = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label_from), 0, 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(label_from), PANGO_ELLIPSIZE_MIDDLE);
	text = g_get_current_dir();
	gtk_label_set_markup(GTK_LABEL(label_from), text);
	g_free(text);
	gtk_table_attach_defaults(GTK_TABLE(table), label_from, 1, 2, 0, 1);

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	text = g_markup_printf_escaped("<b>%s</b>", _("To:"));
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
							GTK_FILL, 0, 0, 0);

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_text(GTK_LABEL(label), device_name);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 1, 2);

	progress = gtk_progress_bar_new();
	gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(progress),
							PANGO_ELLIPSIZE_END);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress),
							_("Connecting..."));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), progress);

	label_status = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label_status), 0, 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(label_status), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start(GTK_BOX(vbox), label_status, TRUE, TRUE, 2);

	g_signal_connect(G_OBJECT(dialog), "response",
				G_CALLBACK(response_callback), NULL);

	gtk_widget_show_all(dialog);
}

static void finish_sending(DBusGProxy *proxy)
{
	gtk_label_set_markup(GTK_LABEL(label_status), NULL);

	dbus_g_proxy_call(proxy, "Disconnect", NULL, G_TYPE_INVALID,
							G_TYPE_INVALID);

	gtk_widget_destroy(dialog);

	gtk_main_quit();
}

static void send_file(DBusGProxy *proxy)
{
	gchar *filename = option_files[file_index];
	GError *error = NULL;
	gchar *basename, *text, *markup;

	if (file_index > file_count - 1) {
		finish_sending(proxy);
		return;
	}

	if (filename[0] == '\0') {
		file_index++;
		send_file(proxy);
		return;
	}

	text = g_path_get_dirname(filename);
	gtk_label_set_markup(GTK_LABEL(label_from), text);
	g_free(text);

	basename = g_path_get_basename(filename);
	text = g_strdup_printf(_("Sending %s"), basename);
	g_free(basename);
	markup = g_markup_printf_escaped("<i>%s</i>", text);
	gtk_label_set_markup(GTK_LABEL(label_status), markup);
	g_free(markup);
	g_free(text);

	text = g_strdup_printf(_("Sending file %d of %d"),
						file_index + 1, file_count);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), text);
	g_free(text);

	dbus_g_proxy_call(proxy, "SendFile", &error,
				G_TYPE_STRING, filename, G_TYPE_INVALID,
							G_TYPE_INVALID);

	if (error != NULL) {
		g_printerr("Sending of file %s failed: %s\n", filename,
							error->message);
		g_error_free(error);
		gtk_main_quit();
	}
}

static void transfer_started(DBusGProxy *proxy, gchar *a, gchar *b,
					guint64 size, gpointer user_data)
{
	current_size = size;

	last_update = get_system_time();
}

static void transfer_progress(DBusGProxy *proxy,
					guint64 bytes, gpointer user_data)
{
	gint64 current_time;
	gint elapsed_time;
	gint remaining_time;
	gint transfer_rate;
	guint64 current_sent;
	gdouble fraction;
	gchar *time, *rate, *file, *text;

	current_sent = total_sent + bytes;
	fraction = (gdouble) current_sent / (gdouble) total_size;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);

	current_time = get_system_time();
	elapsed_time = (current_time - first_update) / 1000000;

	if (current_time < last_update + 1000000)
		return;

	last_update = current_time;

	if (elapsed_time == 0)
		return;

	transfer_rate = current_sent / elapsed_time;

	if (transfer_rate == 0)
		return;

	remaining_time = (total_size - current_sent) / transfer_rate;

	time = format_time(remaining_time);

	if (transfer_rate >= 3000)
		rate = g_strdup_printf(_("%d KB/s"), transfer_rate / 1000);
	else
		rate = g_strdup_printf(_("%d B/s"), transfer_rate);

	file = g_strdup_printf(_("Sending file %d of %d"),
						file_index + 1, file_count);
	text = g_strdup_printf("%s (%s, %s)", file, rate, time);
	g_free(file);
	g_free(rate);
	g_free(time);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), text);
	g_free(text);
}

static void transfer_completed(DBusGProxy *proxy, gpointer user_data)
{
	total_sent += current_size;

	file_index++;

	send_file(proxy);
}

static void transfer_cancelled(DBusGProxy *proxy, gpointer user_data)
{
	transfer_completed(proxy, user_data);
}

static void error_occurred(DBusGProxy *proxy, const gchar *name,
				const gchar *message, gpointer user_data)
{
	gchar *text;

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress),
						_("Error Occurred"));

	text = g_strdup_printf("<span foreground=\"red\">%s</span>", message);
	gtk_label_set_markup(GTK_LABEL(label_status), text);
	g_free(text);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
						GTK_RESPONSE_CLOSE, TRUE);
}

static void session_connected(DBusGProxy *proxy, gpointer user_data)
{
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), NULL);

	first_update = get_system_time();

	send_file(proxy);
}

#define OPENOBEX_CONNECTION_FAILED "org.openobex.Error.ConnectionAttemptFailed"

static gchar *get_error_message(GError *error)
{
	char *message;
	const gchar *name;

	if (error == NULL)
		return g_strdup(_("An unknown error occured"));

	if (error->code != DBUS_GERROR_REMOTE_EXCEPTION) {
		message = g_strdup(error->message);
		goto done;
	}

	name = dbus_g_error_get_name(error);
	if (g_str_equal(name, OPENOBEX_CONNECTION_FAILED) == TRUE &&
					is_palm_device(option_device)) {
		message = g_strdup(_("Make sure that remote device "
					"is switched on and that it "
					"accepts Bluetooth connections"));
		goto done;
	}

	message = g_strdup(error->message);

done:
	g_error_free(error);

	return message;
}

static void create_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError *error = NULL;
	const gchar *path = NULL;

	if (dbus_g_proxy_end_call(proxy, call, &error,
					DBUS_TYPE_G_OBJECT_PATH, &path,
						G_TYPE_INVALID) == FALSE) {
		gchar *message;

		message = get_error_message(error);
		error_occurred(proxy, NULL, message, NULL);
		g_free(message);

		return;
	}

	proxy = dbus_g_proxy_new_for_name(conn, "org.openobex",
						path, "org.openobex.Session");

	dbus_g_proxy_add_signal(proxy, "Connected", G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "Connected",
				G_CALLBACK(session_connected), NULL, NULL);

	dbus_g_proxy_add_signal(proxy, "ErrorOccurred",
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "ErrorOccurred",
				G_CALLBACK(error_occurred), NULL, NULL);

	dbus_g_proxy_add_signal(proxy, "Cancelled", G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "Cancelled",
				G_CALLBACK(transfer_cancelled), NULL, NULL);

	dbus_g_proxy_add_signal(proxy, "TransferStarted", G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "TransferStarted",
				G_CALLBACK(transfer_started), NULL, NULL);

	dbus_g_proxy_add_signal(proxy, "TransferProgress",
						G_TYPE_UINT64, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "TransferProgress",
				G_CALLBACK(transfer_progress), NULL, NULL);

	dbus_g_proxy_add_signal(proxy, "TransferCompleted", G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "TransferCompleted",
				G_CALLBACK(transfer_completed), NULL, NULL);

	dbus_g_proxy_call(proxy, "Connect", NULL, G_TYPE_INVALID,
							G_TYPE_INVALID);
}

static gchar *get_name(DBusGProxy *device)
{
	GHashTable *hash;

	if (dbus_g_proxy_call(device, "GetProperties", NULL,
			G_TYPE_INVALID, dbus_g_type_get_map("GHashTable",
						G_TYPE_STRING, G_TYPE_VALUE),
					&hash, G_TYPE_INVALID) != FALSE) {
		GValue *value;
		gchar *name;

		value = g_hash_table_lookup(hash, "Name");
		name = value ? g_value_dup_string(value) : NULL;
		g_hash_table_destroy(hash);

		return name;
	}

	return NULL;
}

static gchar *get_device_name(const gchar *address)
{
	DBusGConnection *connection;
	DBusGProxy *manager;
	GPtrArray *adapters;
	gchar *name;
	guint i;

	name = NULL;

	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return name;

	manager = dbus_g_proxy_new_for_name(connection, "org.bluez",
						"/", "org.bluez.Manager");
	if (manager == NULL) {
		dbus_g_connection_unref(connection);
		return name;
	}

	if (dbus_g_proxy_call(manager, "ListAdapters", NULL,
			G_TYPE_INVALID, dbus_g_type_get_collection("GPtrArray",
						DBUS_TYPE_G_OBJECT_PATH),
					&adapters, G_TYPE_INVALID) == FALSE) {
		g_object_unref(manager);
		dbus_g_connection_unref(connection);
		return name;
	}

	for (i = 0; i < adapters->len && name == NULL; i++) {
		DBusGProxy *adapter;
		char *device_path;

		adapter = dbus_g_proxy_new_for_name(connection, "org.bluez",
						g_ptr_array_index(adapters, i),
							"org.bluez.Adapter");

		if (dbus_g_proxy_call(adapter, "FindDevice", NULL,
					G_TYPE_STRING, address, G_TYPE_INVALID,
					DBUS_TYPE_G_OBJECT_PATH, &device_path,
						G_TYPE_INVALID) == TRUE) {
			DBusGProxy *device;
			device = dbus_g_proxy_new_for_name(connection,
						"org.bluez", device_path,
							"org.bluez.Device");
			name = get_name(device);
			g_object_unref(device);
			break;
		}

		g_object_unref(adapter);
	}

	g_ptr_array_free(adapters, TRUE);
	g_object_unref(manager);

	dbus_g_connection_unref(connection);

	return name;
}

static gboolean request_callback(DBusGMethodInvocation *context,
				DBusGProxy *transfer, gpointer user_data)
{
	gchar *filename = option_files[file_index];
	gchar *basename, *text, *markup;
	GHashTable *hash;

	if (dbus_g_proxy_call(transfer,
				"GetProperties", NULL, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable",
						G_TYPE_STRING, G_TYPE_VALUE),
					&hash, G_TYPE_INVALID) == TRUE) {
		GValue *value;

		value = g_hash_table_lookup(hash, "Size");
		if (value) {
			current_size = g_value_get_uint64(value);

			last_update = get_system_time();
		}

		g_hash_table_destroy(hash);
	}

	text = g_path_get_dirname(filename);
	gtk_label_set_markup(GTK_LABEL(label_from), text);
	g_free(text);

	basename = g_path_get_basename(filename);
	text = g_strdup_printf(_("Sending %s"), basename);
	g_free(basename);
	markup = g_markup_printf_escaped("<i>%s</i>", text);
	gtk_label_set_markup(GTK_LABEL(label_status), markup);
	g_free(markup);
	g_free(text);

	text = g_strdup_printf(_("Sending file %d of %d"),
						file_index + 1, file_count);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), text);
	g_free(text);

	dbus_g_method_return(context, "");

	return TRUE;
}

static gboolean progress_callback(DBusGMethodInvocation *context,
				DBusGProxy *transfer, guint64 transferred,
							gpointer user_data)
{
	transfer_progress(NULL, transferred, NULL);

	dbus_g_method_return(context);

	return TRUE;
}

static gboolean complete_callback(DBusGMethodInvocation *context,
				DBusGProxy *transfer, gpointer user_data)
{
	total_sent += current_size;

	file_index++;

	dbus_g_method_return(context);

	return TRUE;
}

static gboolean release_callback(DBusGMethodInvocation *context,
							gpointer user_data)
{
	dbus_g_method_return(context);

	gtk_label_set_markup(GTK_LABEL(label_status), NULL);

	gtk_widget_destroy(dialog);

	gtk_main_quit();

	return TRUE;
}

static void send_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError *error = NULL;

	if (dbus_g_proxy_end_call(proxy, call, &error,
						G_TYPE_INVALID) == FALSE) {
		gchar *text, *message;

		message = get_error_message(error);

		text = g_strdup_printf("<span foreground=\"red\">%s</span>",
								message);
		gtk_label_set_markup(GTK_LABEL(label_status), text);
		g_free(text);

		g_free(message);

		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
						GTK_RESPONSE_CLOSE, TRUE);
		return;
	}

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), NULL);

	first_update = get_system_time();
}

static void value_free(GValue *value)
{
	g_value_unset(value);
	g_free(value);
}

static void name_owner_changed(DBusGProxy *proxy, const char *name,
			const char *prev, const char *new, gpointer user_data)
{
	if (g_str_equal(name, "org.openobex") == TRUE && *new == '\0')
		gtk_main_quit();
}

static GOptionEntry options[] = {
	{ "device", 0, 0, G_OPTION_ARG_STRING, &option_device,
				N_("Remote device to use"), "ADDRESS" },
	{ "dest", 0, G_OPTION_FLAG_HIDDEN,
			G_OPTION_ARG_STRING, &option_device, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0,
			G_OPTION_ARG_FILENAME_ARRAY, &option_files },
	{ NULL },
};

int main(int argc, char *argv[])
{
	DBusGProxy *proxy;
	GError *error = NULL;
	int i;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	error = NULL;

	if (gtk_init_with_args(&argc, &argv, "[FILE...]",
				options, GETTEXT_PACKAGE, &error) == FALSE) {
		if (error != NULL) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");

		gtk_exit(1);
	}

	gtk_window_set_default_icon_name("bluetooth");

	if (option_files == NULL) {
		option_files = show_select_dialog();
		if (option_files == NULL)
			gtk_exit(1);
	}

	if (option_device == NULL) {
		option_device = show_browse_dialog();
		if (option_device == NULL) {
			g_strfreev(option_files);
			gtk_exit(1);
		}
	}

	file_count = g_strv_length(option_files);

	for (i = 0; i < file_count; i++) {
		gchar *filename;
		struct stat st;

		filename = filename_to_path(option_files[i]);

		if (filename != NULL) {
			g_free(option_files[i]);
			option_files[i] = filename;
		}

		if (g_file_test(option_files[i],
					G_FILE_TEST_IS_REGULAR) == FALSE) {
			option_files[i][0] = '\0';
			continue;
		}

		if (g_stat(option_files[i], &st) < 0)
			option_files[i][0] = '\0';
		else
			total_size += st.st_size;
	}

	conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (conn == NULL) {
		if (error != NULL) {
			g_printerr("Connecting to session bus failed: %s\n",
							error->message);
			g_error_free(error);
		} else
			g_print("An unknown error occured\n");

		gtk_exit(1);
	}

	dbus_g_object_register_marshaller(marshal_VOID__STRING_STRING,
					G_TYPE_NONE, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_object_register_marshaller(marshal_VOID__STRING_STRING_UINT64,
				G_TYPE_NONE, G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);

	dbus_g_object_register_marshaller(marshal_VOID__UINT64,
				G_TYPE_NONE, G_TYPE_UINT64, G_TYPE_INVALID);

	device_name = get_device_name(option_device);
	if (device_name == NULL)
		device_name = g_strdup(option_device);

	create_window();

	proxy = dbus_g_proxy_new_for_name_owner(conn, "org.openobex.client",
					"/", "org.openobex.Client", NULL);

	if (proxy == NULL) {
		proxy = dbus_g_proxy_new_for_name(conn, "org.openobex",
				"/org/openobex", "org.openobex.Manager");

		dbus_g_proxy_add_signal(proxy, "NameOwnerChanged",
					G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
				G_CALLBACK(name_owner_changed), NULL, NULL);

		dbus_g_proxy_begin_call(proxy, "CreateBluetoothSession",
					create_notify, NULL, NULL,
					G_TYPE_STRING, option_device,
					G_TYPE_STRING, "00:00:00:00:00:00",
					G_TYPE_STRING, "opp", G_TYPE_INVALID);
	} else {
		GHashTable *hash = NULL;
		GValue *value;
		ObexAgent *agent;

		agent = obex_agent_new();

		obex_agent_set_release_func(agent, release_callback, NULL);
		obex_agent_set_request_func(agent, request_callback, NULL);
		obex_agent_set_progress_func(agent, progress_callback, NULL);
		obex_agent_set_complete_func(agent, complete_callback, NULL);

		obex_agent_setup(agent, AGENT_PATH);

		hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free, (GDestroyNotify) value_free);

		value = g_new0(GValue, 1);
		g_value_init(value, G_TYPE_STRING);
		g_value_set_string(value, option_device);
		g_hash_table_insert(hash, g_strdup("Destination"), value);

		dbus_g_proxy_begin_call(proxy, "SendFiles",
				send_notify, NULL, NULL,
				dbus_g_type_get_map("GHashTable",
					G_TYPE_STRING, G_TYPE_VALUE), hash,
				G_TYPE_STRV, option_files,
				DBUS_TYPE_G_OBJECT_PATH, AGENT_PATH,
							G_TYPE_INVALID);
	}

	gtk_main();

	g_object_unref(proxy);

	dbus_g_connection_unref(conn);

	g_free(device_name);

	g_strfreev(option_files);
	g_free(option_device);

	return 0;
}
