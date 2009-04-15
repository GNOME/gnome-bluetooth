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
#include <gdk/gdkkeysyms.h>

#include <dbus/dbus-glib.h>
#include <unique/uniqueapp.h>

#include <bluetooth-client.h>
#include <bluetooth-chooser.h>
#include <bluetooth-agent.h>

#include "pin.h"

#define AGENT_PATH "/org/bluez/agent/wizard"

static gboolean set_page_search_complete(void);

static BluetoothClient *client;
static BluetoothAgent *agent;

static gchar *target_address = NULL;
static gchar *target_name = NULL;
static gchar *target_pincode = NULL;
static guint target_type = BLUETOOTH_TYPE_ANY;
static gboolean target_ssp = FALSE;

/* NULL means automatic, anything else is a pincode specified by the user */
static gchar *user_pincode = NULL;
/* If TRUE, then we won't display the PIN code to the user when pairing */
static gboolean automatic_pincode = FALSE;

static GtkWidget *window_assistant = NULL;
static GtkWidget *page_search = NULL;
static GtkWidget *page_setup = NULL;
static GtkWidget *page_summary = NULL;

static GtkWidget *label_setup = NULL;
static GtkWidget *label_passkey = NULL;
static GtkWidget *label_passkey_help = NULL;

static BluetoothChooser *selector = NULL;

static GtkWidget *passkey_dialog = NULL;
static GtkWidget *radio_auto = NULL;
static GtkWidget *radio_0000 = NULL;
static GtkWidget *radio_1111 = NULL;
static GtkWidget *radio_1234 = NULL;
static GtkWidget *radio_custom = NULL;
static GtkWidget *entry_custom = NULL;

/* Signals */
void close_callback(GtkWidget *assistant, gpointer data);
void prepare_callback(GtkWidget *assistant, GtkWidget *page, gpointer data);
void select_device_changed(BluetoothChooser *selector, gchar *address, gpointer user_data);
void device_selected_name_cb (GObject *object, GParamSpec *spec, gpointer user_data);
gboolean entry_custom_event(GtkWidget *entry, GdkEventKey *event);
void set_user_pincode(GtkWidget *button);
void toggle_set_sensitive(GtkWidget *button, gpointer data);
void passkey_option_button_clicked (GtkButton *button, gpointer data);
void entry_custom_changed(GtkWidget *entry);

static void
set_large_label (GtkLabel *label, const char *text)
{
	char *str;

	str = g_strdup_printf("<span font=\"50\" color=\"black\" bgcolor=\"white\">  %s  </span>", text);
	gtk_label_set_markup(GTK_LABEL(label_passkey), str);
	g_free(str);
}

static gboolean pincode_callback(DBusGMethodInvocation *context,
					DBusGProxy *device, gpointer user_data)
{
	char *pincode;

	if (user_pincode != NULL && *user_pincode != '\0') {
		pincode = g_strdup (user_pincode);
	} else {
		pincode = get_pincode_for_device(target_type, target_address, target_name);
		if (pincode == NULL)
			pincode = g_strdup(target_pincode);
		else
			automatic_pincode = TRUE;
	}

	if (automatic_pincode == FALSE) {
		gtk_widget_show (label_passkey_help);
		gtk_widget_show (label_passkey);

		gtk_label_set_markup(GTK_LABEL(label_passkey_help), _("Please enter the following passkey:"));
		set_large_label (GTK_LABEL (label_passkey), pincode);
	} else {
		char *text;

		gtk_widget_show (label_passkey_help);
		gtk_widget_hide (label_passkey);

		/* translators:
		 * The '%s' is the device name, for example:
		 * Please wait whilst 'Sony Bluetooth Headset' is being paired
		 */
		text = g_strdup_printf(_("Please wait whilst '%s' is being paired"), target_name);
		gtk_label_set_markup(GTK_LABEL(label_passkey_help), text);
		g_free(text);
	}

	dbus_g_method_return(context, pincode);
	g_free(pincode);

	return TRUE;
}

static gboolean display_callback(DBusGMethodInvocation *context,
				 DBusGProxy *device, guint passkey,
				 guint entered, gpointer user_data)
{
	gchar *text, *done, *code;

	code = g_strdup_printf("%d", passkey);

	if (entered > 0) {
		GtkEntry *entry;
		gunichar invisible;

		entry = GTK_ENTRY (gtk_entry_new ());
		invisible = gtk_entry_get_invisible_char (entry);
		done = g_strnfill(entered, invisible);
		g_object_unref (entry);
	} else {
		done = g_strdup ("");
	}

	gtk_widget_show (label_passkey_help);

	gtk_label_set_markup(GTK_LABEL(label_passkey_help), _("Please enter the following passkey:"));
	text = g_strdup_printf("%s%s", done, code + entered);
	set_large_label (GTK_LABEL (label_passkey), text);
	g_free(text);

	g_free(done);
	g_free(code);

	target_ssp = TRUE;

	dbus_g_method_return(context);

	return TRUE;
}

