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

#include <gconf/gconf-client.h>
#include <unique/uniqueapp.h>

#include <bluetooth-client.h>
#include <bluetooth-chooser.h>
#include <bluetooth-killswitch.h>

#include "notify.h"
#include "agent.h"

static BluetoothClient *client;
static GtkTreeModel *devices_model;
static guint num_adapters_present = 0;
static guint num_adapters_powered = 0;

enum {
	ICON_POLICY_NEVER,
	ICON_POLICY_ALWAYS,
	ICON_POLICY_PRESENT,
};

static int icon_policy = ICON_POLICY_PRESENT;

#define PREF_DIR		"/apps/bluetooth-manager"
#define PREF_ICON_POLICY	PREF_DIR "/icon_policy"

static GConfClient* gconf;
static BluetoothKillswitch *killswitch = NULL;

static GtkBuilder *xml = NULL;
static GtkActionGroup *devices_action_group = NULL;
static guint devices_ui_id = 0;

/* Signal callbacks */
void settings_callback(GObject *widget, gpointer user_data);
void browse_callback(GObject *widget, gpointer user_data);
void bluetooth_status_callback (GObject *widget, gpointer user_data);
void wizard_callback(GObject *widget, gpointer user_data);
void sendto_callback(GObject *widget, gpointer user_data);

static void action_set_bold (GtkUIManager *manager, GtkAction *action, const char *path);

void settings_callback(GObject *widget, gpointer user_data)
{
	const char *command = "bluetooth-properties";

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

static void
select_device_changed(BluetoothChooser *sel,
		      char *address,
		      gpointer user_data)
{
	GtkDialog *dialog = user_data;

	gtk_dialog_set_response_sensitive(dialog,
				GTK_RESPONSE_ACCEPT, address != NULL);
}

void browse_callback(GObject *widget, gpointer user_data)
{
	GtkWidget *dialog, *selector, *button, *image;
	char *bdaddr, *cmd;
	int response_id;

	dialog = gtk_dialog_new_with_buttons(_("Select Device to Browse"), NULL,
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
	button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Browse"), GTK_RESPONSE_ACCEPT);
	image = gtk_image_new_from_icon_name (GTK_STOCK_CONNECT, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
					  GTK_RESPONSE_ACCEPT, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 400);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	selector = bluetooth_chooser_new(_("Select device to browse"));
	gtk_container_set_border_width(GTK_CONTAINER(selector), 5);
	gtk_widget_show(selector);
	g_object_set(selector,
		     "show-search", TRUE,
		     "show-device-category", TRUE,
		     "show-device-type", TRUE,
		     NULL);
	g_signal_connect(selector, "selected-device-changed",
			 G_CALLBACK(select_device_changed), dialog);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), selector);
	bluetooth_chooser_start_discovery (BLUETOOTH_CHOOSER (selector));

	bdaddr = NULL;
	response_id = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response_id == GTK_RESPONSE_ACCEPT)
		g_object_get (G_OBJECT (selector), "device-selected", &bdaddr, NULL);

	gtk_widget_destroy (dialog);

	if (response_id != GTK_RESPONSE_ACCEPT)
		return;

	cmd = g_strdup_printf("%s --no-default-window \"obex://[%s]\"",
			      "nautilus", bdaddr);
	g_free (bdaddr);

	if (!g_spawn_command_line_async(cmd, NULL))
		g_printerr("Couldn't execute command: %s\n", cmd);

	g_free (cmd);
}

