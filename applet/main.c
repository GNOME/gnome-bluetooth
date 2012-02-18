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

#include <bluetooth-chooser.h>
#include <bluetooth-client.h>
#include <bluetooth-utils.h>

#include "bluetooth-applet.h"
#include "notify.h"
#include "agent.h"

static gboolean option_debug = FALSE;
static BluetoothApplet *applet = NULL;
static gboolean discover_lock = FALSE;

#define GNOMECC			"gnome-control-center"
#define KEYBOARD_PREFS		GNOMECC " keyboard"
#define MOUSE_PREFS		GNOMECC " mouse"
#define SOUND_PREFS		GNOMECC " sound"

enum {
	CONNECTED,
	DISCONNECTED,
	CONNECTING,
	DISCONNECTING
};

static GtkBuilder *xml = NULL;
static GtkActionGroup *devices_action_group = NULL;

/* Signal callbacks */
void settings_callback(GObject *widget, gpointer user_data);
void quit_callback(GObject *widget, gpointer user_data);
void browse_callback(GObject *widget, gpointer user_data);
void bluetooth_status_callback (GObject *widget, gpointer user_data);
void bluetooth_discoverable_callback (GtkToggleAction *toggleaction, gpointer user_data);
void wizard_callback(GObject *widget, gpointer user_data);
void sendto_callback(GObject *widget, gpointer user_data);

static void action_set_bold (GtkUIManager *manager, GtkAction *action, const char *path);
static void update_icon_visibility (void);

void quit_callback(GObject *widget, gpointer user_data)
{
	gtk_main_quit ();
}

