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

#include "helper.h"

#define AGENT_PATH "/org/bluez/agent/wizard"
#define PIN_CODE_DB "pin-code-database.txt"

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

static GtkWidget *window_assistant = NULL;
static GtkWidget *page_search = NULL;
static GtkWidget *passkey_button = NULL;
static GtkWidget *page_setup = NULL;
static GtkWidget *page_summary = NULL;

static GtkWidget *label_setup = NULL;
static GtkWidget *label_passkey = NULL;
static GtkWidget *label_passkey_help = NULL;

static BluetoothChooser *selector = NULL;

#define TYPE_IS(x, r) {				\
	if (g_str_equal(type, x)) return r;	\
}

static guint string_to_type(const char *type)
{
	TYPE_IS ("any", BLUETOOTH_TYPE_ANY);
	TYPE_IS ("mouse", BLUETOOTH_TYPE_MOUSE);
	TYPE_IS ("keyboard", BLUETOOTH_TYPE_KEYBOARD);
	TYPE_IS ("headset", BLUETOOTH_TYPE_HEADSET);
	TYPE_IS ("headphones", BLUETOOTH_TYPE_HEADPHONES);
	TYPE_IS ("audio", BLUETOOTH_TYPE_OTHER_AUDIO);
	TYPE_IS ("printer", BLUETOOTH_TYPE_PRINTER);
	TYPE_IS ("network", BLUETOOTH_TYPE_NETWORK);

	g_warning ("unhandled type '%s'", type);
	return BLUETOOTH_TYPE_ANY;
}

static char *set_pincode_for_device(guint type, const char *address, const char *name)
{
	char *contents, **lines;
	char *ret_pin = NULL;
	guint i;

	/* Load the PIN database and split it in lines */
	if (!g_file_get_contents(PIN_CODE_DB, &contents, NULL, NULL)) {
		char *filename;

		filename = g_build_filename(PKGDATADIR, PIN_CODE_DB, NULL);
		if (!g_file_get_contents(filename, &contents, NULL, NULL)) {
			g_warning("Could not load "PIN_CODE_DB);
			g_free (filename);
			return NULL;
		}
		g_free (filename);
	}

	lines = g_strsplit(contents, "\n", -1);
	g_free (contents);
	if (lines == NULL) {
		g_warning("Could not parse "PIN_CODE_DB);
		return NULL;
	}

	/* And now process each line */
	for (i = 0; lines[i] != NULL; i++) {
		char **items;
		guint ltype;
		const char *laddress, *lname, *lpin;

		/* Ignore comments and empty lines */
		if (lines[i][0] == '#' || lines[i][0] == '\0')
			continue;

		items = g_strsplit(lines[i], "\t", 4);
		ltype = string_to_type(items[0]);

		if (ltype != BLUETOOTH_TYPE_ANY && ltype != type) {
			g_strfreev (items);
			continue;
		}
		laddress = items[1];
		lname = items[2];
		lpin = items[3];

		/* If we have an address, does the OUI prefix match? */
		if (strlen(laddress) > 0 && g_str_has_prefix(address, laddress) == FALSE) {
			g_strfreev (items);
			continue;
		}

		/* If we have a name, does it match? */
		if (strlen(lname) > 0 && g_str_equal(name, lname) == FALSE) {
			g_strfreev (items);
			continue;
		}

		/* Everything matches, we have a pincode */
		ret_pin = g_strdup(lpin);
		g_strfreev(items);
		break;
	}

	g_strfreev(lines);
	return ret_pin;
}

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

	if (user_pincode != NULL && strlen(user_pincode) == 4) {
		pincode = g_strdup (user_pincode);
	} else {
		pincode = set_pincode_for_device(target_type, target_address, target_name);
		if (pincode == NULL)
			pincode = g_strdup(target_pincode);
	}

	gtk_label_set_markup(GTK_LABEL(label_passkey_help), _("Please enter the following passkey:"));
	set_large_label (GTK_LABEL (label_passkey), pincode);

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

	if (target_ssp == FALSE)
		text = g_strdup_printf(_("Pairing with %s canceled"),
								target_name);
	else
		text = g_strdup_printf(_("Pairing with %s finished"),
								target_name);

	gtk_label_set_markup(GTK_LABEL(label_setup), text);

	g_free(text);

	gtk_label_set_markup(GTK_LABEL(label_passkey_help), NULL);
	gtk_label_set_markup(GTK_LABEL(label_passkey), NULL);

	dbus_g_method_return(context);

	return TRUE;
}