static gboolean cancel_callback(DBusGMethodInvocation *context,
							gpointer user_data)
{
	gchar *text;

	if (target_ssp == FALSE) {
		/* translators:
		 * The '%s' is the device name, for example:
		 * Pairing with 'Sony Bluetooth Headset' cancelled
		 */
		text = g_strdup_printf(_("Pairing with '%s' cancelled"), target_name);
	} else {
		/* translators:
		 * The '%s' is the device name, for example:
		 * Pairing with 'Sony Bluetooth Headset' finished
		 */
		text = g_strdup_printf(_("Pairing with '%s' finished"), target_name);
	}

	gtk_label_set_markup(GTK_LABEL(label_setup), text);

	g_free(text);

	gtk_widget_hide (label_passkey);
	gtk_widget_hide (label_passkey_help);

	gtk_label_set_markup(GTK_LABEL(label_passkey_help), NULL);
	gtk_label_set_markup(GTK_LABEL(label_passkey), NULL);

	dbus_g_method_return(context);

	return TRUE;
}

static void connect_callback(gpointer user_data)
{
	GtkAssistant *assistant = user_data;

	gtk_widget_hide (label_passkey_help);
	gtk_assistant_set_page_complete(assistant, page_setup, TRUE);
}

static void create_callback(const char *path, gpointer user_data)
{
	GtkAssistant *assistant = user_data;
	gboolean complete = FALSE;
	gchar *text;

	if (path != NULL) {
		gint page;

		/* translators:
		 * The '%s' is the device name, for example:
		 * Successfully paired with 'Sony Bluetooth Headset'
		 */
		text = g_strdup_printf(_("Successfully paired with '%s'"), target_name);

		page = gtk_assistant_get_current_page(assistant);
		if (page > 0)
			gtk_assistant_set_current_page(assistant, page + 1);

		bluetooth_client_set_trusted(client, path, TRUE);

		if (bluetooth_client_connect_service(client, path, connect_callback, assistant) != FALSE) {
			gtk_label_set_text (GTK_LABEL (label_passkey_help),
					    _("Please wait while setting up the device..."));
			gtk_widget_show (label_passkey_help);
		} else {
			complete = TRUE;
		}
	} else {
		/* translators:
		 * The '%s' is the device name, for example:
		 * Pairing with 'Sony Bluetooth Headset' failed
		 */
		text = g_strdup_printf(_("Pairing with '%s' failed"), target_name);
	}

	gtk_label_set_markup(GTK_LABEL(label_setup), text);

	gtk_widget_hide (label_passkey_help);
	gtk_widget_hide (label_passkey);

	g_free(text);

	gtk_assistant_set_page_complete(assistant, page_setup, complete);
}

void close_callback(GtkWidget *assistant, gpointer data)
{
	gtk_widget_destroy(assistant);

	gtk_main_quit();
}

void prepare_callback(GtkWidget *assistant,
		      GtkWidget *page,
		      gpointer data)
{
	gboolean complete = TRUE;
	const char *path = AGENT_PATH;

	if (page == page_search) {
		complete = set_page_search_complete ();
		bluetooth_client_start_discovery(client);
	} else {
		bluetooth_client_stop_discovery(client);
	}

	if (page == page_setup) {
		gchar *text, *address, *name, *pincode;
		guint type;

		/* Get the info about the device now,
		 * we can't get here without a valid selection */
		g_object_get (G_OBJECT (selector),
			      "device-selected", &address,
			      "device-selected-name", &name,
			      "device-selected-type", &type,
			      NULL);

		g_free(target_address);
		target_address = address;

		g_free(target_name);
		target_name = name;

		target_type = type;

		/* translators:
		 * The '%s' is the device name, for example:
		 * Connecting to 'Sony Bluetooth Headset' now...
		 */
		text = g_strdup_printf(_("Connecting to '%s' now..."), target_name);
		gtk_label_set_markup(GTK_LABEL(label_setup), text);

		g_free(text);

		complete = FALSE;

		g_object_ref(agent);

		/* Do we pair, or don't we? */
		pincode = get_pincode_for_device (target_type, target_address, target_name);
		if (pincode != NULL && g_str_equal (pincode, "NULL"))
			path = NULL;
		g_free (pincode);

		bluetooth_client_create_device(client, target_address,
					path, create_callback, assistant);
	}

	gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant),
							page, complete);
}