void settings_callback(GObject *widget, gpointer user_data)
{
	const char *command = "gnome-control-center bluetooth";

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

static void
mount_finish_cb (GObject *source_object,
		 GAsyncResult *res,
		 gpointer user_data)
{
	GError *error = NULL;

	if (bluetooth_applet_browse_address_finish (applet, res, &error) == FALSE) {
		g_printerr ("Failed to mount OBEX volume: %s", error->message);
		g_error_free (error);
		return;
	}
}

void browse_callback(GObject *widget, gpointer user_data)
{
	const char *address;

	address = g_object_get_data (widget, "address");
	if (address == NULL)
		return;

	bluetooth_applet_browse_address (applet, address,
					 GDK_CURRENT_TIME, mount_finish_cb, NULL);
}

void sendto_callback(GObject *widget, gpointer user_data)
{
	const char *address, *alias;

	address = g_object_get_data (widget, "address");
	alias = g_object_get_data (widget, "alias");

	bluetooth_applet_send_to_address (applet, address, alias);
}

static void keyboard_callback(GObject *widget, gpointer user_data)
{
	const char *command = KEYBOARD_PREFS;

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

static void mouse_callback(GObject *widget, gpointer user_data)
{
	const char *command = MOUSE_PREFS;

	if (!g_spawn_command_line_async(command, NULL))
		g_printerr("Couldn't execute command: %s\n", command);
}

static void sound_callback(GObject *widget, gpointer user_data)
{
	const char *command = SOUND_PREFS;

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

	object = gtk_builder_get_object (xml, "killswitch");
	active = GPOINTER_TO_INT (g_object_get_data (object, "bt-active"));
	active = !active;
	bluetooth_applet_set_killswitch_state (applet,
					active ? BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED : BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED);
	g_object_set_data (object, "bt-active", GINT_TO_POINTER (active));
}

void
bluetooth_discoverable_callback (GtkToggleAction *toggleaction, gpointer user_data)
{
	gboolean discoverable;

	discoverable = gtk_toggle_action_get_active (toggleaction);
	discover_lock = TRUE;
	bluetooth_applet_set_discoverable (applet, discoverable);
	discover_lock = FALSE;
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

	/* Always show the menu, whatever button is pressed.
	 * Agent dialog can be shown from the notification (or should
	 * be shown automatically in the background).
	 */
	popup_callback(widget, 0, activate_time, user_data);
}

static void
killswitch_state_changed (GObject    *gobject,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
	GObject *object;
	BluetoothApplet *applet = BLUETOOTH_APPLET (gobject);
	BluetoothKillswitchState state = bluetooth_applet_get_killswitch_state (applet);
	gboolean sensitive = TRUE;
	gboolean bstate = FALSE;
	const char *label, *status_label;

	if (state == BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER) {
		object = gtk_builder_get_object (xml, "bluetooth-applet-popup");
		gtk_menu_popdown (GTK_MENU (object));
		update_icon_visibility ();
		return;
	}

	if (state == BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED) {
		label = N_("Turn on Bluetooth");
		status_label = N_("Bluetooth: Off");
		bstate = FALSE;
	} else if (state == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED) {
		label = N_("Turn off Bluetooth");
		status_label = N_("Bluetooth: On");
		bstate = TRUE;
	} else if (state == BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED) {
		sensitive = FALSE;
		label = NULL;
		status_label = N_("Bluetooth: Disabled");
	} else {
		g_assert_not_reached ();
	}

	object = gtk_builder_get_object (xml, "killswitch-label");
	gtk_action_set_label (GTK_ACTION (object), _(status_label));
	gtk_action_set_visible (GTK_ACTION (object), TRUE);

	object = gtk_builder_get_object (xml, "killswitch");
	gtk_action_set_visible (GTK_ACTION (object), sensitive);
	gtk_action_set_label (GTK_ACTION (object), _(label));

	if (sensitive != FALSE) {
		gtk_action_set_label (GTK_ACTION (object), _(label));
		g_object_set_data (object, "bt-active", GINT_TO_POINTER (bstate));
	}

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");
	gtk_ui_manager_ensure_update (GTK_UI_MANAGER (object));

	update_icon_visibility ();
}

static GtkWidget *create_popupmenu(void)
{
	GObject *object;
	GError *error = NULL;

	xml = gtk_builder_new ();
	if (gtk_builder_add_from_file (xml, "popup-menu.ui", &error) == 0) {
		if (error->domain == GTK_BUILDER_ERROR) {
			g_warning ("Failed to load popup-menu.ui: %s", error->message);
			g_error_free (error);
			return NULL;
		}
		g_error_free (error);
		error = NULL;
		if (gtk_builder_add_from_file (xml, PKGDATADIR "/popup-menu.ui", &error) == 0) {
			g_warning ("Failed to load popup-menu.ui: %s", error->message);
			g_error_free (error);
			return NULL;
		}
	}

	gtk_builder_connect_signals (xml, NULL);

	if (bluetooth_applet_get_killswitch_state (applet) != BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER) {
		GObject *object;

		object = gtk_builder_get_object (xml, "killswitch-label");
		gtk_action_set_visible (GTK_ACTION (object), TRUE);
	}

	if (option_debug != FALSE) {
		GObject *object;

		object = gtk_builder_get_object (xml, "quit");
		gtk_action_set_visible (GTK_ACTION (object), TRUE);
	}

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");
	devices_action_group = gtk_action_group_new ("devices-action-group");
	gtk_ui_manager_insert_action_group (GTK_UI_MANAGER (object),
					    devices_action_group, -1);

	action_set_bold (GTK_UI_MANAGER (object),
			 GTK_ACTION (gtk_builder_get_object (xml, "devices-label")),
			 "/bluetooth-applet-popup/devices-label");

	return GTK_WIDGET (gtk_builder_get_object (xml, "bluetooth-applet-popup"));
}

static void
update_menu_items (GObject    *gobject,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
	BluetoothApplet *applet = BLUETOOTH_APPLET (gobject);
	gboolean enabled;
	GObject *object;

	enabled = bluetooth_applet_get_show_full_menu (applet);

	object = gtk_builder_get_object (xml, "adapter-action-group");
	gtk_action_group_set_visible (GTK_ACTION_GROUP (object), enabled);
	gtk_action_group_set_visible (devices_action_group, enabled);

	if (enabled == FALSE)
		return;

	gtk_action_group_set_sensitive (GTK_ACTION_GROUP (object), TRUE);
}

static void
update_icon_visibility (void)
{
	gboolean state = bluetooth_applet_get_killswitch_state (applet);
	if (state != BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED)
		set_icon (FALSE);
	else
		set_icon (TRUE);

	if (state != BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER) {
		show_icon ();
		return;
	}
	hide_icon ();
}

static void
update_discoverability (GObject    *gobject,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
	BluetoothApplet *applet = BLUETOOTH_APPLET (gobject);
	gboolean discoverable;
	GObject *object;

	/* Avoid loops from changing the UI */
	if (discover_lock != FALSE)
		return;

	discover_lock = TRUE;

	object = gtk_builder_get_object (xml, "discoverable");

	discoverable = bluetooth_applet_get_discoverable (applet);

	gtk_action_set_visible (GTK_ACTION (object), TRUE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (object), discoverable);

	discover_lock = FALSE;
}

static void
set_device_status_label (const char *address, int connected)
{
	char *action_name, *action_path;
	GtkAction *status;
	GObject *object;
	const char *label;

	g_return_if_fail (address != NULL);

	switch (connected) {
	case DISCONNECTING:
		label = N_("Disconnecting...");
		break;
	case CONNECTING:
		label = N_("Connecting...");
		break;
	case CONNECTED:
		label = N_("Connected");
		break;
	case DISCONNECTED:
		label = N_("Disconnected");
		break;
	default:
		g_assert_not_reached ();
	}

	action_name = g_strdup_printf ("%s-status", address);
	status = gtk_action_group_get_action (devices_action_group, action_name);
	g_free (action_name);

	g_assert (status != NULL);
	gtk_action_set_label (status, _(label));

	object = gtk_builder_get_object (xml, "bluetooth-applet-ui-manager");
	action_path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s/%s-status",
				       address, address);
	action_set_bold (GTK_UI_MANAGER (object), status, action_path);
	g_free (action_path);
}

static void
connection_action_callback (BluetoothApplet *_client,
			    gboolean success,
			    gpointer user_data)
{
	GtkAction *action = (GtkAction *) user_data;
	const char *address;
	int connected;

	/* Revert to the previous state and wait for the
	 * BluetoothApplet to catch up */
	connected = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "connected"));
	if (connected == DISCONNECTING)
		connected = CONNECTED;
	else if (connected == CONNECTING)
		connected = DISCONNECTED;
	else
		return;

	g_object_set_data_full (G_OBJECT (action),
				"connected", GINT_TO_POINTER (connected), NULL);
	address = g_object_get_data (G_OBJECT (action), "address");
	set_device_status_label (address, connected);
}