static void connect_callback(gpointer user_data)
{
}

static void create_callback(const char *path, gpointer user_data)
{
	GtkAssistant *assistant = user_data;
	gboolean complete = FALSE;
	gchar *text;

	if (path != NULL) {
		gint page;

		text = g_strdup_printf(_("Successfully paired with %s"),
								target_name);

		page = gtk_assistant_get_current_page(assistant);
		if (page > 0)
			gtk_assistant_set_current_page(assistant, page + 1);

		if (target_type == BLUETOOTH_TYPE_KEYBOARD ||
					target_type == BLUETOOTH_TYPE_MOUSE) {
			bluetooth_client_set_trusted(client, path, TRUE);
			bluetooth_client_connect_input(client, path,
						connect_callback, assistant);
		}

		complete = TRUE;
	} else {
		text = g_strdup_printf(_("Pairing with %s failed"),
								target_name);
	}

	gtk_label_set_markup(GTK_LABEL(label_setup), text);
	gtk_label_set_markup(GTK_LABEL(label_passkey_help), NULL);
	gtk_label_set_markup(GTK_LABEL(label_passkey), NULL);

	g_free(text);

	gtk_assistant_set_page_complete(assistant, page_setup, complete);
}

static void close_callback(GtkWidget *assistant, gpointer data)
{
	gtk_widget_destroy(assistant);

	gtk_main_quit();
}

static void prepare_callback(GtkWidget *assistant,
					GtkWidget *page, gpointer data)
{
	gboolean complete = TRUE;
	const char *path = AGENT_PATH;

	if (page == page_search) {
		gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), passkey_button);
		complete = set_page_search_complete ();
		bluetooth_client_start_discovery(client);
	} else {
		if (gtk_widget_get_parent (passkey_button) != NULL) {
			g_object_ref (passkey_button);
			gtk_assistant_remove_action_widget (GTK_ASSISTANT (assistant), passkey_button);
		}
		bluetooth_client_stop_discovery(client);
	}

	if (page == page_setup) {
		gchar *text, *markup, *address, *name;
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

		text = g_strdup_printf(_("Connecting to %s now ..."),
								target_name);
		markup = g_strdup_printf("%s\n\n", text);
		g_free(text);

		gtk_label_set_markup(GTK_LABEL(label_setup), markup);

		g_free(markup);

		complete = FALSE;

		g_object_ref(agent);

		if (target_type == BLUETOOTH_TYPE_MOUSE)
			path = NULL;

		/* Sony PlayStation 3 Remote Control */
		if (g_str_equal(target_name, "BD Remote Control") == TRUE &&
					(g_str_has_prefix(target_address,
							"00:19:C1:") == TRUE ||
					g_str_has_prefix(target_address,
							"00:1E:3D:") == TRUE))
			path = NULL;

		bluetooth_client_create_device(client, target_address,
					path, create_callback, assistant);
	}

	gtk_assistant_set_page_complete(GTK_ASSISTANT(assistant),
							page, complete);
}

static GtkWidget *create_vbox(GtkWidget *assistant, GtkAssistantPageType type,
				const gchar *title, const gchar *section)
{
	GtkWidget *vbox;
	GtkWidget *label;
	GdkPixbuf *pixbuf;
	gchar *str;

	vbox = gtk_vbox_new(FALSE, 6);

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);
	gtk_assistant_append_page(GTK_ASSISTANT(assistant), vbox);
	gtk_assistant_set_page_type(GTK_ASSISTANT(assistant), vbox, type);
	gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), vbox, title);

	pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
						"bluetooth", 32, 0, NULL);
	gtk_assistant_set_page_header_image(GTK_ASSISTANT(assistant),
								vbox, pixbuf);
	g_object_unref(pixbuf);

	if (section) {
		label = gtk_label_new(NULL);
		str = g_strdup_printf("<b>%s</b>\n", section);
		gtk_label_set_markup(GTK_LABEL(label), str);
		g_free(str);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	}

	return vbox;
}