static gboolean
set_page_search_complete (void)
{
	char *name, *address;
	gboolean complete = FALSE;

	g_object_get (G_OBJECT (selector),
		      "device-selected", &address,
		      "device-selected-name", &name,
		      NULL);

	if (address != NULL && name != NULL)
		complete = (user_pincode == NULL || strlen(user_pincode) >= 4);

	g_free (address);
	g_free (name);

	gtk_assistant_set_page_complete(GTK_ASSISTANT (window_assistant),
					page_search, complete);

	return complete;
}

gboolean
entry_custom_event(GtkWidget *entry, GdkEventKey *event)
{
	if (event->length == 0)
		return FALSE;

	if ((event->keyval >= GDK_0 && event->keyval <= GDK_9) ||
	    (event->keyval >= GDK_KP_0 && event->keyval <= GDK_KP_9))
		return FALSE;

	return TRUE;
}

void
entry_custom_changed(GtkWidget *entry)
{
	user_pincode = g_strdup (gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (passkey_dialog),
					   GTK_RESPONSE_ACCEPT,
					   gtk_entry_get_text_length (GTK_ENTRY (entry)) >= 1);
}

void
toggle_set_sensitive(GtkWidget *button, gpointer data)
{
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	gtk_widget_set_sensitive(entry_custom, active);
	/* When selecting another PIN, make sure the "Close" button is sensitive */
	if (!active)
		gtk_dialog_set_response_sensitive (GTK_DIALOG (passkey_dialog),
						   GTK_RESPONSE_ACCEPT, TRUE);
}

void
set_user_pincode(GtkWidget *button)
{
	GSList *list, *l;

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	for (l = list; l ; l = l->next) {
		GtkEntry *entry;
		GtkWidget *radio;
		const char *pin;

		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
			continue;

		/* Is it radio_fixed that changed? */
		radio = g_object_get_data (G_OBJECT (button), "button");
		if (radio != NULL) {
			set_user_pincode (radio);
			return;
		}

		pin = g_object_get_data (G_OBJECT (button), "pin");
		entry = g_object_get_data (G_OBJECT (button), "entry");

		if (entry != NULL) {
			g_free (user_pincode);
			user_pincode = g_strdup (gtk_entry_get_text(entry));
			gtk_dialog_set_response_sensitive (GTK_DIALOG (passkey_dialog),
							   GTK_RESPONSE_ACCEPT,
							   gtk_entry_get_text_length (entry) >= 1);
		} else if (pin != NULL) {
			g_free (user_pincode);
			if (*pin == '\0')
				user_pincode = NULL;
			else
				user_pincode = g_strdup (pin);
		}

		break;
	}
}

void device_selected_name_cb (GObject *object,
			      GParamSpec *spec,
			      gpointer user_data)
{
	set_page_search_complete ();
}

void select_device_changed(BluetoothChooser *selector,
			   gchar *address,
			   gpointer user_data)
{
	set_page_search_complete ();
}

void
passkey_option_button_clicked (GtkButton *button, gpointer data)
{
	GtkWidget *radio;
	char *oldpin;

	oldpin = user_pincode;
	user_pincode = NULL;

	gtk_window_set_transient_for (GTK_WINDOW (passkey_dialog),
				      GTK_WINDOW (window_assistant));
	gtk_window_present (GTK_WINDOW (passkey_dialog));

	/* When reopening, try to guess where the pincode was set */
	if (oldpin == NULL)
		radio = radio_auto;
	else if (g_str_equal (oldpin, "0000"))
		radio = radio_0000;
	else if (g_str_equal (oldpin, "1111"))
		radio = radio_1111;
	else if (g_str_equal (oldpin, "1234"))
		radio = radio_1234;
	else
		radio = radio_custom;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	if (radio == radio_custom)
		gtk_entry_set_text (GTK_ENTRY (entry_custom), oldpin);

	if (gtk_dialog_run (GTK_DIALOG (passkey_dialog)) != GTK_RESPONSE_ACCEPT) {
		g_free (user_pincode);
		user_pincode = oldpin;
	} else {
		g_free (oldpin);
	}

	gtk_widget_hide(passkey_dialog);
}