static void
on_connect_activate (GtkAction *action, gpointer data)
{
	int connected;
	const char *path;
	const char *address;

	connected = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "connected"));
	/* Don't try connecting if something is already in progress */
	if (connected == CONNECTING || connected == DISCONNECTING)
		return;

	path = g_object_get_data (G_OBJECT (action), "device-path");

	if (connected == DISCONNECTED) {
		if (bluetooth_applet_connect_device (applet, path, connection_action_callback, action) != FALSE)
			connected = DISCONNECTING;
	} else if (connected == CONNECTED) {
		if (bluetooth_applet_disconnect_device (applet, path, connection_action_callback, action) != FALSE)
			connected = CONNECTING;
	} else {
		g_assert_not_reached ();
	}

	g_object_set_data_full (G_OBJECT (action),
				"connected", GINT_TO_POINTER (connected), NULL);

	address = g_object_get_data (G_OBJECT (action), "address");
	set_device_status_label (address, connected);
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
	gtk_label_set_markup (GTK_LABEL (gtk_bin_get_child (GTK_BIN (widget))), str);
	g_free (str);
}

static void
remove_action_item (GtkAction *action, GtkUIManager *manager)
{
	guint menu_merge_id;
	GList *actions, *l;

	menu_merge_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (action), "merge-id"));
	gtk_ui_manager_remove_ui (GTK_UI_MANAGER (manager), menu_merge_id);
	actions = gtk_action_group_list_actions (devices_action_group);
	for (l = actions; l != NULL; l = l->next) {
		GtkAction *a = l->data;
		/* Don't remove the top-level action straight away */
		if (g_str_equal (gtk_action_get_name (a), gtk_action_get_name (action)) != FALSE)
			continue;
		/* But remove all the sub-actions for it */
		if (g_str_has_prefix (gtk_action_get_name (a), gtk_action_get_name (action)) != FALSE)
			gtk_action_group_remove_action (devices_action_group, a);
	}
	g_list_free (actions);
	gtk_action_group_remove_action (devices_action_group, action);
}