static void create_intro(GtkWidget *assistant)
{
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *combo;
	GtkTreeModel *model;
	GtkCellRenderer *renderer;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_INTRO,
			_("Introduction"),
			_("Welcome to the Bluetooth device setup wizard"));

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("The device wizard will "
				"walk you through the process of configuring "
				"Bluetooth enabled devices for use with this "
				"computer.\n\n"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

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

	if (gtk_tree_model_iter_n_children(model, NULL) > 1)
		gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);
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

static gboolean entry_custom_event(GtkWidget *entry,
					GdkEventKey *event, GtkWidget *box)
{
	if (event->length == 0)
		return FALSE;

	if ((event->keyval >= GDK_0 && event->keyval <= GDK_9) ||
	    (event->keyval >= GDK_KP_0 && event->keyval <= GDK_KP_9))
		return FALSE;

	return TRUE;
}

static void entry_custom_changed(GtkWidget *entry, GtkWidget *dialog)
{
	user_pincode = g_strdup (gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_ACCEPT,
					   gtk_entry_get_text_length (GTK_ENTRY (entry)) >= 1);
}

static void toggle_set_sensitive(GtkWidget *button, GtkWidget *widget)
{
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	gtk_widget_set_sensitive(widget, active);
}

static void set_user_pincode(GtkWidget *button, GtkWidget *dialog)
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
			set_user_pincode (radio, NULL);
			return;
		}

		pin = g_object_get_data (G_OBJECT (button), "pin");
		entry = g_object_get_data (G_OBJECT (button), "entry");

		if (entry != NULL) {
			g_free (user_pincode);
			user_pincode = g_strdup (gtk_entry_get_text(entry));
			gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
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

static void device_selected_name_cb (GObject *object,
				     GParamSpec *spec,
				     gpointer user_data)
{
	set_page_search_complete ();
}

static void select_device_changed(BluetoothChooser *selector,
				  gchar *address,
				  gpointer user_data)
{
	set_page_search_complete ();
}

static void
passkey_option_button_clicked (GtkButton *button, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *radio_auto;
	GtkWidget *radio_fixed;
	GtkWidget *align_fixed;
	GtkWidget *vbox_fixed;
	GtkWidget *radio;
	GtkWidget *radio_0000;
	GtkWidget *radio_1111;
	GtkWidget *radio_1234;
	GtkWidget *hbox_custom;
	GtkWidget *radio_custom;
	GtkWidget *entry_custom;
	char *oldpin;

	oldpin = user_pincode;
	user_pincode = NULL;

	dialog = gtk_dialog_new_with_buttons (_("Passkey options"),
					      GTK_WINDOW (data),
					      GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_REJECT,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_ACCEPT,
					      NULL);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	radio_auto = gtk_radio_button_new_with_mnemonic(NULL,
					_("_Automatic passkey selection"));
	gtk_container_add(GTK_CONTAINER(vbox), radio_auto);

	radio_fixed = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio_auto), _("Use _fixed passkey:"));
	gtk_container_add(GTK_CONTAINER(vbox), radio_fixed);

	align_fixed = gtk_alignment_new(0, 0, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align_fixed), 0, 0, 24, 0);
	gtk_container_add(GTK_CONTAINER(vbox), align_fixed);
	vbox_fixed = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align_fixed), vbox_fixed);

	radio_0000 = gtk_radio_button_new_with_label(NULL, _("'0000' (most headsets, mice and GPS devices)"));
	gtk_container_add(GTK_CONTAINER(vbox_fixed), radio_0000);

	radio_1111 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_0000), _("'1111'"));
	gtk_container_add(GTK_CONTAINER(vbox_fixed), radio_1111);

	radio_1234 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_0000), _("'1234'"));
	gtk_container_add(GTK_CONTAINER(vbox_fixed), radio_1234);

	hbox_custom = gtk_hbox_new(FALSE, 6);
	radio_custom = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_0000), _("Custom passkey code:"));
	entry_custom = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry_custom), 20);
	gtk_entry_set_width_chars(GTK_ENTRY(entry_custom), 20);
	g_signal_connect (entry_custom, "key-press-event",
			  G_CALLBACK (entry_custom_event), NULL);
	g_signal_connect (entry_custom, "changed",
			  G_CALLBACK (entry_custom_changed), dialog);
	gtk_box_pack_start(GTK_BOX(hbox_custom), radio_custom,
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_custom), entry_custom,
			FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox_fixed), hbox_custom);

	toggle_set_sensitive(radio_fixed, vbox_fixed);
	g_signal_connect(radio_fixed, "toggled",
			G_CALLBACK(toggle_set_sensitive), vbox_fixed);
	toggle_set_sensitive(radio_custom, entry_custom);
	g_signal_connect(radio_custom, "toggled",
			G_CALLBACK(toggle_set_sensitive), entry_custom);

	/* When reopening, try to guess where the pincode was set */
	radio = NULL;
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

	if (radio != radio_auto)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_fixed), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	if (radio == radio_custom)
		gtk_entry_set_text (GTK_ENTRY (entry_custom), oldpin);

	g_object_set_data (G_OBJECT (radio_auto), "pin", "");
	g_object_set_data (G_OBJECT (radio_fixed), "button", radio_0000);
	g_object_set_data (G_OBJECT (radio_0000), "pin", "0000");
	g_object_set_data (G_OBJECT (radio_1111), "pin", "1111");
	g_object_set_data (G_OBJECT (radio_1234), "pin", "1234");
	g_object_set_data (G_OBJECT (radio_custom), "entry", entry_custom);

	g_signal_connect(radio_auto, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);
	g_signal_connect(radio_fixed, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);
	g_signal_connect(radio_0000, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);
	g_signal_connect(radio_1111, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);
	g_signal_connect(radio_1234, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);
	g_signal_connect(radio_custom, "toggled",
			 G_CALLBACK(set_user_pincode), dialog);

	gtk_widget_show_all (vbox);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
		g_free (user_pincode);
		user_pincode = oldpin;
	} else {
		g_free (oldpin);
	}

	gtk_widget_destroy (dialog);
}

