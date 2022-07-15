/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2013 Intel Corporation.
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

#include <adwaita.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <bluetooth-client.h>
#include <bluetooth-device.h>

#define OBEX_SERVICE	"org.bluez.obex"
#define OBEX_PATH	"/org/bluez/obex"
#define TRANSFER_IFACE	"org.bluez.obex.Transfer1"
#define OPP_IFACE	"org.bluez.obex.ObjectPush1"
#define CLIENT_IFACE	"org.bluez.obex.Client1"

#define RESPONSE_RETRY 1

static GDBusConnection *conn = NULL;
static GDBusProxy *client_proxy = NULL;
static GDBusProxy *session = NULL;
static GDBusProxy *current_transfer = NULL;
static GCancellable *cancellable = NULL;

static GtkWidget *dialog;
static GtkWidget *label_from;
static GtkWidget *image_status;
static GtkWidget *label_status;
static GtkWidget *progress;

static gchar *option_device = NULL;
static gchar *option_device_name = NULL;
static gchar **option_files = NULL;

static guint64 current_size = 0;
static guint64 total_size = 0;
static guint64 total_sent = 0;

static int file_count = 0;
static int file_index = 0;

static gint64 first_update = 0;
static gint64 last_update = 0;

static void on_transfer_properties (GVariant *props);
static void on_transfer_progress (guint64 transferred);
static void on_transfer_complete (void);
static void on_transfer_error (void);

static gint64
get_system_time (void)
{
	struct timeval tmp;

	gettimeofday(&tmp, NULL);

	return (gint64) tmp.tv_usec +
		(gint64) tmp.tv_sec * G_GINT64_CONSTANT(1000000);
}

static void
update_from_label (void)
{
	char *filename = option_files[file_index];
	GFile *file, *dir;
	char *text, *markup;

	file = g_file_new_for_path (filename);
	dir = g_file_get_parent (file);
	g_object_unref (file);
	if (g_file_has_uri_scheme (dir, "file") != FALSE) {
		text = g_file_get_path (dir);
	} else {
		text = g_file_get_uri (dir);
	}
	markup = g_markup_escape_text (text, -1);
	g_free (text);
	g_object_unref (dir);
	gtk_label_set_markup (GTK_LABEL (label_from), markup);
	g_free (markup);
}

static char *
cleanup_error (GError *error)
{
	char *remote_error;

	if (!error || *error->message == '\0')
		return g_strdup (_("An unknown error occurred"));
	if (g_dbus_error_is_remote_error (error) == FALSE)
		return g_strdup (error->message);

	remote_error = g_dbus_error_get_remote_error (error);
	g_debug ("Remote error is: %s", remote_error);
	g_free (remote_error);

	g_dbus_error_strip_remote_error (error);
	g_debug ("Error message is: %s", error->message);

	/* And now, take advantage of the fact that obexd isn't translated */
	if (g_strcmp0 (error->message, "Unable to find service record") == 0) {
		return g_strdup (_("Make sure that the remote device is switched on and that it accepts Bluetooth connections"));
	}

	return g_strdup (error->message);
}

static void
handle_error (GError *error)
{
	char *message;

	message = cleanup_error (error);

	gtk_widget_show (image_status);
	gtk_label_set_markup (GTK_LABEL (label_status), message);
	g_clear_error (&error);
	g_free (message);

	/* Clear the progress bar as it may be saying 'Connecting' or
	 * 'Sending file 1 of 1' which is not true. */
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), "");

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_RETRY, TRUE);
}

static void
transfer_properties_changed (GDBusProxy *proxy,
			     GVariant *changed_properties,
			     GStrv invalidated_properties,
			     gpointer user_data)
{
	GVariantIter iter;
	const char *key;
	GVariant *value;

	g_variant_iter_init (&iter, changed_properties);
	while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
		if (g_str_equal (key, "Status")) {
			const char *status;

			status = g_variant_get_string (value, NULL);

			if (g_str_equal (status, "complete")) {
				on_transfer_complete ();
			} else if (g_str_equal (status, "error")) {
				on_transfer_error ();
			}
		} else if (g_str_equal (key, "Transferred")) {
			guint64 transferred = g_variant_get_uint64 (value);

			on_transfer_progress (transferred);
		}

		g_variant_unref (value);
	}
}