static char *
escape_label_for_action (const char *alias)
{
	char **tokens, *name;

	if (strchr (alias, '_') == NULL)
		return g_strdup (alias);

	tokens = g_strsplit (alias, "_", -1);
	name = g_strjoinv ("__", tokens);
	g_strfreev (tokens);

	return name;
}

static gboolean
device_has_submenu (BluetoothSimpleDevice *dev)
{
	if (dev->can_connect != FALSE)
		return TRUE;
	if (dev->capabilities != BLUETOOTH_CAPABILITIES_NONE)
		return TRUE;
	return FALSE;
}

static GtkAction *
add_menu_item (const char *address,
	       const char *suffix,
	       const char *label,
	       GtkUIManager *uimanager,
	       guint merge_id,
	       GCallback callback)
{
	GtkAction *action;
	char *action_path, *action_name;

	g_return_val_if_fail (address != NULL, NULL);
	g_return_val_if_fail (suffix != NULL, NULL);
	g_return_val_if_fail (label != NULL, NULL);

	action_name = g_strdup_printf ("%s-%s", address, suffix);
	action = gtk_action_new (action_name, label, NULL, NULL);

	gtk_action_group_add_action (devices_action_group, action);
	g_object_unref (action);

	action_path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s", address);
	gtk_ui_manager_add_ui (uimanager, merge_id,
			       action_path, action_name, action_name,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
	g_free (action_path);

	g_free (action_name);

	if (callback != NULL)
		g_signal_connect (G_OBJECT (action), "activate", callback, NULL);

	g_object_set_data_full (G_OBJECT (action),
				"address", g_strdup (address), g_free);

	return action;
}

static void
add_separator_item (const char *address,
		    const char *suffix,
		    GtkUIManager *uimanager,
		    guint merge_id)
{
	char *action_path, *action_name;

	g_return_if_fail (address != NULL);
	g_return_if_fail (suffix != NULL);

	action_name = g_strdup_printf ("%s-%s", address, suffix);
	action_path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s", address);

	gtk_ui_manager_add_ui (uimanager, merge_id,
			       action_path, action_name, NULL,
			       GTK_UI_MANAGER_SEPARATOR, FALSE);
	g_free (action_path);
	g_free (action_name);
}

static void
update_device_list (BluetoothApplet *applet,
					gpointer user_data)
{
	GtkUIManager *uimanager;
	GList *actions, *devices, *l;
	gboolean has_devices = FALSE;

	uimanager = GTK_UI_MANAGER (gtk_builder_get_object (xml, "bluetooth-applet-ui-manager"));

	devices = bluetooth_applet_get_devices (applet);

	if (devices == NULL) {
		/* No devices? Remove everything */
		actions = gtk_action_group_list_actions (devices_action_group);
		g_list_foreach (actions, (GFunc) remove_action_item, uimanager);
		g_list_free (actions);
		goto done;
	}

	/* Get a list of actions, and we'll remove the ones with a
	 * device in the list. We remove the submenu items first */
	actions = gtk_action_group_list_actions (devices_action_group);
	for (l = actions; l != NULL; l = l->next) {
		if (bluetooth_verify_address (gtk_action_get_name (l->data)) == FALSE)
			l->data = NULL;
	}
	actions = g_list_remove_all (actions, NULL);

	for (l = devices; l != NULL; l = g_list_next (l)) {
		BluetoothSimpleDevice *device = l->data;
		GtkAction *action, *status, *oper;
		char *name;

		if (device_has_submenu (device) == FALSE) {
			g_boxed_free (BLUETOOTH_TYPE_SIMPLE_DEVICE, device);
			continue;
		}

		has_devices = TRUE;

		action = gtk_action_group_get_action (devices_action_group, device->bdaddr);
		oper = NULL;
		status = NULL;
		if (action) {
			char *action_name;

			actions = g_list_remove (actions, action);

			action_name = g_strdup_printf ("%s-status", device->bdaddr);
			status = gtk_action_group_get_action (devices_action_group, action_name);
			g_free (action_name);

			action_name = g_strdup_printf ("%s-action", device->bdaddr);
			oper = gtk_action_group_get_action (devices_action_group, action_name);
			g_free (action_name);
		}

		name = escape_label_for_action (device->alias);

		if (action == NULL) {
			guint menu_merge_id;
			char *action_path;

			/* The menu item with descendants */
			action = gtk_action_new (device->bdaddr, name, NULL, NULL);

			gtk_action_group_add_action (devices_action_group, action);
			g_object_unref (action);
			menu_merge_id = gtk_ui_manager_new_merge_id (uimanager);
			gtk_ui_manager_add_ui (uimanager, menu_merge_id,
					       "/bluetooth-applet-popup/devices-placeholder", device->bdaddr, device->bdaddr,
					       GTK_UI_MANAGER_MENU, FALSE);
			g_object_set_data_full (G_OBJECT (action),
						"merge-id", GUINT_TO_POINTER (menu_merge_id), NULL);

			/* The status menu item */
			status = add_menu_item (device->bdaddr,
						"status",
						device->connected ? _("Connected") : _("Disconnected"),
						uimanager,
						menu_merge_id,
						NULL);
			gtk_action_set_sensitive (status, FALSE);

			if (device->can_connect) {
				action_path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s/%s-status",
							       device->bdaddr, device->bdaddr);
				action_set_bold (uimanager, status, action_path);
				g_free (action_path);
			} else {
				gtk_action_set_visible (status, FALSE);
			}

			/* The connect button */
			oper = add_menu_item (device->bdaddr,
					      "action",
					      device->connected ? _("Disconnect") : _("Connect"),
					      uimanager,
					      menu_merge_id,
					      G_CALLBACK (on_connect_activate));
			if (!device->can_connect)
				gtk_action_set_visible (oper, FALSE);

			add_separator_item (device->bdaddr, "connect-sep", uimanager, menu_merge_id);

			/* The Send to... button */
			if (device->capabilities & BLUETOOTH_CAPABILITIES_OBEX_PUSH) {
				add_menu_item (device->bdaddr,
					       "sendto",
					       _("Send files..."),
					       uimanager,
					       menu_merge_id,
					       G_CALLBACK (sendto_callback));
				g_object_set_data_full (G_OBJECT (action),
							"alias", g_strdup (device->alias), g_free);
			}
			if (device->capabilities & BLUETOOTH_CAPABILITIES_OBEX_FILE_TRANSFER) {
				add_menu_item (device->bdaddr,
					       "browse",
					       _("Browse files..."),
					       uimanager,
					       menu_merge_id,
					       G_CALLBACK (browse_callback));
			}

			add_separator_item (device->bdaddr, "files-sep", uimanager, menu_merge_id);

			if (device->type == BLUETOOTH_TYPE_KEYBOARD && program_available (GNOMECC)) {
				add_menu_item (device->bdaddr,
					       "keyboard",
					       _("Keyboard Settings"),
					       uimanager,
					       menu_merge_id,
					       G_CALLBACK (keyboard_callback));
			}
			if (device->type == BLUETOOTH_TYPE_MOUSE && program_available (GNOMECC)) {
				add_menu_item (device->bdaddr,
					       "mouse",
					       _("Mouse and Touchpad Settings"),
					       uimanager,
					       menu_merge_id,
					       G_CALLBACK (mouse_callback));
			}
			if ((device->type == BLUETOOTH_TYPE_HEADSET ||
			     device->type == BLUETOOTH_TYPE_HEADPHONES ||
			     device->type == BLUETOOTH_TYPE_OTHER_AUDIO) && program_available (GNOMECC)) {
				add_menu_item (device->bdaddr,
					       "sound",
					       _("Sound Settings"),
					       uimanager,
					       menu_merge_id,
					       G_CALLBACK (sound_callback));
			}
		} else {
			gtk_action_set_label (action, name);

			if (device->can_connect) {
				gtk_action_set_visible (status, TRUE);
				gtk_action_set_visible (oper, TRUE);
				set_device_status_label (device->bdaddr, device->connected ? CONNECTED : DISCONNECTED);
				gtk_action_set_label (oper, device->connected ? _("Disconnect") : _("Connect"));
			} else {
				gtk_action_set_visible (status, FALSE);
				gtk_action_set_visible (oper, FALSE);
			}
		}
		g_free (name);

		if (oper != NULL) {
			g_object_set_data_full (G_OBJECT (oper),
						"connected", GINT_TO_POINTER (device->connected ? CONNECTED : DISCONNECTED), NULL);
			g_object_set_data_full (G_OBJECT (oper),
						"device-path", g_strdup (device->device_path), g_free);
		}

		/* And now for the trick of the day */
		if (device->connected != FALSE) {
			char *path;

			path = g_strdup_printf ("/bluetooth-applet-popup/devices-placeholder/%s", device->bdaddr);
			action_set_bold (uimanager, action, path);
			g_free (path);
		}

		g_boxed_free (BLUETOOTH_TYPE_SIMPLE_DEVICE, device);
	}

	/* Remove the left-over devices */
	g_list_foreach (actions, (GFunc) remove_action_item, uimanager);
	g_list_free (actions);

	/* Cleanup */
	g_list_free (devices);
done:
	gtk_ui_manager_ensure_update (uimanager);

	gtk_action_set_visible (GTK_ACTION (gtk_builder_get_object (xml, "devices-label")),
				has_devices);
}