static void create_search(GtkWidget *assistant)
{
	GtkWidget *vbox;
	GtkWidget *button;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_CONTENT, _("Device search"), NULL);

	/* The selector */
	selector = BLUETOOTH_CHOOSER (bluetooth_chooser_new(_("Select the device you want to setup")));
	gtk_container_set_border_width(GTK_CONTAINER(selector), 5);
	gtk_widget_show(GTK_WIDGET (selector));
	g_object_set(selector,
		     "show-search", TRUE,
		     "show-device-type", TRUE,
		     "show-device-category", FALSE,
		     "device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
		     NULL);

	g_signal_connect(selector, "selected-device-changed",
			 G_CALLBACK(select_device_changed), assistant);
	g_signal_connect(selector, "notify::device-selected-name",
			 G_CALLBACK(device_selected_name_cb), assistant);

	gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET (selector));

	page_search = vbox;

	button = gtk_button_new_with_mnemonic (_("Passkey _options..."));
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (passkey_option_button_clicked), assistant);
	gtk_widget_show (button);

	passkey_button = button;
}

static void create_setup(GtkWidget *assistant)
{
	GtkWidget *vbox;
	GtkWidget *label;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_CONTENT,
				_("Device setup"),
				_("Setting up new device"));

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
	label_setup = label;

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
	label_passkey_help = label;

	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
	label_passkey = label;

	page_setup = vbox;
}

static void create_summary(GtkWidget *assistant)
{
	GtkWidget *vbox;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_SUMMARY,
				_("Summary"),
				_("Successfully configured new device"));

	page_summary = vbox;
}

static GtkWidget *create_wizard(void)
{
	GtkWidget *assistant;

	assistant = gtk_assistant_new();
	gtk_window_set_title(GTK_WINDOW(assistant), _("Bluetooth Device Wizard"));
	gtk_window_set_position(GTK_WINDOW(assistant), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(assistant), 440, 440);

	create_intro(assistant);
	//create_type(assistant);
	create_search(assistant);
	create_setup(assistant);
	create_summary(assistant);

	g_signal_connect(G_OBJECT(assistant), "close",
					G_CALLBACK(close_callback), NULL);
	g_signal_connect(G_OBJECT(assistant), "cancel",
					G_CALLBACK(close_callback), NULL);
	g_signal_connect(G_OBJECT(assistant), "prepare",
					G_CALLBACK(prepare_callback), NULL);

	gtk_widget_show_all(assistant);

	gtk_assistant_update_buttons_state(GTK_ASSISTANT(assistant));

	return assistant;
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
	window_assistant = window;

	g_signal_connect (app, "message-received",
			  G_CALLBACK (message_received_cb), window);

	gtk_main();

	g_object_unref(agent);

	g_object_unref(client);

	g_object_unref(app);

	return 0;
}