static void
transfer_proxy (GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;

	current_transfer = g_dbus_proxy_new_finish (res, &error);

	if (current_transfer == NULL) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			return;
		}

		handle_error (error);
		return;
	}

	g_signal_connect (G_OBJECT (current_transfer), "g-properties-changed",
		G_CALLBACK (transfer_properties_changed), NULL);
}

static void
transfer_created (GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *variant, *properties;
	const char *transfer;

	variant = g_dbus_proxy_call_finish (proxy, res, &error);

	if (variant == NULL) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			return;
		}

		handle_error (error);
		return;
	}

	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), NULL);

	first_update = get_system_time ();

	g_variant_get (variant, "(&o@a{sv})", &transfer, &properties);

	on_transfer_properties (properties);

	g_dbus_proxy_new (conn,
			  G_DBUS_PROXY_FLAGS_NONE,
			  NULL,
			  OBEX_SERVICE,
			  transfer,
			  TRANSFER_IFACE,
			  cancellable,
			  (GAsyncReadyCallback) transfer_proxy,
			  NULL);

	g_variant_unref (properties);
	g_variant_unref (variant);
}

static void
send_next_file (void)
{
	update_from_label ();

	g_dbus_proxy_call (session,
			   "SendFile",
			   g_variant_new ("(s)", option_files[file_index]),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   (GAsyncReadyCallback) transfer_created,
			   NULL);
}

static void
session_proxy (GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;

	g_clear_object (&session);
	session = g_dbus_proxy_new_finish (res, &error);

	if (session == NULL) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			return;
		}

		handle_error (error);
		return;
	}

	send_next_file ();
}

static void
session_created (GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *variant;
	const char *session;

	variant = g_dbus_proxy_call_finish (proxy, res, &error);

	if (variant == NULL) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			return;
		}

		handle_error (error);
		return;
	}

	g_variant_get (variant, "(&o)", &session);

	g_dbus_proxy_new (conn,
			  G_DBUS_PROXY_FLAGS_NONE,
			  NULL,
			  OBEX_SERVICE,
			  session,
			  OPP_IFACE,
			  cancellable,
			  (GAsyncReadyCallback) session_proxy,
			  NULL);

	g_variant_unref (variant);
}

static void
send_files (void)
{
	GVariant *parameters;
	GVariantBuilder *builder;

	builder = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);
	g_variant_builder_add (builder, "{sv}", "Target",
						g_variant_new_string ("opp"));

	parameters = g_variant_new ("(sa{sv})", option_device, builder);

	g_dbus_proxy_call (client_proxy,
			   "CreateSession",
			   parameters,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   (GAsyncReadyCallback) session_created,
			   NULL);

	g_variant_builder_unref (builder);
}

static gchar *filename_to_path(const gchar *filename)
{
	GFile *file;
	gchar *path;

	file = g_file_new_for_commandline_arg(filename);
	path = g_file_get_path(file);
	g_object_unref(file);

	return path;
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
	if (response == RESPONSE_RETRY) {
		/* Reset buttons */
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_RETRY, FALSE);

		/* Reset status and progress bar */
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress),
					  _("Connecting…"));
		gtk_label_set_text (GTK_LABEL (label_status), "");
		gtk_widget_hide (image_status);

		/* If we have a session, we don't need to create another one. */
		if (session)
			send_next_file ();
		else
			send_files ();

		return;
	}

	/* Cancel any ongoing dbus calls we may have */
	g_cancellable_cancel (cancellable);

	if (current_transfer != NULL) {
		g_dbus_proxy_call (current_transfer,
				   "Cancel",
				   NULL,
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   (GAsyncReadyCallback) NULL,
				   NULL);
		g_object_unref (current_transfer);
		current_transfer = NULL;
	}

	gtk_window_destroy(GTK_WINDOW (dialog));
}