static GOptionEntry options[] = {
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, N_("Debug"), NULL },
	{ NULL },
};

int main(int argc, char *argv[])
{
	GApplication *app;
	GtkStatusIcon *statusicon;
	GtkWidget *menu;
	GOptionContext *context;
	GError *error = NULL;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	g_type_init ();

	/* Parse command-line options */
	context = g_option_context_new (N_("- Bluetooth applet"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
			 error->message, argv[0]);
		g_error_free (error);
		return 1;
	}

	if (option_debug == FALSE) {
		GError *error = NULL;

		app = g_application_new ("org.gnome.Bluetooth.applet",
					 G_APPLICATION_FLAGS_NONE);
		if (!g_application_register (app, NULL, &error)) {
			g_object_unref (app);
			g_warning ("%s", error->message);
			g_error_free (error);
			return 1;
		}
		if (g_application_get_is_remote (app)) {
			g_object_unref (app);
			g_warning ("Applet is already running, exiting");
			return 0;
		}
	} else {
		app = NULL;
	}

	g_set_application_name(_("Bluetooth Applet"));

	gtk_window_set_default_icon_name("bluetooth");

	applet = g_object_new (BLUETOOTH_TYPE_APPLET, NULL);
	g_signal_connect (G_OBJECT (applet), "notify::killswitch-state",
			G_CALLBACK (killswitch_state_changed), NULL);

	menu = create_popupmenu();

	g_signal_connect (G_OBJECT (applet), "devices-changed",
			G_CALLBACK (update_device_list), NULL);
	g_signal_connect (G_OBJECT (applet), "notify::discoverable",
			G_CALLBACK (update_discoverability), NULL);
	g_signal_connect (G_OBJECT (applet), "notify::show-full-menu",
			G_CALLBACK (update_menu_items), NULL);

	killswitch_state_changed ((GObject*) applet, NULL, NULL);
	update_menu_items ((GObject*) applet, NULL, NULL);
	update_discoverability ((GObject*) applet, NULL, NULL);
	update_device_list (applet, NULL);

	statusicon = init_notification();

	update_icon_visibility();

	g_signal_connect(statusicon, "activate",
				G_CALLBACK(activate_callback), menu);
	g_signal_connect(statusicon, "popup-menu",
				G_CALLBACK(popup_callback), menu);

	setup_agents(applet);

	gtk_main();

	gtk_widget_destroy(menu);

	cleanup_notification();

	g_object_unref(applet);

	if (app != NULL)
		g_object_unref (app);

	return 0;
}
