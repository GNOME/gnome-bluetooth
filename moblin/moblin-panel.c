/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2009, Intel Corporation.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>

#include <nbtk/nbtk-gtk.h>

#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "bluetooth-chooser.h"
#include "bluetooth-chooser-private.h"
#include "bluetooth-killswitch.h"
#include "bluetooth-plugin-manager.h"
#include "bluetooth-filter-widget.h"
#include "gnome-bluetooth-enum-types.h"

#include "pin.h"

#include "mux-cell-renderer-text.h"
#include "koto-cell-renderer-pixbuf.h"

#include "moblin-panel.h"

G_DEFINE_TYPE (MoblinPanel, moblin_panel, GTK_TYPE_HBOX)

#define MOBLIN_PANEL_GET_PRIVATE(o)                                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOBLIN_TYPE_PANEL, MoblinPanelPrivate))

struct _MoblinPanelPrivate
{
	BluetoothKillswitch *killswitch;
	BluetoothClient *client;

	GtkWidget *power_switch;
	GtkWidget *notebook;
	GtkWidget *label_pin_help;
	GtkWidget *label_pin;
	GtkWidget *label_failure;
	GtkWidget *chooser;
	GtkWidget *display;
	GtkWidget *send_button;
	GtkWidget *add_new_button;
	GtkTreeModel *chooser_model;

	gchar *pincode;

	gboolean connecting;
};

#define CONNECT_TIMEOUT 3.0
#define AGENT_PATH "/org/bluez/agent/moblin"

typedef struct {
	char *path;
	GTimer *timer;
	MoblinPanel *self;
} ConnectData;

enum {
	PAGE_DEVICES,
	PAGE_ADD,
	PAGE_SETUP,
	PAGE_FAILURE
} MoblinPages;

enum {
	STATUS_CONNECTING,
	LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = {0, };

static void
power_switch_toggled_cb (NbtkGtkLightSwitch *light_switch,
                         gpointer            user_data)
{
	g_assert (MOBLIN_IS_PANEL (user_data));
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE(user_data);

	if (nbtk_gtk_light_switch_get_active (NBTK_GTK_LIGHT_SWITCH (priv->power_switch))) {
		bluetooth_killswitch_set_state (priv->killswitch, KILLSWITCH_STATE_UNBLOCKED);
	} else {
		bluetooth_killswitch_set_state (priv->killswitch, KILLSWITCH_STATE_SOFT_BLOCKED);
	}
}

static void
enable_send_file (MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	BluetoothChooser *chooser = BLUETOOTH_CHOOSER (priv->display);
	GValue value = {0, };
	gchar *name = NULL;
	gboolean enabled = FALSE;

	name = bluetooth_chooser_get_selected_device_name (chooser);
	if (name != NULL) {
		guint i;
		const char **uuids;

		bluetooth_chooser_get_selected_device_info (chooser, "uuids", &value);

		uuids = (const char **) g_value_get_boxed (&value);
		if (uuids != NULL) {
			for (i = 0; uuids[i] != NULL; i++)
				if (g_str_equal (uuids[i], "OBEXObjectPush")) {
					enabled = TRUE;
					break;
				}
			g_value_unset (&value);
		}
	}
	gtk_widget_set_sensitive (priv->send_button, enabled);
}

static void
killswitch_state_changed_cb (BluetoothKillswitch *killswitch,
                             KillswitchState      state,
                             gpointer             user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);

	g_signal_handlers_block_by_func (priv->power_switch, power_switch_toggled_cb, user_data);

