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

#include "bluetooth-applet.h"
#include "notify.h"
#include "agent.h"

static GList *input_list = NULL;

typedef struct input_data input_data;
struct input_data {
	BluetoothApplet *applet;
	char *path;
	char *uuid;
	gboolean numeric;
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *entry;
};

static void input_free(input_data *input)
{
	gtk_widget_destroy(input->dialog);

	g_object_unref (G_OBJECT (input->applet));
	g_free(input->uuid);
	g_free(input->path);

	input_list = g_list_remove(input_list, input);

	g_free(input);

}

static void pin_callback(GtkWidget *dialog,
				gint response, gpointer user_data)
{
	input_data *input = user_data;

	if (response == GTK_RESPONSE_OK) {
		const char *text;

		text = gtk_entry_get_text(GTK_ENTRY(input->entry));

		if (input->numeric == TRUE) {
			guint pin = atoi(text);
			bluetooth_applet_agent_reply_passkey (input->applet, input->path, pin);
		} else
			bluetooth_applet_agent_reply_pincode (input->applet, input->path, text);
	} else {
		if (input->numeric == TRUE)
			bluetooth_applet_agent_reply_passkey (input->applet, input->path, -1);
		else
			bluetooth_applet_agent_reply_pincode (input->applet, input->path, NULL);
	}

	input_free(input);
}

static void
confirm_callback (GtkWidget *dialog,
		  gint response,
		  gpointer user_data)
{
	input_data *input = user_data;

	bluetooth_applet_agent_reply_confirm (input->applet, input->path, response == GTK_RESPONSE_ACCEPT);

	input_free(input);
}

static void
auth_callback (GtkWidget *dialog,
               gint response,
               gpointer user_data)
{
	input_data *input = user_data;

	if (response == GTK_RESPONSE_ACCEPT) {
		gboolean trusted = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (input->button));
		bluetooth_applet_agent_reply_auth (input->applet, input->path, TRUE, trusted);
	} else
		bluetooth_applet_agent_reply_auth (input->applet, input->path, FALSE, FALSE);

	input_free(input);
}

static void insert_callback(GtkEditable *editable, const gchar *text,
			gint length, gint *position, gpointer user_data)
{
	input_data *input = user_data;
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
	input_data *input = user_data;
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(input->entry));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (input->dialog),
					   GTK_RESPONSE_OK,
					   *text != '\0' ? TRUE : FALSE);
}

static void toggled_callback(GtkWidget *button, gpointer user_data)
{
	input_data *input = user_data;
	gboolean mode;

	mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	gtk_entry_set_visibility (GTK_ENTRY (input->entry), mode);
}

static void
pin_dialog (BluetoothApplet *applet,
            const char *path,
            const char *name,
            const char *long_name,
            gboolean numeric)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *entry;
	GtkBuilder *xml;
	char *str;
	input_data *input;

	xml = gtk_builder_new ();
	if (gtk_builder_add_from_file (xml, "passkey-dialogue.ui", NULL) == 0)
		gtk_builder_add_from_file (xml, PKGDATADIR "/passkey-dialogue.ui", NULL);

	input = g_new0 (input_data, 1);
	input->applet = g_object_ref (G_OBJECT (applet));
	input->path = g_strdup (path);
	input->numeric = numeric;

	dialog = GTK_WIDGET (gtk_builder_get_object (xml, "dialog"));

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	if (notification_supports_actions () != FALSE)
		gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
	else
		gtk_window_set_focus_on_map (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	input->dialog = dialog;

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_OK,
					   FALSE);

	str = g_strdup_printf (_("Device '%s' wants to pair with this computer"),
			       name);
	g_object_set (G_OBJECT (dialog), "text", str, NULL);
	g_free (str);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Please enter the PIN mentioned on device %s."),
						  long_name);

	entry = GTK_WIDGET (gtk_builder_get_object (xml, "entry"));
	if (numeric == TRUE) {
		gtk_entry_set_max_length (GTK_ENTRY (entry), 6);
		gtk_entry_set_width_chars (GTK_ENTRY (entry), 6);
		g_signal_connect (G_OBJECT (entry), "insert-text",
				  G_CALLBACK (insert_callback), input);
	} else {
		gtk_entry_set_max_length (GTK_ENTRY (entry), 16);
		gtk_entry_set_width_chars (GTK_ENTRY (entry), 16);
		gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
	}
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	input->entry = entry;
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (changed_callback), input);

	button = GTK_WIDGET (gtk_builder_get_object (xml, "showinput_button"));
	if (numeric == FALSE) {
		g_signal_connect (G_OBJECT (button), "toggled",
				  G_CALLBACK (toggled_callback), input);
	} else {
		gtk_widget_set_no_show_all (button, TRUE);
		gtk_widget_hide (button);
	}

	input_list = g_list_append(input_list, input);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (pin_callback), input);
}

static void
confirm_dialog (BluetoothApplet *applet,
                const char *path,
                const char *name,
                const char *long_name,
                const char *value)
{
	GtkWidget *dialog;
	GtkBuilder *xml;
	char *str;
	input_data *input;

	input = g_new0 (input_data, 1);
	input->applet = g_object_ref (G_OBJECT (applet));
	input->path = g_strdup (path);

	xml = gtk_builder_new ();
	if (gtk_builder_add_from_file (xml, "confirm-dialogue.ui", NULL) == 0)
		gtk_builder_add_from_file (xml, PKGDATADIR "/confirm-dialogue.ui", NULL);

	dialog = GTK_WIDGET (gtk_builder_get_object (xml, "dialog"));
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	if (notification_supports_actions () != FALSE)
		gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
	else
		gtk_window_set_focus_on_map (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	input->dialog = dialog;

	str = g_strdup_printf (_("Device '%s' wants to pair with this computer"),
			       name);
	g_object_set (G_OBJECT (dialog), "text", str, NULL);
	g_free (str);

	str = g_strdup_printf ("<b>%s</b>", value);
	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
						    _("Please confirm whether the PIN '%s' matches the one on device %s."),
						    str, long_name);
	g_free (str);

	input_list = g_list_append (input_list, input);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (confirm_callback), input);
}