void sendto_callback(GObject *widget, gpointer user_data)
{
	const char *command = "bluetooth-sendto";

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

void wizard_callback(GObject *widget, gpointer user_data)
{
	const char *command = "bluetooth-wizard";

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

void bluetooth_status_callback (GObject *widget, gpointer user_data)
{
	GObject *object;
	gboolean active;

	object = gtk_builder_get_object (xml, "killswitch-label");
	active = GPOINTER_TO_INT (g_object_get_data (object, "bt-active"));
	active = !active;
	bluetooth_killswitch_set_state (killswitch,
					active ? KILLSWITCH_STATE_NOT_KILLED : KILLSWITCH_STATE_KILLED);
	g_object_set_data (object, "bt-active", GINT_TO_POINTER (active));
}

static gboolean program_available(const char *program)
{
	gchar *path;

	path = g_find_program_in_path(program);
	if (path == NULL)
		return FALSE;

	g_free(path);

	return TRUE;
}

static void popup_callback(GObject *widget, guint button,
				guint activate_time, gpointer user_data)
{
	GtkWidget *menu = user_data;

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			gtk_status_icon_position_menu,
			GTK_STATUS_ICON(widget), button, activate_time);
}

static void activate_callback(GObject *widget, gpointer user_data)
{
	guint32 activate_time = gtk_get_current_event_time();

	if (query_blinking() == TRUE) {
		show_agents();
		return;
	}

	popup_callback(widget, 0, activate_time, user_data);
}

static void
killswitch_state_changed (BluetoothKillswitch *killswitch, KillswitchState state)
{
	GObject *object;
	gboolean sensitive = TRUE;
	gboolean bstate = FALSE;
	const char *label, *status_label;

	if (state == KILLSWITCH_STATE_UNKNOWN || state == KILLSWITCH_STATE_MIXED) {
		sensitive = FALSE;
		label = NULL;
		status_label = N_("Bluetooth: Unknown");
	} else if (state == KILLSWITCH_STATE_KILLED) {
		label = N_("Turn On Bluetooth");
		status_label = N_("Bluetooth: Off");
		bstate = FALSE;
	} else if (state == KILLSWITCH_STATE_NOT_KILLED) {
		label = N_("Turn Off Bluetooth");
		status_label = N_("Bluetooth: On");
		bstate = TRUE;
	} else {
		g_assert_not_reached ();
	}

	object = gtk_builder_get_object (xml, "killswitch-label");
	gtk_action_set_label (GTK_ACTION (object), _(status_label));

	object = gtk_builder_get_object (xml, "killswitch");
	gtk_action_set_visible (GTK_ACTION (object), sensitive);
	gtk_action_set_label (GTK_ACTION (object), _(label));

	if (sensitive != FALSE) {
		gtk_action_set_label (GTK_ACTION (object), _(label));
		g_object_set_data (object, "bt-active", GINT_TO_POINTER (bstate));
	}

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");
	gtk_ui_manager_ensure_update (GTK_UI_MANAGER (object));
}

static GtkWidget *create_popupmenu(void)
{
	GObject *object;

	xml = gtk_builder_new ();
	if (gtk_builder_add_from_file (xml, "popup-menu.ui", NULL) == 0)
		gtk_builder_add_from_file (xml, PKGDATADIR "/popup-menu.ui", NULL);

	gtk_builder_connect_signals (xml, NULL);

	if (killswitch != NULL) {
		GObject *object;

		object = gtk_builder_get_object (xml, "killswitch-label");
		gtk_action_set_visible (GTK_ACTION (object), TRUE);
	}

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");
	devices_ui_id = gtk_ui_manager_new_merge_id (GTK_UI_MANAGER (object));
	action_set_bold (GTK_UI_MANAGER (object),
			 GTK_ACTION (gtk_builder_get_object (xml, "devices-label")),
			 "/bluetooth-applet-popup/devices-label");

	return GTK_WIDGET (gtk_builder_get_object (xml, "bluetooth-applet-popup"));
}

static void
update_menu_items (void)
{
	gboolean enabled;
	GObject *object;

	if (num_adapters_present == 0)
		enabled = FALSE;
	else
		enabled = (num_adapters_present - num_adapters_powered) >= 0;


	object = gtk_builder_get_object (xml, "send-file");
	gtk_action_set_sensitive (GTK_ACTION (object),
				  enabled &&
				  (program_available ("obex-data-server")
				   || program_available ("obexd")));

	object = gtk_builder_get_object (xml, "browse-device");
	gtk_action_set_sensitive (GTK_ACTION (object),
				  enabled && program_available ("nautilus"));

	object = gtk_builder_get_object (xml, "setup-new");
	gtk_action_set_sensitive (GTK_ACTION (object), enabled);
}

static void
update_icon_visibility (void)
{
	if (num_adapters_powered == 0)
		set_icon (FALSE);
	else
		set_icon (TRUE);

	if (icon_policy == ICON_POLICY_NEVER)
		hide_icon();
	else if (icon_policy == ICON_POLICY_ALWAYS)
		show_icon();
	else if (icon_policy == ICON_POLICY_PRESENT) {
		if (num_adapters_present > 0 || killswitch != NULL)
			show_icon();
		else
			hide_icon();
	}
}

static void
on_connect_activate (GtkAction *action, gpointer data)
{
	gboolean connected;
	const char *path;

	connected = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "connected"));
	path = g_object_get_data (G_OBJECT (action), "device-path");

	g_message ("we should %s %s", connected ? "disconnect" : "connect", path);
}