	if (state == KILLSWITCH_STATE_SOFT_BLOCKED) {
		nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH (priv->power_switch),
	                                      FALSE);
		gtk_widget_set_sensitive (priv->power_switch, TRUE);
		gtk_widget_set_sensitive (priv->add_new_button, FALSE);
		gtk_widget_set_sensitive (priv->send_button, FALSE);
	} else if (state == KILLSWITCH_STATE_UNBLOCKED) {
		nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH (priv->power_switch), TRUE);
		gtk_widget_set_sensitive (priv->power_switch, TRUE);
		gtk_widget_set_sensitive (priv->add_new_button, TRUE);
		enable_send_file (self);
	} else if (state == KILLSWITCH_STATE_HARD_BLOCKED) {
		gtk_widget_set_sensitive (priv->power_switch, FALSE);
		gtk_widget_set_sensitive (priv->add_new_button, FALSE);
		gtk_widget_set_sensitive (priv->send_button, FALSE);
	} else {
		g_assert_not_reached ();
	}

	g_signal_handlers_unblock_by_func (priv->power_switch, power_switch_toggled_cb, user_data);
}

static void
selected_device_changed_cb (BluetoothChooser *chooser, const char *address, gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	enable_send_file (MOBLIN_PANEL (self));
}

static void
set_frame_title (GtkFrame *frame, gchar *title)
{
	GtkWidget *frame_title;
	gchar *label = NULL;

	label = g_strdup_printf ("<span font_desc=\"Liberation Sans Bold 18px\""
	                           "foreground=\"#3e3e3e\">%s</span>", title);
	frame_title = gtk_frame_get_label_widget (frame);
	gtk_label_set_markup (GTK_LABEL (frame_title), label);
	g_free (label);
	gtk_widget_show (frame_title);
}

static void
send_file_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
	BluetoothChooser *chooser = BLUETOOTH_CHOOSER (user_data);
	GPtrArray *a;
	GError *err = NULL;
	guint i;
	const char *address, *name;

	address = bluetooth_chooser_get_selected_device (chooser);
	name = bluetooth_chooser_get_selected_device_name (chooser);

	a = g_ptr_array_new ();
	g_ptr_array_add (a, "bluetooth-sendto");
	if (address != NULL) {
		char *s;

		s = g_strdup_printf ("--device=\"%s\"", address);
		g_ptr_array_add (a, s);
	}
	if (address != NULL && name != NULL) {
		char *s;

		s = g_strdup_printf ("--name=\"%s\"", name);
		g_ptr_array_add (a, s);
	}
	g_ptr_array_add (a, NULL);

	if (g_spawn_async(NULL, (char **) a->pdata, NULL,
			  G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err) == FALSE) {
		g_printerr("Couldn't execute command: %s\n", err->message);
		g_error_free (err);
	}

	for (i = 1; a->pdata[i] != NULL; i++)
		g_free (a->pdata[i]);

	g_ptr_array_free (a, TRUE);
}

static void
pin_options_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
	g_debug ("PIN options clicked.");
}

/*
 * This helper function forces the currently selected row in the chooser to be
 * the same row which contains the activated cell.
 */
static void
ensure_selection (BluetoothChooser *chooser, const gchar *path)
{
	GtkTreeView *view;
	GtkTreePath *tree_path;
	view = GTK_TREE_VIEW (bluetooth_chooser_get_treeview (chooser));

	/* Set selection */
	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_view_set_cursor (view, tree_path, NULL, FALSE);
	gtk_tree_path_free (tree_path);
}

static void
remove_clicked_cb (GtkCellRenderer *cell, const gchar *path, gpointer user_data)
{
	BluetoothChooser *chooser = BLUETOOTH_CHOOSER (user_data);
	const gchar *address = NULL;
	GValue value = { 0, };

	ensure_selection (chooser, path);

	/* Get address */
	if (bluetooth_chooser_get_selected_device_info (chooser, "address", &value)) {
		address = g_value_get_string (&value);
		g_value_unset (&value);
	}

	if (bluetooth_chooser_remove_selected_device (chooser) != FALSE && address) {
		bluetooth_plugin_manager_device_deleted (address);
	}
}

static void
browse_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	BluetoothChooser *chooser = BLUETOOTH_CHOOSER (user_data);
	const gchar *address = NULL;
	GValue value = { 0, };
	gchar *cmd;

	ensure_selection (chooser, path);

	/* Get address */
	if (bluetooth_chooser_get_selected_device_info (chooser, "address", &value)) {
		address = g_value_get_string (&value);
		g_value_unset (&value);
	}

	if (address == NULL) {
		cmd = g_strdup_printf ("%s --no-default-window \"obex://[%s]\"",
				       "nautilus", address);
		if (!g_spawn_command_line_async (cmd, NULL))
			g_printerr("Couldn't execute command: %s\n", cmd);
		g_free (cmd);
	}
	g_debug ("Browse clicked on %s", address);
}