static GtkWidget *create_wizard(void)
{
	GtkAssistant *assistant;
	GtkBuilder *builder;
	GError *err = NULL;
	GtkWidget *combo, *page_intro;
	GtkTreeModel *model;
	GtkCellRenderer *renderer;
	GdkPixbuf *pixbuf;

	builder = gtk_builder_new ();
	if (gtk_builder_add_from_file (builder, "wizard.ui", NULL) == 0) {
		if (gtk_builder_add_from_file (builder, PKGDATADIR "/wizard.ui", &err) == 0) {
			g_warning ("Could not load UI from %s: %s", PKGDATADIR "/wizard.ui", err->message);
			g_error_free(err);
			return NULL;
		}
	}

	assistant = GTK_ASSISTANT(gtk_builder_get_object(builder, "assistant"));

	/* Intro page */
	combo = gtk_combo_box_new();

	model = bluetooth_client_get_adapter_model(client);
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
	g_object_unref(model);

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer,
					"text", BLUETOOTH_COLUMN_NAME, NULL);

	page_intro = GTK_WIDGET(gtk_builder_get_object(builder, "page_intro"));
	if (gtk_tree_model_iter_n_children(model, NULL) > 1)
		gtk_box_pack_start(GTK_BOX(page_intro), combo, FALSE, FALSE, 0);

	/* Search page */
	page_search = GTK_WIDGET(gtk_builder_get_object(builder, "page_search"));

	/* The selector */
	selector = BLUETOOTH_CHOOSER (gtk_builder_get_object (builder, "selector"));
	g_object_set (selector,
		      "device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
		      NULL);

	/* Setup page */
	page_setup = GTK_WIDGET(gtk_builder_get_object(builder, "page_setup"));
	label_setup = GTK_WIDGET(gtk_builder_get_object(builder, "label_setup"));
	label_passkey_help = GTK_WIDGET(gtk_builder_get_object(builder, "label_passkey_help"));
	label_passkey = GTK_WIDGET(gtk_builder_get_object(builder, "label_passkey"));

	/* Summary page */
	page_summary = GTK_WIDGET(gtk_builder_get_object(builder, "page_summary"));

	/* Set page icons (named icons not supported by Glade) */
	pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
					   "bluetooth", 32, 0, NULL);
	gtk_assistant_set_page_header_image (assistant, page_intro, pixbuf);
	gtk_assistant_set_page_header_image (assistant, page_search, pixbuf);
	gtk_assistant_set_page_header_image (assistant, page_setup, pixbuf);
	gtk_assistant_set_page_header_image (assistant, page_summary, pixbuf);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);

	/* Passkey dialog */
	passkey_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "passkey_dialog"));
	radio_auto = GTK_WIDGET(gtk_builder_get_object(builder, "radio_auto"));
	radio_0000 = GTK_WIDGET(gtk_builder_get_object(builder, "radio_0000"));
	radio_1111 = GTK_WIDGET(gtk_builder_get_object(builder, "radio_1111"));
	radio_1234 = GTK_WIDGET(gtk_builder_get_object(builder, "radio_1234"));
	radio_custom = GTK_WIDGET(gtk_builder_get_object(builder, "radio_custom"));
	entry_custom = GTK_WIDGET(gtk_builder_get_object(builder, "entry_custom"));

	g_object_set_data (G_OBJECT (radio_auto), "pin", "");
	g_object_set_data (G_OBJECT (radio_0000), "pin", "0000");
	g_object_set_data (G_OBJECT (radio_1111), "pin", "1111");
	g_object_set_data (G_OBJECT (radio_1234), "pin", "1234");
	g_object_set_data (G_OBJECT (radio_custom), "entry", entry_custom);

	gtk_builder_connect_signals(builder, NULL);

	gtk_widget_show (GTK_WIDGET(assistant));

	gtk_assistant_update_buttons_state(GTK_ASSISTANT(assistant));

	return GTK_WIDGET(assistant);
}

static UniqueResponse
message_received_cb (UniqueApp         *app,
                     int                command,
                     UniqueMessageData *message_data,
                     guint              time_,
                     gpointer           user_data)
{
        gtk_window_present (GTK_WINDOW (user_data));

        return UNIQUE_RESPONSE_OK;
}

static GOptionEntry options[] = {
	{ NULL },
};

int main(int argc, char *argv[])
{
	UniqueApp *app;
	GtkWidget *window;
	GError *error = NULL;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	if (gtk_init_with_args(&argc, &argv, NULL,
				options, GETTEXT_PACKAGE, &error) == FALSE) {
		if (error) {
			g_print("%s\n", error->message);
			g_error_free(error);
		} else
			g_print("An unknown error occurred\n");

		return 1;
	}

	app = unique_app_new ("org.gnome.Bluetooth.wizard", NULL);
	if (unique_app_is_running (app)) {
		unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
		return 0;
	}

	gtk_window_set_default_icon_name("bluetooth");

	target_pincode = g_strdup_printf("%d", g_random_int_range(100000, 999999));

	client = bluetooth_client_new();

	agent = bluetooth_agent_new();

	bluetooth_agent_set_pincode_func(agent, pincode_callback, NULL);
	bluetooth_agent_set_display_func(agent, display_callback, NULL);
	bluetooth_agent_set_cancel_func(agent, cancel_callback, NULL);

	bluetooth_agent_setup(agent, AGENT_PATH);

	window = create_wizard();
	if (window == NULL)
		return 1;
	window_assistant = window;

	g_signal_connect (app, "message-received",
			  G_CALLBACK (message_received_cb), window);

	gtk_main();

	g_object_unref(agent);

	g_object_unref(client);

	g_object_unref(app);

	return 0;
}