static void
auth_dialog (BluetoothApplet *applet,
             const char *path,
             const char *name,
             const char *long_name,
             const char *uuid)
{
	GtkBuilder *xml;
	GtkWidget *dialog;
	GtkWidget *button;
	char *str;
	input_data *input;

	input = g_new0 (input_data, 1);
	input->applet = g_object_ref (G_OBJECT (applet));
	input->path = g_strdup (path);
	input->uuid = g_strdup (uuid);

	xml = gtk_builder_new ();
	if (gtk_builder_add_from_file (xml, "authorisation-dialogue.ui", NULL) == 0)
		gtk_builder_add_from_file (xml, PKGDATADIR "/authorisation-dialogue.ui", NULL);

	dialog = GTK_WIDGET (gtk_builder_get_object (xml, "dialog"));
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	if (notification_supports_actions () != FALSE)
		gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
	else
		gtk_window_set_focus_on_map (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	input->dialog = dialog;

	/* translators: Whether to grant access to a particular service */
	str = g_strdup_printf (_("Grant access to '%s'"), uuid);
	g_object_set (G_OBJECT (dialog), "text", str, NULL);
	g_free (str);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Device %s wants access to the service '%s'."),
						  long_name, uuid);

	button = GTK_WIDGET (gtk_builder_get_object (xml, "always_button"));
	input->button = button;

	input_list = g_list_append (input_list, input);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (auth_callback), input);
}

static void show_dialog(gpointer data, gpointer user_data)
{
	input_data *input = data;

	gtk_widget_show_all(input->dialog);

	gtk_window_present(GTK_WINDOW(input->dialog));
}

static void present_notification_dialogs (void)
{
	g_list_foreach(input_list, show_dialog, NULL);
}

static void notification_closed(GObject *object, gpointer user_data)
{
	present_notification_dialogs ();
}

static gboolean
pin_request(BluetoothApplet *applet,
            char *path,
            char *name,
            char *long_name,
            gboolean numeric,
            gpointer user_data)
{
	char *line;

	pin_dialog(applet, path, name, long_name, numeric);

	if (notification_supports_actions () == FALSE) {
		present_notification_dialogs ();
		return TRUE;
	}

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Pairing request for '%s'"), name);

	show_notification(_("Bluetooth device"),
			  line, _("Enter PIN"), 0,
			  G_CALLBACK(notification_closed));

	g_free(line);

	return TRUE;
}

static gboolean
confirm_request (BluetoothApplet *applet,
                 char *path,
                 char *name,
                 char *long_name,
                 guint pin,
                 gpointer user_data)
{
	char *text, *line;

	text = g_strdup_printf("%06d", pin);
	confirm_dialog(applet, path, name, long_name, text);
	g_free(text);

	/* translators: this is a popup telling you a particular device
	 * has asked for pairing */
	line = g_strdup_printf(_("Pairing confirmation for '%s'"), name);

	/* translators:
	 * This message is for Bluetooth 2.1 support, when the
	 * action is clicked in the notification popup, the user
	 * will get to check whether the PIN matches the one
	 * showing on the Bluetooth device */
	if (notification_supports_actions () != FALSE)
		show_notification(_("Bluetooth device"),
				    line, _("Verify PIN"), 0,
				    G_CALLBACK(notification_closed));
	else
		present_notification_dialogs ();

	g_free(line);

	return TRUE;
}

static gboolean
authorize_request (BluetoothApplet *applet,
                   char *path,
                   char *name,
                   char *long_name,
                   char *uuid,
                   gpointer user_data)
{
	char *line;

	auth_dialog (applet, path, name, long_name, uuid);

	if (notification_supports_actions () == FALSE) {
		present_notification_dialogs ();
		return TRUE;
	}

	line = g_strdup_printf (_("Authorization request from '%s'"), name);

	show_notification (_("Bluetooth device"),
			   line, _("Check authorization"), 0,
			   G_CALLBACK (notification_closed));

	g_free (line);

	return TRUE;
}

static void input_free_list (gpointer data, gpointer user_data) {
	input_data *input = data;

	gtk_widget_destroy(input->dialog);

	g_object_unref (G_OBJECT (input->applet));
	g_free(input->uuid);
	g_free(input->path);
	g_free(input);
}

static void cancel_request(BluetoothApplet *applet,
							gpointer user_data)
{
	g_list_foreach (input_list, input_free_list, NULL);
	g_list_free (input_list);
	input_list = NULL;
}

int setup_agents(BluetoothApplet *applet)
{
	g_signal_connect (applet, "pincode-request",
			G_CALLBACK (pin_request), NULL);
	g_signal_connect (applet, "confirm-request",
			G_CALLBACK (confirm_request), NULL);
	g_signal_connect (applet, "auth-request",
			G_CALLBACK (authorize_request), NULL);
	g_signal_connect (applet, "cancel-request",
			G_CALLBACK (cancel_request), NULL);

	return 0;
}

void show_agents(void)
{
	close_notification();

	g_list_foreach(input_list, show_dialog, NULL);
}