static void
connect_callback (BluetoothClient *client, gboolean success, gpointer user_data)
{
	ConnectData *data = (ConnectData *)user_data;
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (data->self);

	if (success == FALSE && g_timer_elapsed (data->timer, NULL) < CONNECT_TIMEOUT) {
		if (bluetooth_client_connect_service (client, data->path, connect_callback, data) != FALSE)
			return;
	}

	if (success == FALSE)
		g_message ("Failed to connect to device %s", data->path);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_DEVICES);

	g_timer_destroy (data->timer);
	g_free (data->path);
	g_object_unref (data->self);
	g_free (data);
}

static void
set_failure_message (MoblinPanel *self, gchar *device)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	gchar *str;

	str = g_strdup_printf (_("Pairing with %s failed."), device);
	gtk_label_set_text (GTK_LABEL (priv->label_failure), str);
}

static void
connect_device (const gchar *device_path, MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	ConnectData *data;

	data = g_new0 (ConnectData, 1);
	data->path = g_strdup (device_path);
	data->timer = g_timer_new ();
	data->self = g_object_ref (self);

	if (bluetooth_client_connect_service (priv->client, device_path, connect_callback, data) == FALSE) {
		g_timer_destroy (data->timer);
		g_free (data->path);
		g_object_unref (data->self);
		g_free (data);
	}
}

static void
create_callback (BluetoothClient *client, const gchar *path, const GError *error, gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	gchar *device_name = bluetooth_chooser_get_selected_device_name (BLUETOOTH_CHOOSER (priv->chooser));

	g_debug ("Create callback entered");

	if (path == NULL) {
		g_debug ("Path is NULL !!!");
		set_failure_message (self, device_name);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_FAILURE);
		return;
	}

	bluetooth_client_set_trusted (client, path, TRUE);

	connect_device (path, self);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_DEVICES);
}

static void
set_large_label (GtkLabel *label, const char *text)
{
	char *str;

	str = g_strdup_printf("<span font_desc=\"50\" color=\"black\" bgcolor=\"white\">  %s  </span>", text);
	gtk_label_set_markup(GTK_LABEL(label), str);
	g_free(str);
}

static void
pair_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	const gchar *agent_path = AGENT_PATH;
	gchar *name;
	BluetoothType type;
	guint legacy_pairing;
	gboolean target_ssp, automatic_pincode;
	const gchar *address = NULL;
	gchar *pincode = NULL;
	gchar *pin_ret;
	gchar *target_pincode;
	GValue value = { 0, };

	ensure_selection (BLUETOOTH_CHOOSER (priv->chooser), path);

	g_debug ("Pair clicked");

	target_pincode = g_strdup_printf ("%d", g_random_int_range (pow (10, PIN_NUM_DIGITS - 1),
								    pow (10, PIN_NUM_DIGITS) - 1));

	name = bluetooth_chooser_get_selected_device_name (BLUETOOTH_CHOOSER (priv->chooser));
	type = bluetooth_chooser_get_selected_device_type (BLUETOOTH_CHOOSER (priv->chooser));
	bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (priv->chooser), "address", &value);
	address = g_value_dup_string (&value);
	g_value_unset (&value);
	if (bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (priv->chooser), "legacypairing", &value) != FALSE) {
		legacy_pairing = g_value_get_int (&value);
		if (legacy_pairing == -1)
			target_ssp = TRUE;
	} else {
		target_ssp = TRUE;
	}
	g_value_unset (&value);

	if (priv->pincode != NULL && *priv->pincode != '\0') {
		pincode = g_strdup (priv->pincode);
		automatic_pincode = TRUE;
	} else if (address != NULL) {
		guint max_digits;
		pincode = get_pincode_for_device (type, address, name, &max_digits);
		if (target_pincode == NULL) {
			/* Truncate the default pincode if the device doesn't like long
			 * PIN codes */
			if (max_digits != PIN_NUM_DIGITS && max_digits > 0)
				pincode = g_strndup(target_pincode, max_digits);
			else
				pincode = g_strdup(target_pincode);
		} else if (target_ssp == FALSE) {
			automatic_pincode = TRUE;
		}
	}

	pin_ret = get_pincode_for_device (type, address, name, NULL);
	if (pin_ret != NULL && g_str_equal (pin_ret, "NULL")) {
		agent_path = NULL;
		g_debug ("Setting agent_path to NULL");
	}
	g_free (pin_ret);

	bluetooth_client_create_device (priv->client, address, agent_path,
					create_callback, user_data);

	if (automatic_pincode == FALSE && target_ssp == FALSE) {
		char *text;

		if (type == BLUETOOTH_TYPE_KEYBOARD) {
			text = g_strdup_printf (_("Please enter the following PIN on '%s' and press “Enter” on the keyboard:"), name);
		} else {
			text = g_strdup_printf (_("Please enter the following PIN on '%s':"), name);
		}
		gtk_label_set_markup(GTK_LABEL(priv->label_pin_help), text);
		g_free (text);
		set_large_label (GTK_LABEL (priv->label_pin), pincode);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_SETUP);
	}
}