static void create_window(void)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *table;
	GtkWidget *label;
	gchar *text;

	dialog = g_object_new (GTK_TYPE_DIALOG,
			       "use-header-bar", 1,
			       "title", _("Bluetooth File Transfer"),
			       NULL);
	gtk_dialog_add_buttons(GTK_DIALOG (dialog),
			       _("_Cancel"), GTK_RESPONSE_CANCEL,
			       _("_Retry"), RESPONSE_RETRY,
			       NULL);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_RETRY, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_end(vbox, 12);
	gtk_widget_set_margin_start(vbox, 12);
	gtk_widget_set_margin_top(vbox, 12);
	gtk_widget_set_margin_bottom(vbox, 12);
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_window_set_child (GTK_WINDOW(dialog), vbox);

	table = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_box_append(GTK_BOX(vbox), table);

	label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	text = g_markup_printf_escaped("<b>%s</b>", _("From:"));
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	label_from = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label_from), 0.0);
	gtk_label_set_yalign(GTK_LABEL(label_from), 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(label_from), PANGO_ELLIPSIZE_MIDDLE);
	gtk_grid_attach(GTK_GRID(table), label_from, 1, 0, 1, 1);

	update_from_label ();

	label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	text = g_markup_printf_escaped("<b>%s</b>", _("To:"));
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_text(GTK_LABEL(label), option_device_name);
	gtk_grid_attach(GTK_GRID(table), label, 1, 1, 1, 1);

	progress = gtk_progress_bar_new();
	gtk_widget_set_vexpand (progress, TRUE);
	gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (progress), TRUE);
	gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(progress),
							PANGO_ELLIPSIZE_END);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress),
							_("Connecting…"));
	gtk_box_append(GTK_BOX(vbox), progress);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	image_status = gtk_image_new_from_icon_name ("dialog-warning");
	gtk_widget_hide(image_status);
	gtk_box_append(GTK_BOX(hbox), image_status);

	label_status = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label_status), 0.0);
	gtk_label_set_yalign(GTK_LABEL(label_status), 0.5);
	gtk_label_set_wrap(GTK_LABEL(label_status), TRUE);
	gtk_box_append(GTK_BOX(hbox), label_status);

	gtk_box_append(GTK_BOX(vbox), hbox);

	g_signal_connect(G_OBJECT(dialog), "response",
				G_CALLBACK(response_callback), NULL);

	gtk_window_present(GTK_WINDOW(dialog));
}

static char *
get_device_name (const char *address)
{
	g_autoptr(BluetoothClient) client = NULL;
	g_autoptr(GListStore) model = NULL;
	guint n_devices, i;

	model = bluetooth_client_get_devices (client);
	n_devices = g_list_model_get_n_items (G_LIST_MODEL (model));
	for (i = 0; i < n_devices; i++) {
		g_autoptr(BluetoothDevice) device = NULL;
		g_autofree char *name = NULL;
		g_autofree char *_address = NULL;

		g_object_get (device,
			      "name", &name,
			      "address", &_address,
			      NULL);
		if (g_strcmp0 (address, _address) != 0)
			continue;

		return g_steal_pointer (&name);
	}

	return NULL;
}

static void
on_transfer_properties (GVariant *props)
{
	char *filename = option_files[file_index];
	char *basename, *text, *markup;
	GVariant *size;

	size = g_variant_lookup_value (props, "Size", G_VARIANT_TYPE_UINT64);
	if (size) {
		current_size = g_variant_get_uint64 (size);
		last_update = get_system_time ();
	}

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
}

static void
on_transfer_progress (guint64 transferred)
{
	gint64 current_time;
	gint elapsed_time;
	gint remaining_time;
	gint transfer_rate;
	guint64 current_sent;
	gdouble fraction;
	gchar *time, *rate, *file, *text;

	current_sent = total_sent + transferred;
	if (total_size == 0)
		fraction = 0.0;
	else
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
		rate = g_strdup_printf(_("%d kB/s"), transfer_rate / 1000);
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

static void
on_transfer_complete (void)
{
	total_sent += current_size;

	file_index++;

	/* And we're done with the transfer */
	g_object_unref (current_transfer);
	current_transfer = NULL;

	if (file_index == file_count) {
		GtkWidget *button;
		char *complete;

		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 1.0);

		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), "");

		complete = g_strdup_printf (ngettext ("%u transfer complete",
						      "%u transfers complete",
						      file_count), file_count);
		gtk_label_set_text (GTK_LABEL (label_status), complete);
		g_free (complete);

		button = gtk_dialog_get_widget_for_response(GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
		gtk_button_set_label (GTK_BUTTON (button), _("_Close"));
	} else {
		send_next_file ();
	}
}

static void
on_transfer_error (void)
{
	gtk_widget_show (image_status);
	gtk_label_set_markup (GTK_LABEL (label_status), _("There was an error"));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_RETRY, TRUE);

	g_object_unref (current_transfer);
	current_transfer = NULL;
}

