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

#include <bluetooth-instance.h>
#include <bluetooth-client.h>
#include <bluetooth-agent.h>

#include "helper.h"

#define AGENT_PATH "/org/bluez/agent/wizard"
#define PIN_CODE_DB "pin-code-database.txt"

static BluetoothClient *client;
static BluetoothAgent *agent;

static gchar *target_address = NULL;
static gchar *target_name = NULL;
static gchar *target_pincode = NULL;
static guint target_type = BLUETOOTH_TYPE_ANY;
static gboolean target_ssp = FALSE;

/* NULL means automatic, anything else is a pincode specified by the user */
static const gchar *user_pincode = NULL;
static const gchar *last_fixed_pincode = "0000";

static GtkWidget *window_assistant = NULL;
static GtkWidget *page_search = NULL;
static GtkWidget *page_setup = NULL;
static GtkWidget *page_summary = NULL;

static GtkWidget *label_setup = NULL;
static GtkWidget *label_passkey = NULL;

static GtkTreeSelection *search_selection = NULL;

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

static gboolean pincode_callback(DBusGMethodInvocation *context,
					DBusGProxy *device, gpointer user_data)
{
	char *pincode;
	gchar *text;

	if (user_pincode != NULL && strlen(user_pincode) == 4) {
		pincode = g_strdup (user_pincode);
	} else {
		pincode = set_pincode_for_device(target_type, target_address, target_name);
		if (pincode == NULL)
			pincode = g_strdup(target_pincode);
	}

	text = g_strdup_printf(_("Please enter the following PIN code: %s"),
								pincode);
	gtk_label_set_markup(GTK_LABEL(label_passkey), text);
	g_free(text);

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
	done = g_strnfill(entered, '*');

	text = g_strdup_printf(_("Please enter the following passkey: %s%s"),
							done, code + entered);
	gtk_label_set_markup(GTK_LABEL(label_passkey), text);
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
	} else
		text = g_strdup_printf(_("Pairing with %s failed"),
								target_name);

	gtk_label_set_markup(GTK_LABEL(label_setup), text);
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
		complete = gtk_tree_selection_get_selected(search_selection,
								NULL, NULL);
		bluetooth_client_start_discovery(client);
	} else
		bluetooth_client_stop_discovery(client);

	if (page == page_setup) {
		GtkTreeModel *model;
		GtkTreeIter iter;
		gchar *text, *markup, *address, *name;
		guint type;

		/* Get the info about the device now,
		 * we can't get here without a valid selection */
		if (gtk_tree_selection_get_selected(search_selection,
						&model, &iter) == FALSE)
			g_assert_not_reached();

		gtk_tree_model_get(model, &iter,
					BLUETOOTH_COLUMN_ADDRESS, &address,
					BLUETOOTH_COLUMN_ALIAS, &name,
					BLUETOOTH_COLUMN_TYPE, &type, -1);

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
	str = g_strdup_printf(" %s", title);
	gtk_assistant_set_page_title(GTK_ASSISTANT(assistant), vbox, str);
	g_free(str);

	//pixbuf = gtk_widget_render_icon(GTK_WIDGET(assistant),
	//		GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG, NULL);

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

#if 0
static void create_type(GtkWidget *assistant)
{
	GtkWidget *vbox;
	GtkWidget *button;
	GSList *group = NULL;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_CONTENT,
			_("Device type"),
			_("Select the type of device you want to setup"));

	button = gtk_radio_button_new_with_label(group, _("Mouse"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(group, _("Keyboard"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(group, _("Mobile phone"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(group, _("Printer"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	/* translators: a hands-free headset, a combination of a single speaker with a microphone */
	button = gtk_radio_button_new_with_label(group, _("Headset"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(group, _("Any device"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
}
#endif

static void set_page_search_complete(void)
{
	int selected;
	gboolean complete;

	selected = gtk_tree_selection_count_selected_rows (search_selection);
	complete = (selected != 0 &&
			(user_pincode == NULL || strlen(user_pincode) == 4));

	gtk_assistant_set_page_complete(GTK_ASSISTANT (window_assistant),
					page_search, complete);
}

static void select_callback(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean selected;

	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (selected == TRUE) {
		gboolean paired = FALSE;

		gtk_tree_model_get(model, &iter,
				BLUETOOTH_COLUMN_PAIRED, &paired, -1);

		if (paired == TRUE)
			selected = FALSE;
	}

	set_page_search_complete();
}

static gboolean device_filter(GtkTreeModel *model,
					GtkTreeIter *iter, gpointer user_data)
{
	gboolean paired;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PAIRED, &paired, -1);

	return (paired == TRUE) ? FALSE : TRUE;
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

static void entry_custom_changed(GtkWidget *entry, gpointer user_data)
{
	user_pincode = gtk_entry_get_text(GTK_ENTRY(entry));
	set_page_search_complete();
}

static void toggle_set_sensitive(GtkWidget *button, GtkWidget *widget)
{
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	gtk_widget_set_sensitive(widget, active);
}

static void set_user_pincode(GtkWidget *button, const gchar *pincode)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		return;

	if (pincode == NULL && user_pincode != NULL)
		last_fixed_pincode = user_pincode;

	user_pincode = pincode;
	set_page_search_complete();
}

static void set_from_last_fixed_pincode(GtkWidget *button, gpointer user_data)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		return;

	user_pincode = last_fixed_pincode;
	set_page_search_complete();
}

static void create_search(GtkWidget *assistant)
{
	GtkWidget *vbox;
	GtkWidget *scrolled;
	GtkWidget *tree;
	GtkTreeModel *model;
	GtkTreeModel *sorted;
	GtkTreeSelection *selection;
	GtkWidget *radio_auto;
	GtkWidget *radio_fixed;
	GtkWidget *align_fixed;
	GtkWidget *vbox_fixed;
	GtkWidget *radio_0000;
	GtkWidget *radio_1111;
	GtkWidget *radio_1234;
	GtkWidget *hbox_custom;
	GtkWidget *radio_custom;
	GtkWidget *entry_custom;

	vbox = create_vbox(assistant, GTK_ASSISTANT_PAGE_CONTENT,
				_("Device search"),
				_("Select the device you want to setup"));

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
							GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(vbox), scrolled);

	model = bluetooth_client_get_device_filter_model(client,
					NULL, device_filter, NULL, NULL);
	sorted = gtk_tree_model_sort_new_with_model(model);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sorted),
				BLUETOOTH_COLUMN_RSSI, GTK_SORT_DESCENDING);
	tree = create_tree(sorted, TRUE);
	g_object_unref(sorted);
	g_object_unref(model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(select_callback), assistant);
	search_selection = selection;

	gtk_container_add(GTK_CONTAINER(scrolled), tree);

	radio_auto = gtk_radio_button_new_with_mnemonic(NULL,
					_("_Automatic PIN code selection"));
	gtk_container_add(GTK_CONTAINER(vbox), radio_auto);

	radio_fixed = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio_auto), _("Use _fixed PIN code:"));
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
	radio_custom = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_0000), _("Custom PIN code:"));
	entry_custom = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry_custom), 4);
	gtk_entry_set_width_chars(GTK_ENTRY(entry_custom), 4);
        g_signal_connect (entry_custom, "key-press-event",
			  G_CALLBACK (entry_custom_event), NULL);
        g_signal_connect (entry_custom, "changed",
			  G_CALLBACK (entry_custom_changed), NULL);
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

	g_signal_connect(radio_auto, "toggled",
			G_CALLBACK(set_user_pincode), NULL);
	g_signal_connect(radio_fixed, "toggled",
			G_CALLBACK(set_from_last_fixed_pincode), NULL);
	g_signal_connect(radio_0000, "toggled",
			G_CALLBACK(set_user_pincode), "0000");
	g_signal_connect(radio_1111, "toggled",
			G_CALLBACK(set_user_pincode), "1111");
	g_signal_connect(radio_1234, "toggled",
			G_CALLBACK(set_user_pincode), "1234");
        g_signal_connect_swapped (radio_custom, "toggled",
			  G_CALLBACK (entry_custom_changed), entry_custom);

	page_search = vbox;
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

static GOptionEntry options[] = {
	{ NULL },
};

int main(int argc, char *argv[])
{
	BluetoothInstance *instance;
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

	instance = bluetooth_instance_new("wizard");
	if (instance == NULL)
		return 0;

	gtk_window_set_default_icon_name("bluetooth");

	target_pincode = g_strdup_printf("%d", g_random_int_range(1000, 9999));

	client = bluetooth_client_new();

	agent = bluetooth_agent_new();

	bluetooth_agent_set_pincode_func(agent, pincode_callback, NULL);
	bluetooth_agent_set_display_func(agent, display_callback, NULL);
	bluetooth_agent_set_cancel_func(agent, cancel_callback, NULL);

	bluetooth_agent_setup(agent, AGENT_PATH);

	window = create_wizard();
	window_assistant = window;

	bluetooth_instance_set_window(instance, GTK_WINDOW(window));

	gtk_main();

	g_object_unref(agent);

	g_object_unref(client);

	g_object_unref(instance);

	return 0;
}