static void
connect_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	GValue value = { 0, };
	DBusGProxy *device;
	const gchar *device_path = NULL;

	ensure_selection (BLUETOOTH_CHOOSER (priv->display), path);

	bluetooth_chooser_get_selected_device_info (BLUETOOTH_CHOOSER (priv->display), "proxy", &value);
	device = g_value_get_object (&value);
	device_path = dbus_g_proxy_get_path (device);
	g_value_unset (&value);

	connect_device (device_path, self);
}

static void
remove_to_icon (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired, trusted;
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &paired,
			BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);
	if (paired || trusted) {
		g_object_set (cell, "icon-name", GTK_STOCK_CLEAR, NULL);
	} else {
		g_object_set (cell, "icon-name", NULL, NULL);
	}
}

static void
pair_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired, trusted;
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &paired,
			BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if (!paired && !trusted) {
		g_object_set (cell, "markup", _("Pair"), NULL);
	}
}

static void
connect_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired, trusted, connected;
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &paired,
			BLUETOOTH_COLUMN_CONNECTED, &connected,
			BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if ((paired || trusted) && !connected) {
		g_object_set (cell, "markup", _("Connect"), NULL);
	}
}

static void
browse_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean *connected;
	guint type;
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_CONNECTED, &connected,
			BLUETOOTH_COLUMN_TYPE, &type, -1);

	if (connected &&
	(type == BLUETOOTH_TYPE_PHONE || type == BLUETOOTH_TYPE_COMPUTER || type == BLUETOOTH_TYPE_CAMERA)) {
		g_object_set (cell, "markup", _("Browse"), NULL);
	}
}


static void
set_scanning_view (GtkButton *button, MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_ADD);
}

static void
set_device_view (GtkButton *button, MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_DEVICES);
}

static void
determine_connecting (const gchar *key, gpointer value, gpointer user_data)
{
	BluetoothStatus status = GPOINTER_TO_INT (value);
	BluetoothStatus *other_status = user_data;

	if (status == BLUETOOTH_STATUS_CONNECTING)
		(*other_status) = status;
}

static void
have_connecting_device (MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	GHashTable *states;
	GtkTreeIter iter;
	BluetoothStatus status = BLUETOOTH_STATUS_INVALID;
	gboolean connecting = FALSE;

	gtk_tree_model_get_iter_first (priv->chooser_model, &iter);
	gtk_tree_model_get(priv->chooser_model, &iter, BLUETOOTH_COLUMN_SERVICES, &states, -1);

	if (states) {
		g_hash_table_foreach (states, (GHFunc) determine_connecting, &status);

		g_hash_table_unref (states);
	}

	if (status == BLUETOOTH_STATUS_CONNECTING)
		connecting = TRUE;

	if (connecting != priv->connecting) {
		priv->connecting = connecting;
		g_signal_emit (self, _signals[STATUS_CONNECTING], 0,
			priv->connecting);
	}
}

