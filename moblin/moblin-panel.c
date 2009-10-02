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

#include <nbtk/nbtk-gtk.h>

#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "bluetooth-chooser.h"
#include "bluetooth-chooser-private.h"
#include "bluetooth-killswitch.h"
#include "bluetooth-plugin-manager.h"
#include "bluetooth-agent.h"
#include "bluetooth-filter-widget.h"

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
	BluetoothAgent *agent;

	GtkWidget *power_switch;
	GtkWidget *notebook;
	GtkTreeModel *chooser_model;

	gboolean target_ssp;
	gchar *pincode;
	gboolean automatic_pincode;
};

#define CONNECT_TIMEOUT 3.0
#define AGENT_PATH "/org/bluez/agent/wizard"

typedef struct {
	char *path;
	GTimer *timer;
	MoblinPanel *self;
} ConnectData;

enum {
	PROPS_PAGE,
	ADD_PAGE,
	SETUP_PAGE,
	MESSAGE_PAGE
} MoblinPages;

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
killswitch_state_changed_cb (BluetoothKillswitch *killswitch,
                             KillswitchState      state,
                             gpointer             user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);

	g_signal_handlers_block_by_func (priv->power_switch, power_switch_toggled_cb, user_data);

	if (state == KILLSWITCH_STATE_SOFT_BLOCKED) {
		nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH (priv->power_switch),
	                                      FALSE);
		gtk_widget_set_sensitive (priv->power_switch, TRUE);
	} else if (state == KILLSWITCH_STATE_UNBLOCKED) {
		nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH (priv->power_switch), TRUE);
		gtk_widget_set_sensitive (priv->power_switch, TRUE);
	} else if (state == KILLSWITCH_STATE_HARD_BLOCKED) {
		gtk_widget_set_sensitive (priv->power_switch, FALSE);
	} else {
		g_assert_not_reached ();
	}

	g_signal_handlers_unblock_by_func (priv->power_switch, power_switch_toggled_cb, user_data);
}

static void
selected_device_changed_cb (BluetoothChooser *chooser, const char *address, gpointer data)
{
	GtkWidget *send_button = GTK_WIDGET (data);

	if (!address) {
		gtk_widget_set_sensitive (send_button, FALSE);	} else {
		gtk_widget_set_sensitive (send_button, TRUE);
	}
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

static void
remove_clicked_cb (GtkCellRenderer *cell, gchar *path, gpointer user_data)
{
	BluetoothChooser *chooser = BLUETOOTH_CHOOSER (user_data);
	GtkTreeView *view;
	gchar *address = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *tree_path;

	view = GTK_TREE_VIEW (bluetooth_chooser_get_treeview (chooser));

	/* Set selection */
	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_view_set_cursor (view, tree_path, NULL, FALSE);
	gtk_tree_path_free (tree_path);
	/* Get address */
	selection = gtk_tree_view_get_selection (view);
	if (gtk_tree_selection_get_selected (selection, NULL, &iter) == FALSE)
		return;

	if (bluetooth_chooser_remove_selected_device (chooser) != FALSE)
		bluetooth_plugin_manager_device_deleted (address);
}
#if 0
static gboolean
pincode_callback (DBusGMethodInvocation *context,
		  DBusGProxy *device,
		  gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	priv->target_ssp = FALSE;
	if (priv->automatic_pincode == FALSE)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), SETUP_PAGE);
	dbus_g_method_return (context, priv->pincode);
	return TRUE;
}
#endif
static void
browse_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	GtkTreeIter iter;
	char *address = NULL;
	char *cmd = NULL;
	GtkTreePath *tree_path;

	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (priv->chooser_model, &iter, tree_path);
	gtk_tree_model_get (priv->chooser_model, &iter, BLUETOOTH_COLUMN_ADDRESS, &address, -1);

	if (address == NULL) {
		cmd = g_strdup_printf ("%s --no-default-window \"obex://[%s]\"",
				       "nautilus", address);
		if (!g_spawn_command_line_async (cmd, NULL))
			g_printerr("Couldn't execute command: %s\n", cmd);
		g_free (address);
		g_free (cmd);
	}
	g_debug ("Browse clicked on %s", address);
}
#if 0
static void
connect_callback (BluetoothClient *client, gboolean success, gpointer user_data)
{
	ConnectData *data = (ConnectData *)user_data;
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (data->self);

/*	if (success == FALSE && g_timer_elapsed (data->timer, NULL) < CONNECT_TIMEOUT) {
		if (bluetooth_client_connect_service (client, data->path, connect_callback, data) != FALSE) {
			return;
		}
	}

	if (success == FALSE)
		g_message ("Failed to connect to device %s", data->path);

	set_failure_view (MOBLIN_PANEL (data->self));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), MESSAGE_PAGE);

	g_timer_destroy (data->timer);
	g_free (data->path);
	g_free (data);*/
}