static void
action_set_bold (GtkUIManager *manager, GtkAction *action, const char *path)
{
	GtkWidget *widget;
	char *str;
	const char *label;

	/* That sucks, but otherwise the widget might not exist */
	gtk_ui_manager_ensure_update (manager);

	widget = gtk_ui_manager_get_widget (manager, path);
	g_assert (widget);

	label = gtk_action_get_label (action);
	str = g_strdup_printf ("<b>%s</b>", label);
	gtk_label_set_markup (GTK_LABEL (GTK_BIN (widget)->child), str);
	g_free (str);
}

static void
remove_action_item (GtkAction *action, gpointer data)
{
	gtk_action_group_remove_action (devices_action_group, action);
	g_object_unref (action); //FIXME ?
}

static void
update_device_list (GtkTreeIter *parent)
{
	GObject *object;
	GtkTreeIter iter;
	gboolean cont;
	guint num_devices;
	GList *actions;

	num_devices = 0;

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");

	if (devices_action_group == NULL) {
		devices_action_group = gtk_action_group_new ("devices-action-group");
		gtk_ui_manager_insert_action_group (GTK_UI_MANAGER (object),
						    devices_action_group, -1); 
	}

	if (parent == NULL) {
		/* No default adapter? Remove everything */
		actions = gtk_action_group_list_actions (devices_action_group);
		g_list_foreach (actions, (GFunc) remove_action_item, NULL);
		g_list_free (actions);
		gtk_ui_manager_remove_ui (GTK_UI_MANAGER (object), devices_ui_id);
		goto done;
	}

	/* Get a list of actions, and remove the ones with a
	 * device in the list */
	actions = gtk_action_group_list_actions (devices_action_group);

	cont = gtk_tree_model_iter_children (devices_model, &iter, parent);
	while (cont) {
		GHashTable *table;
		DBusGProxy *proxy;
		const char *name, *address;
		gboolean connected;
		GtkAction *action;

		action = NULL;

		gtk_tree_model_get (devices_model, &iter,
				    BLUETOOTH_COLUMN_PROXY, &proxy,
				    BLUETOOTH_COLUMN_ADDRESS, &address,
				    BLUETOOTH_COLUMN_SERVICES, &table,
				    BLUETOOTH_COLUMN_ALIAS, &name,
				    BLUETOOTH_COLUMN_CONNECTED, &connected,
				    -1);

		if (address != NULL) {
			action = gtk_action_group_get_action (devices_action_group, address);
			if (action)
				actions = g_list_remove (actions, action);
		}

		if (table != NULL && address != NULL && proxy != NULL) {
			if (action == NULL) {
				action = gtk_action_new (address, name, NULL, NULL);

				gtk_action_group_add_action (devices_action_group, action);
				g_object_unref (action);
				gtk_ui_manager_add_ui (GTK_UI_MANAGER (object), devices_ui_id,
						       "/bluetooth-applet-popup/devices-placeholder", address, address,
						       GTK_UI_MANAGER_MENUITEM, FALSE);
				g_signal_connect (G_OBJECT (action), "activate",
						  G_CALLBACK (on_connect_activate), NULL);
			} else {
				gtk_action_set_label (action, name);
			}
			g_object_set_data_full (G_OBJECT (action),
						"connected", GINT_TO_POINTER (connected), NULL);
			g_object_set_data_full (G_OBJECT (action),
						"device-path", g_strdup (dbus_g_proxy_get_path (proxy)), g_free);

			/* And now for the trick of the day */
			if (connected != FALSE) {
				char *path;

				path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s", address);
				action_set_bold (GTK_UI_MANAGER (object), action, path);
				g_free (path);
			}

			num_devices++;
		}
		if (proxy != NULL)
			g_object_unref (proxy);

		cont = gtk_tree_model_iter_next (devices_model, &iter);
	}

	/* Remove the left-over devices */
	g_list_foreach (actions, (GFunc) remove_action_item, NULL);
	g_list_free (actions);

done:
	gtk_ui_manager_ensure_update (GTK_UI_MANAGER (object));

	object = gtk_builder_get_object (xml, "devices-label");
	gtk_action_set_visible (GTK_ACTION (object), num_devices > 0);
}