static void
model_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	have_connecting_device (MOBLIN_PANEL (user_data));
}

static void
row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer user_data)
{
	have_connecting_device (MOBLIN_PANEL (user_data));
}

static GtkWidget *
create_failure_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page;
	GtkWidget *page_title;
	GtkWidget *vbox, *hbox;
	GtkWidget *back_button;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);

	page = nbtk_gtk_frame_new ();
	page_title = gtk_label_new ("");
	gtk_frame_set_label_widget (GTK_FRAME (page), page_title);
	set_frame_title (GTK_FRAME (page), _("Device setup failed"));
	gtk_widget_show (page);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (page), vbox);
	priv->label_failure = gtk_label_new ("");
	gtk_widget_show (priv->label_failure);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 6);
	back_button = gtk_button_new_with_label (_("Back to devices"));
	gtk_widget_show (back_button);
	gtk_box_pack_start (GTK_BOX (hbox), back_button, FALSE, FALSE, 6);
	g_signal_connect (back_button, "clicked", G_CALLBACK (set_device_view), self);

	return page;
}

static GtkWidget *
create_setup_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page, *page_title;
	GtkWidget *vbox, *hbox;
	GtkWidget *back_button;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);

	page = nbtk_gtk_frame_new ();
	page_title = gtk_label_new ("");
	gtk_frame_set_label_widget (GTK_FRAME (page), page_title);
	set_frame_title (GTK_FRAME (page), _("Device setup"));
	gtk_widget_show (page);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (page), vbox);
	priv->label_pin_help = gtk_label_new ("");
	gtk_widget_show (priv->label_pin_help);
	gtk_box_pack_start (GTK_BOX (vbox), priv->label_pin_help, FALSE, FALSE, 6);
	priv->label_pin = gtk_label_new ("");
	gtk_widget_show (priv->label_pin);
	gtk_box_pack_start (GTK_BOX (vbox), priv->label_pin, FALSE, FALSE, 6);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 6);
	back_button = gtk_button_new_with_label (_("Back to devices"));
	gtk_widget_show (back_button);
	gtk_box_pack_start (GTK_BOX (hbox), back_button, FALSE, FALSE, 6);
	g_signal_connect (back_button, "clicked", G_CALLBACK (set_device_view), self);

	return page;
}

static GtkWidget *
create_add_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page;
	GtkWidget *vbox, *hbox;
	GtkWidget *filter;
	GtkWidget *frame_title;
	GtkWidget *frame;
	GtkWidget *back_button;
	GtkWidget *pin_button;
	GtkWidget *tree_view;
	GtkTreeViewColumn *type_column;
	GtkCellRenderer *cell;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);

	page = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (page);

	/* Add child widgetry */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (page), vbox, TRUE, TRUE, 4);

	frame = nbtk_gtk_frame_new ();
	frame_title = gtk_label_new ("");
	gtk_frame_set_label_widget (GTK_FRAME (frame), frame_title);
	set_frame_title (GTK_FRAME (frame), _("Devices"));

	/* Device list */
	priv->chooser = g_object_new (BLUETOOTH_TYPE_CHOOSER,
				"has-internal-device-filter", FALSE,
				"show-device-category", FALSE,
				"show-searching", TRUE,
				"device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
				NULL);
	tree_view = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (priv->chooser));
	g_object_set (tree_view, "enable-grid-lines", TRUE, "headers-visible", FALSE, NULL);
	bluetooth_chooser_start_discovery (BLUETOOTH_CHOOSER (priv->chooser));
	type_column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (priv->chooser));
	/* Add the pair button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 pair_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (pair_clicked), self);

	gtk_widget_show (priv->chooser);
	gtk_container_add (GTK_CONTAINER (frame), priv->chooser);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	/* Back button */
	back_button = gtk_button_new_with_label (_("Back to devices"));
	gtk_widget_show (back_button);
	g_signal_connect (back_button, "clicked",
			G_CALLBACK (set_device_view), self);
	gtk_box_pack_start (GTK_BOX (vbox), back_button, FALSE, FALSE, 4);

	/* Right column */
	frame = nbtk_gtk_frame_new ();
	gtk_widget_show (frame);
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 4);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

	/* Filter combo */
	filter = bluetooth_filter_widget_new ();
	gtk_widget_show (filter);
	bluetooth_filter_widget_set_title (BLUETOOTH_FILTER_WIDGET (filter), _("Only show:"));
	bluetooth_filter_widget_bind_filter (BLUETOOTH_FILTER_WIDGET (filter),
					     BLUETOOTH_CHOOSER (priv->chooser));
	gtk_box_pack_start (GTK_BOX (vbox), filter, FALSE, FALSE, 4);

	/* Button for PIN options file */
	pin_button = gtk_button_new_with_label (_("PIN options"));
	gtk_widget_show (pin_button);
	g_signal_connect (pin_button, "clicked",
                    G_CALLBACK (pin_options_button_clicked_cb), self);
	gtk_box_pack_start (GTK_BOX (vbox), pin_button, FALSE, FALSE, 4);

	return page;
}