static void
create_callback (BluetoothClient *client, const gchar *path, const GError *error, gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	ConnectData *data;

/*	if (path == NULL) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), MESSAGE_PAGE);
		return;
	}
	bluetooth_client_set_trusted (priv->client, path, TRUE);

	data = g_new0 (ConnectData, 1);
	data->path = g_strdup (path);
	data->timer = g_timer_new ();

	if (bluetooth_client_connect_service (client, path, connect_callback, data) == FALSE) {
		g_timer_destroy (data->timer);
		g_free (data->path);
		g_free (data);
	}*/

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PROPS_PAGE);
}
#endif
static void
pair_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	/*MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	const gchar *agent_path = AGENT_PATH;

	gchar *pin_ret = NULL;

	pin_ret = get_pincode_for_device (target_type, target_address, target_name, NULL);
	if (pin_ret != NULL && g_str_equal (pin_ret, "NULL"))
		path = NULL;

	g_object_ref (priv->agent);
	bluetooth_client_create_device (priv->client, target_address, agent_path,
					create_callback, user_data);*/
	g_debug ("Pair clicked");
}

static void
connect_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	/*MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);

	gchar *pin_ret = NULL;

	pin_ret = get_pincode_for_device (target_type, target_address, target_name, NULL);
	if (pin_ret != NULL && g_str_equal (pin_ret, "NULL"))
		path = NULL;

	g_object_ref (priv->agent);
	bluetooth_client_create_device (priv->client, target_address, agent_path,
					create_callback, user_data);*/
	g_debug ("Connect clicked");
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
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), ADD_PAGE);
}

static void
set_properties_view (GtkButton *button, MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PROPS_PAGE);
}

#if 0
static GtkWidget *
create_message_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *label;

	label = gtk_label_new ("Zing");

	return label;
}

static GtkWidget *
create_setup_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *label;

	label = gtk_label_new ("Zing");
	return label;
}
#endif

static GtkWidget *
create_add_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page;
	GtkWidget *vbox, *hbox;
	GtkWidget *chooser, *filter;
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
	chooser = g_object_new (BLUETOOTH_TYPE_CHOOSER,
				"has-internal-device-filter", FALSE,
				"show-device-category", FALSE,
				"show-searching", TRUE,
				"device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
				NULL);
	tree_view = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (chooser));
	g_object_set (tree_view, "enable-grid-lines", TRUE, "headers-visible", FALSE, NULL);
	//"grid-line-width", 2, "horizontal-separator", 6, "vertical-separator", 6,
	bluetooth_chooser_start_discovery (BLUETOOTH_CHOOSER (chooser));
	type_column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (chooser));
	/* Add the pair button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 pair_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (pair_clicked), self);

	gtk_widget_show (chooser);
	gtk_container_add (GTK_CONTAINER (frame), chooser);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	/* Back button */
	back_button = gtk_button_new_with_label (_("Back to devices"));
	gtk_widget_show (back_button);
	g_signal_connect (back_button, "clicked",
			G_CALLBACK (set_properties_view), self);
	gtk_box_pack_start (GTK_BOX (vbox), back_button, FALSE, FALSE, 4);

	/* Right column */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (page), vbox, FALSE, FALSE, 4);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

	/* Filter combo */
	filter = bluetooth_filter_widget_new ();
	gtk_widget_show (filter);
	bluetooth_filter_widget_set_title (BLUETOOTH_FILTER_WIDGET (filter), _("Only show:"));
	bluetooth_filter_widget_bind_filter (BLUETOOTH_FILTER_WIDGET (filter),
					     BLUETOOTH_CHOOSER (chooser));
	gtk_box_pack_start (GTK_BOX (vbox), filter, FALSE, FALSE, 4);

	/* Button for PIN options file */
	pin_button = gtk_button_new_with_label (_("PIN options"));
	gtk_widget_show (pin_button);
	g_signal_connect (pin_button, "clicked",
                    G_CALLBACK (pin_options_button_clicked_cb), chooser);
	gtk_box_pack_start (GTK_BOX (vbox), pin_button, FALSE, FALSE, 4);

	return page;
}