static gint select_dialog_response;
static GMainLoop *select_dialog_mainloop = NULL;

static void select_dialog_response_callback(GtkWidget *dialog,
					gint response, gpointer user_data)
{
	select_dialog_response = response;
	g_main_loop_quit(select_dialog_mainloop);
}
static char **
show_select_dialog(void)
{
	GtkWidget *dialog, *button;
	gchar **files = NULL;

	dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			       "title", _("Choose files to send"),
			       "action", GTK_FILE_CHOOSER_ACTION_OPEN,
			       "use-header-bar", 1,
			       "hide-on-close", TRUE,
			       NULL);
	gtk_dialog_add_buttons(GTK_DIALOG (dialog),
			       _("_Cancel"), GTK_RESPONSE_CANCEL,
			       _("Select"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

	button = gtk_dialog_get_widget_for_response(GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_add_css_class(button, "suggested-action");

	g_signal_connect(dialog, "response", G_CALLBACK(select_dialog_response_callback), NULL);

	gtk_widget_show(dialog);

	select_dialog_mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(select_dialog_mainloop);

	if (select_dialog_response == GTK_RESPONSE_ACCEPT) {
		g_autoptr(GListModel) selected_files = NULL;
		guint i, n_items;

		selected_files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(dialog));
		n_items = g_list_model_get_n_items(selected_files);

		files = g_new(gchar *, n_items + 1);

		for (i = 0; i < n_items; i++) {
			g_autoptr(GFile) file = g_list_model_get_item(selected_files, i);
			files[i] = g_file_get_path(file);
		}
		files[i] = NULL;
	}

	gtk_window_destroy(GTK_WINDOW(dialog));

	return files;
}

static GOptionEntry options[] = {
	{ "device", 0, 0, G_OPTION_ARG_STRING, &option_device,
				N_("Remote device to use"), N_("ADDRESS") },
	{ "name", 0, 0, G_OPTION_ARG_STRING, &option_device_name,
				N_("Remote device’s name"), N_("NAME") },
	{ "dest", 0, G_OPTION_FLAG_HIDDEN,
			G_OPTION_ARG_STRING, &option_device, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0,
			G_OPTION_ARG_FILENAME_ARRAY, &option_files },
	{ NULL },
};

int main(int argc, char *argv[])
{
	g_autoptr(GOptionContext) option_context = NULL;
	GError *error = NULL;
	int i;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	error = NULL;

	gtk_init();
	adw_init();

	option_context = g_option_context_new(NULL);
	g_option_context_add_main_entries(option_context, options, GETTEXT_PACKAGE);
	if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE) {
		if (error != NULL) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");

		return 1;
	}

	gtk_window_set_default_icon_name("bluetooth");

	cancellable = g_cancellable_new ();

	/* A device name, but no device? */
	if (option_device == NULL && option_device_name != NULL) {
		if (option_files != NULL)
			g_strfreev(option_files);
		g_free (option_device_name);
		return 1;
	}

	if (option_files == NULL) {
		option_files = show_select_dialog();
		if (option_files == NULL)
			return 1;
	}

	if (option_device == NULL)
		return 1;

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

	conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (conn == NULL) {
		if (error != NULL) {
			g_printerr("Connecting to session bus failed: %s\n",
							error->message);
			g_error_free(error);
		} else
			g_print("An unknown error occurred\n");

		return 1;
	}

	client_proxy = g_dbus_proxy_new_sync (conn,
					      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
					      NULL,
					      OBEX_SERVICE,
					      OBEX_PATH,
					      CLIENT_IFACE,
					      cancellable,
					      &error);
	if (client_proxy == NULL) {
		g_printerr("Acquiring proxy failed: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	if (option_device_name == NULL)
		option_device_name = get_device_name(option_device);
	if (option_device_name == NULL)
		option_device_name = g_strdup(option_device);

	create_window();

	if (!g_cancellable_is_cancelled (cancellable))
		send_files ();

	while (g_list_model_get_n_items (gtk_window_get_toplevels()) > 0)
		g_main_context_iteration (NULL, TRUE);

	g_cancellable_cancel (cancellable);

	g_clear_object (&cancellable);
	g_clear_object (&current_transfer);
	g_clear_object (&session);
	g_object_unref (client_proxy);
	g_object_unref (conn);

	g_strfreev(option_files);
	g_free(option_device);
	g_free(option_device_name);

	return 0;
}