static GtkWidget *
create_devices_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page;
	GtkWidget *frame_title;
	GtkWidget *vbox, *hbox;
	GtkWidget *frame;
	GtkWidget *power_label;
	GtkTreeViewColumn *type_column;
	GtkCellRenderer *cell;
	KillswitchState switch_state;
	GtkWidget *tree_view;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);

	page = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (page);
	/* Add child widgetry */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (page), vbox, TRUE, TRUE, 4);

	frame = nbtk_gtk_frame_new ();
	frame_title = gtk_label_new ("");
	gtk_frame_set_label_widget (GTK_FRAME (frame), frame_title);
	set_frame_title (GTK_FRAME (frame), _("Devices"));

	/* Device list */
	priv->display = g_object_new (BLUETOOTH_TYPE_CHOOSER,
			        "has-internal-device-filter", FALSE,
				"show-device-category", FALSE,
				"show-searching", FALSE,
				"device-category-filter", BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED,
				NULL);
	type_column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (priv->display));
	if (!priv->chooser_model) {
		priv->chooser_model = bluetooth_chooser_get_model (BLUETOOTH_CHOOSER (priv->display));
		g_signal_connect (priv->chooser_model, "row-changed", G_CALLBACK (model_changed_cb), self);
		g_signal_connect (priv->chooser_model, "row-deleted", G_CALLBACK (row_deleted_cb), self);
		g_signal_connect (priv->chooser_model, "row-inserted", G_CALLBACK (model_changed_cb), self);
	}

	tree_view = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (priv->display));
	g_object_set (tree_view, "enable-grid-lines", TRUE, "headers-visible", FALSE, NULL);

	/* Add the browse button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 browse_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (browse_clicked), priv->display);

	/* Add the connect button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 connect_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (connect_clicked), self);
	/* Add the remove button */
	cell = koto_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_end (type_column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						remove_to_icon, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (remove_clicked_cb), priv->display);

	gtk_widget_show (priv->display);
	gtk_container_add (GTK_CONTAINER (frame), priv->display);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	/* Add new button */
	priv->add_new_button = gtk_button_new_with_label (_("Add a new device"));
	gtk_widget_show (priv->add_new_button);
	g_signal_connect (priv->add_new_button, "clicked",
			G_CALLBACK (set_scanning_view), self);
	gtk_box_pack_start (GTK_BOX (vbox), priv->add_new_button, FALSE, FALSE, 4);

	/* Right column */
	frame = nbtk_gtk_frame_new ();
	gtk_widget_show (frame);
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 4);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

	/* Power switch */
	/* Translators: This string appears next to a toggle switch which controls
	 * enabling/disabling Bluetooth radio's on the device. Akin to the power
	 * switches in the Network UI of Moblin */
	power_label = gtk_label_new (_("Bluetooth"));
	gtk_widget_show (power_label);
	gtk_box_pack_start (GTK_BOX (hbox), power_label, FALSE, FALSE, 4);

	priv->power_switch = nbtk_gtk_light_switch_new ();
	if (priv->killswitch != NULL) {
		if (bluetooth_killswitch_has_killswitches (priv->killswitch) == FALSE) {
			g_object_unref (priv->killswitch);
			priv->killswitch = NULL;
			gtk_widget_set_sensitive (priv->power_switch, FALSE);
		} else {
			switch_state = bluetooth_killswitch_get_state (priv->killswitch);

			switch (switch_state) {
				case KILLSWITCH_STATE_UNBLOCKED:
					nbtk_gtk_light_switch_set_active
						(NBTK_GTK_LIGHT_SWITCH (priv->power_switch),
						 TRUE);
				break;
				case KILLSWITCH_STATE_SOFT_BLOCKED:
					nbtk_gtk_light_switch_set_active
						(NBTK_GTK_LIGHT_SWITCH (priv->power_switch),
							FALSE);
				break;
				case KILLSWITCH_STATE_HARD_BLOCKED:
				default:
					gtk_widget_set_sensitive (priv->power_switch, FALSE);
				break;
			}

			g_signal_connect  (priv->killswitch, "state-changed",
				G_CALLBACK (killswitch_state_changed_cb), self);
			g_signal_connect (priv->power_switch, "switch-flipped",
				G_CALLBACK (power_switch_toggled_cb), self);
		}
	} else {
		gtk_widget_set_sensitive (priv->power_switch, FALSE);
	}
	gtk_widget_show (priv->power_switch);
	gtk_box_pack_start (GTK_BOX (hbox), priv->power_switch, FALSE, FALSE, 4);

	/* Button for Send file */
	priv->send_button = gtk_button_new_with_label (_("Send file from your computer"));
	gtk_widget_show (priv->send_button);
	g_signal_connect (priv->send_button, "clicked",
                    G_CALLBACK (send_file_button_clicked_cb), priv->display);
	g_signal_connect (priv->display, "selected-device-changed",
			G_CALLBACK (selected_device_changed_cb), priv->send_button);
	gtk_box_pack_start (GTK_BOX (vbox), priv->send_button, FALSE, FALSE, 4);

	return page;
}