static GtkWidget *
create_properties_page (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *page;
	GtkWidget *frame_title;
	GtkWidget *chooser;
	GtkWidget *vbox, *hbox;
	GtkWidget *frame;
	GtkWidget *send_button, *add_new_button;
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
	chooser = g_object_new (BLUETOOTH_TYPE_CHOOSER,
			        "has-internal-device-filter", FALSE,
				"show-device-category", FALSE,
				"show-searching", FALSE,
				"device-category-filter", BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED,
				NULL);
	type_column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (chooser));
	if (!priv->chooser_model)
		priv->chooser_model = bluetooth_chooser_get_model (BLUETOOTH_CHOOSER (chooser));

	tree_view = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (chooser));
	g_object_set (tree_view, "enable-grid-lines", TRUE, "headers-visible", FALSE, NULL);
	//"grid-line-width", 2, "horizontal-separator", 6, "vertical-separator", 6, zebra

	/* Add the browse button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 browse_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (browse_clicked), self);

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
	g_signal_connect (cell, "activated", G_CALLBACK (remove_clicked_cb), chooser);

	gtk_widget_show (chooser);
	gtk_container_add (GTK_CONTAINER (frame), chooser);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	/* Add new button */
	add_new_button = gtk_button_new_with_label (_("Add a new device"));
	gtk_widget_show (add_new_button);
	g_signal_connect (add_new_button, "clicked",
			G_CALLBACK (set_scanning_view), self);
	gtk_box_pack_start (GTK_BOX (vbox), add_new_button, FALSE, FALSE, 4);

	/* Right column */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (page), vbox, FALSE, FALSE, 4);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

	/* Power switch */
	power_label = gtk_label_new (_("Turn Bluetooth"));
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
	send_button = gtk_button_new_with_label (_("Send file from your computer"));
	gtk_widget_show (send_button);
	g_signal_connect (send_button, "clicked",
                    G_CALLBACK (send_file_button_clicked_cb), chooser);
	g_signal_connect (chooser, "selected-device-changed",
			G_CALLBACK (selected_device_changed_cb), send_button);
	gtk_box_pack_start (GTK_BOX (vbox), send_button, FALSE, FALSE, 4);

	return page;
}

static void
moblin_panel_init (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *properties_page, *add_page; //, *setup_page, *message_page;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);
	priv->target_ssp = FALSE;
	priv->pincode = FALSE;
	priv->automatic_pincode = FALSE;

	priv->killswitch = bluetooth_killswitch_new ();

	priv->agent = bluetooth_agent_new ();
	bluetooth_plugin_manager_init ();

	gtk_box_set_homogeneous (GTK_BOX (self), FALSE);
	gtk_box_set_spacing (GTK_BOX (self), 4);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	properties_page = create_properties_page (self);
	gtk_widget_show (properties_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), properties_page, NULL);

	add_page = create_add_page (self);
	gtk_widget_show (add_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), add_page, NULL);
#if 0
	setup_page = create_setup_page (self);
	gtk_widget_show (setup_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), setup_page, NULL);

	message_page = create_message_page (self);
	gtk_widget_show (message_page);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), setup_page, NULL);
#endif
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), PROPS_PAGE);
	gtk_widget_show (priv->notebook);
	gtk_container_add (GTK_CONTAINER (self), priv->notebook);
}

static void
moblin_panel_dispose (GObject *object)
{
	//MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (object);

	bluetooth_plugin_manager_cleanup ();

	//g_object_unref (priv->agent);

	G_OBJECT_CLASS (moblin_panel_parent_class)->dispose (object);
}

static void
moblin_panel_class_init (MoblinPanelClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (MoblinPanelPrivate));
	obj_class->dispose = moblin_panel_dispose;
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