static void device_changed (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeIter  *_iter,
			     gpointer      data)
{
	GtkTreeIter iter, *default_iter;
	gboolean powered, cont;

	default_iter = NULL;
	num_adapters_present = num_adapters_powered = 0;

	cont = gtk_tree_model_get_iter_first (model, &iter);
	while (cont) {
		gboolean is_default;

		num_adapters_present++;

		gtk_tree_model_get (model, &iter,
				    BLUETOOTH_COLUMN_DEFAULT, &is_default,
				    BLUETOOTH_COLUMN_POWERED, &powered,
				    -1);
		if (powered)
			num_adapters_powered++;
		if (is_default && powered)
			default_iter = gtk_tree_iter_copy (&iter);

		cont = gtk_tree_model_iter_next (model, &iter);
	}

	update_icon_visibility ();
	update_menu_items ();
	update_device_list (default_iter);

	if (default_iter != NULL)
		gtk_tree_iter_free (default_iter);
}

static void device_added(GtkTreeModel *model,
			  GtkTreePath *path,
			  GtkTreeIter *iter,
			  gpointer user_data)
{
	device_changed (model, path, iter, user_data);
}

static void device_removed(GtkTreeModel *model,
			    GtkTreePath *path,
			    gpointer user_data)
{
	device_changed (model, path, NULL, user_data);
}

static GConfEnumStringPair icon_policy_enum_map [] = {
	{ ICON_POLICY_NEVER,	"never"		},
	{ ICON_POLICY_ALWAYS,	"always"	},
	{ ICON_POLICY_PRESENT,	"present"	},
	{ ICON_POLICY_PRESENT,	NULL		},
};

static void gconf_callback(GConfClient *client, guint cnxn_id,
					GConfEntry *entry, gpointer user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value(entry);
	if (value == NULL)
		return;

	if (g_str_equal(entry->key, PREF_ICON_POLICY) == TRUE) {
		const char *str;

		str = gconf_value_get_string(value);
		if (!str)
			return;

		gconf_string_to_enum(icon_policy_enum_map, str, &icon_policy);

		update_icon_visibility();
		return;
	}
}

static GOptionEntry options[] = {
	{ NULL },
};

int main(int argc, char *argv[])
{
	UniqueApp *app;
	GtkStatusIcon *statusicon;
	GtkWidget *menu;
	GError *error = NULL;
	char *str;

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

	app = unique_app_new ("org.gnome.Bluetooth.applet", NULL);
	if (unique_app_is_running (app)) {
		g_warning ("Applet is already running, exiting");
		return 0;
	}

	g_set_application_name(_("Bluetooth Applet"));

	gtk_window_set_default_icon_name("bluetooth");

	killswitch = bluetooth_killswitch_new ();
	if (bluetooth_killswitch_has_killswitches (killswitch) == FALSE) {
		g_object_unref (killswitch);
		killswitch = NULL;
	} else {
		g_signal_connect (G_OBJECT (killswitch), "state-changed",
				  G_CALLBACK (killswitch_state_changed), NULL);
	}

	menu = create_popupmenu();

	client = bluetooth_client_new();

	devices_model = bluetooth_client_get_model(client);

	g_signal_connect(G_OBJECT(devices_model), "row-inserted",
			 G_CALLBACK(device_added), NULL);
	g_signal_connect(G_OBJECT(devices_model), "row-deleted",
			 G_CALLBACK(device_removed), NULL);
	g_signal_connect (G_OBJECT (devices_model), "row-changed",
			  G_CALLBACK (device_changed), NULL);
	/* Set the default */
	device_changed (devices_model, NULL, NULL, NULL);

	gconf = gconf_client_get_default();

	str = gconf_client_get_string(gconf, PREF_ICON_POLICY, NULL);
	if (str) {
		gconf_string_to_enum(icon_policy_enum_map, str, &icon_policy);
		g_free(str);
	}

	gconf_client_add_dir(gconf, PREF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);

	gconf_client_notify_add(gconf, PREF_DIR,
					gconf_callback, NULL, NULL, NULL);

	statusicon = init_notification();

	update_icon_visibility();

	g_signal_connect(statusicon, "activate",
				G_CALLBACK(activate_callback), menu);
	g_signal_connect(statusicon, "popup-menu",
				G_CALLBACK(popup_callback), menu);

	setup_agents();

	gtk_main();

	gtk_widget_destroy(menu);

	g_object_unref(gconf);

	cleanup_agents();

	cleanup_notification();

	g_object_unref(devices_model);

	g_object_unref(client);

	g_object_unref (app);

	return 0;
}