static void
moblin_panel_init (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *devices_page, *add_page, *setup_page, *failure_page;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);
	priv->pincode = NULL;
	priv->connecting = FALSE;

	priv->client = bluetooth_client_new ();
	priv->killswitch = bluetooth_killswitch_new ();
	bluetooth_plugin_manager_init ();

	gtk_box_set_homogeneous (GTK_BOX (self), FALSE);
	gtk_box_set_spacing (GTK_BOX (self), 4);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	devices_page = create_devices_page (self);
	gtk_widget_show (devices_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), devices_page, NULL);

	add_page = create_add_page (self);
	gtk_widget_show (add_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), add_page, NULL);

	setup_page = create_setup_page (self);
	gtk_widget_show (setup_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), setup_page, NULL);

	failure_page = create_failure_page (self);
	gtk_widget_show (failure_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), failure_page, NULL);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PAGE_DEVICES);
	gtk_widget_show (priv->notebook);
	gtk_container_add (GTK_CONTAINER (self), priv->notebook);
}

static void
moblin_panel_dispose (GObject *object)
{
	//MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (object);

	bluetooth_plugin_manager_cleanup ();
	//g_object_unref (priv->client);

	G_OBJECT_CLASS (moblin_panel_parent_class)->dispose (object);
}

static void
moblin_panel_class_init (MoblinPanelClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (MoblinPanelPrivate));
	obj_class->dispose = moblin_panel_dispose;

	_signals[STATUS_CONNECTING] = g_signal_new ("status-connecting", MOBLIN_TYPE_PANEL,
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET (MoblinPanelClass, status_connecting),
						NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
						G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * moblin_panel_new:
 *
 * Return value: A #MoblinPanel widget
 **/
GtkWidget *
moblin_panel_new (void)
{
	return g_object_new (MOBLIN_TYPE_PANEL, NULL);
}
