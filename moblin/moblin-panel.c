#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nbtk/nbtk-gtk.h>

#include "bluetooth-client.h"
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
	BluetoothClient *client;
	BluetoothKillswitch *killswitch;
	BluetoothAgent *agent;

	GtkWidget *device_list_frame;
	GtkWidget *chooser;
	GtkWidget *power_switch;
	GtkWidget *power_label;
	GtkWidget *filter;
	GtkWidget *action_button;
	GtkWidget *scan_label;
	
	GtkTreeModel *chooser_model;
	
	gboolean target_ssp;
	gchar *pincode;
	gboolean automatic_pincode;
};

static void set_scanning_view (GtkButton *button, gpointer   user_data);
static void send_file_button_clicked_cb (GtkButton *button, gpointer   user_data);
static void pin_options_button_clicked_cb (GtkButton *button, gpointer   user_data);

static void
power_switch_toggled_cb (NbtkGtkLightSwitch *light_switch,
                         gpointer            user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE(user_data);

	if (nbtk_gtk_light_switch_get_active (NBTK_GTK_LIGHT_SWITCH (light_switch))) {
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
	MoblinPanel        *self = user_data;
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);

	g_signal_handlers_disconnect_by_func (priv->power_switch, power_switch_toggled_cb,
                                        NULL);

	g_return_if_fail (self != NULL);

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

	g_signal_connect (priv->power_switch, "switch-flipped", 
			G_CALLBACK (power_switch_toggled_cb), self);
}

static void
selected_device_changed_cb (BluetoothChooser *chooser, const char *address, gpointer data)
{
	MoblinPanel *self = MOBLIN_PANEL (data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	
	if (!address) {
		gtk_widget_set_sensitive (priv->action_button, FALSE);	
	} else {
		gtk_widget_set_sensitive (priv->action_button, TRUE);
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
set_default_view (GtkButton *button, gpointer user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);

	set_frame_title (GTK_FRAME (priv->device_list_frame), _("Devices"));

	bluetooth_chooser_stop_discovery (BLUETOOTH_CHOOSER (priv->chooser));
	g_object_set (priv->chooser, "show-searching", FALSE, "device-category-filter",
	                BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED, NULL);

	g_signal_handlers_disconnect_by_func (button, set_default_view, user_data);
	gtk_button_set_label (button, _("Add a new device"));
	g_signal_connect (button, "clicked", G_CALLBACK (set_scanning_view), user_data);
	gtk_widget_show (priv->power_switch);
	gtk_widget_show (priv->power_label);

	g_signal_handlers_disconnect_by_func (priv->action_button, pin_options_button_clicked_cb,
                                        	NULL);
	gtk_button_set_label (GTK_BUTTON (priv->action_button), _("Send file from your computer"));
	gtk_widget_show (priv->action_button);
	g_signal_connect (priv->action_button, "clicked", G_CALLBACK (send_file_button_clicked_cb),
                    NULL);
	gtk_widget_hide (priv->scan_label);
	
	g_signal_connect (priv->chooser, "selected-device-changed",
			G_CALLBACK (selected_device_changed_cb), MOBLIN_PANEL (user_data));

	gtk_widget_hide (priv->filter);
}

static void
set_scanning_view (GtkButton *button, gpointer   user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);

	set_frame_title (GTK_FRAME (priv->device_list_frame), _("Add a new Bluetooth device"));

	g_object_set (priv->chooser, "show-searching", TRUE, "device-category-filter",
                BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED, NULL);

	g_signal_handlers_disconnect_by_func (button, set_scanning_view, user_data);
        gtk_button_set_label (button, _("Back to devices"));
	g_signal_connect (button, "clicked", G_CALLBACK (set_default_view),
                    	user_data);

	bluetooth_chooser_start_discovery (BLUETOOTH_CHOOSER (priv->chooser));
	gtk_widget_hide (priv->power_switch);
	gtk_widget_hide (priv->power_label);

	g_signal_handlers_disconnect_by_func (priv->action_button,
        	                                send_file_button_clicked_cb, NULL);
        g_signal_handlers_disconnect_by_func (priv->chooser, selected_device_changed_cb,
        					MOBLIN_PANEL (user_data));
	gtk_button_set_label (GTK_BUTTON (priv->action_button), _("PIN options"));
	gtk_widget_show (priv->action_button);
	g_signal_connect (priv->action_button, "clicked",
                    G_CALLBACK (pin_options_button_clicked_cb), NULL);
	gtk_widget_show (priv->scan_label);

	gtk_widget_show (priv->filter);
}

static void
set_pairing_view (MoblinPanel *self)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);

	set_frame_title (GTK_FRAME (priv->device_list_frame), _("Device setup"));

	gtk_widget_hide (priv->chooser);

	g_signal_handlers_disconnect_by_func (priv->action_button,
        	                                send_file_button_clicked_cb, NULL);
        g_signal_handlers_disconnect_by_func (priv->chooser, selected_device_changed_cb,
        					self);
	gtk_widget_hide (priv->action_button);

	gtk_widget_hide (priv->scan_label);

	gtk_widget_hide (priv->filter);
}

static void
send_file_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	GPtrArray *a;
	GError *err = NULL;
	guint i;
	const char *address, *name;

	address = bluetooth_chooser_get_selected_device (BLUETOOTH_CHOOSER (priv->chooser));
	name = bluetooth_chooser_get_selected_device_name (BLUETOOTH_CHOOSER (priv->chooser));

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
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	GtkTreeView *view = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (priv->chooser));
	gchar *address = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *tree_path;

	/* Set selection */
	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_view_set_cursor (view, tree_path, NULL, FALSE);
	gtk_tree_path_free (tree_path);
	/* Get address */
	selection = gtk_tree_view_get_selection (view);
	if (gtk_tree_selection_get_selected (selection, NULL, &iter) == FALSE)
		return;

	bluetooth_chooser_remove_selected_device (BLUETOOTH_CHOOSER (priv->chooser));
	//bluetooth_plugin_manager_device_deleted (address);
}

static void
set_pin_view (MoblinPanel *self)
{
	g_debug ("Show PIN");
}

static gboolean
pincode_callback (DBusGMethodInvocation *context,
		  DBusGProxy *device,
		  gpointer user_data)
{
	MoblinPanel *self = MOBLIN_PANEL (user_data);
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (self);
	
	priv->target_ssp = FALSE;
	if (priv->automatic_pincode == FALSE)
		set_pin_view (self);
	dbus_g_method_return (context, priv->pincode);
	
	return TRUE;
}

static void
activity_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	GtkTreeIter iter;
	DBusGProxy *proxy = NULL;
	char *address = NULL;
	char *name = NULL;
	GtkTreePath *tree_path;
	
	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (priv->chooser_model, &iter, tree_path);
	gtk_tree_model_get (priv->chooser_model, &iter, BLUETOOTH_COLUMN_PROXY, &proxy,
			   BLUETOOTH_COLUMN_ADDRESS, &address,
			   BLUETOOTH_COLUMN_NAME, &name, -1);
			   
	if (address == NULL || name == NULL || proxy == NULL) {
		g_object_unref (proxy);
		g_free (address);
		g_free (name);
		return;
	}
	
	g_debug ("Activity clicked on %s", address);
	
	g_free (address);
	g_free (name);
	g_object_unref (proxy);
}

static void
connect_clicked (GtkCellRenderer *renderer, const gchar *path, gpointer user_data)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (user_data);
	GtkTreeIter iter;
	DBusGProxy *proxy = NULL;
	char *address = NULL;
	char *name = NULL;
	GtkTreePath *tree_path;
	
	tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (priv->chooser_model, &iter, tree_path);
	gtk_tree_model_get (priv->chooser_model, &iter, BLUETOOTH_COLUMN_PROXY, &proxy,
			   BLUETOOTH_COLUMN_ADDRESS, &address,
			   BLUETOOTH_COLUMN_NAME, &name, -1);
			   
	if (address == NULL || name == NULL || proxy == NULL) {
		g_object_unref (proxy);
		g_free (address);
		g_free (name);
		return;
	}
	
	g_debug ("Connect clicked on %s", address);

	/*bluetooth_agent_set_pincode_func (priv->agent, pincode_callback, self);
	bluetooth_agent_set_display_func (priv->agent, display_callback, self);
	bluetooth_agent_set_cancel_func (priv->agent, cancel_callback, self);
	bluetooth_agent_set_confirm_func (priv->agent, confirm_callback, self);
	bluetooth_agent_setup (priv->agent, bluetooth_path);
	
	set_pairing_view (self);*/
	
	g_free (address);
	g_free (name);
	g_object_unref (proxy);
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
activity_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired, trusted;
	
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &paired,
			BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if (paired || trusted) {
		g_object_set (cell, "markup", _("Activity"), NULL);
	}
}

static void
connect_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean paired, trusted;
	
	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &paired,
			BLUETOOTH_COLUMN_TRUSTED, &trusted, -1);

	if (paired || trusted) {
		g_object_set (cell, "markup", _("Connect"), NULL);
	}
}

static void
moblin_panel_dispose (GObject *object)
{
	MoblinPanelPrivate *priv = MOBLIN_PANEL_GET_PRIVATE (object);
	
	bluetooth_plugin_manager_cleanup ();
	
	//g_object_unref (priv->agent);
	//g_object_unref (priv->client);
	
	G_OBJECT_CLASS (moblin_panel_parent_class)->dispose (object);
}

static void
moblin_panel_class_init (MoblinPanelClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (MoblinPanelPrivate));
	
	obj_class->dispose = moblin_panel_dispose;
}

static void
moblin_panel_init (MoblinPanel *self)
{
	MoblinPanelPrivate *priv;
	GtkWidget *add_new_button;
	GtkWidget *frame_title;
	GtkWidget *vbox, *hbox;
	gchar *scan_text = NULL;
	KillswitchState switch_state;
	GtkSizeGroup *group;
	GtkTreeViewColumn *type_column;
	GtkCellRenderer *cell;

	priv = MOBLIN_PANEL_GET_PRIVATE (self);
	
	priv->target_ssp = FALSE;
	priv->pincode = FALSE;
	priv->automatic_pincode = FALSE;

	priv->client = bluetooth_client_new ();
	priv->killswitch = bluetooth_killswitch_new ();
	if (bluetooth_killswitch_has_killswitches (priv->killswitch) == FALSE) {
		g_object_unref (priv->killswitch);
		priv->killswitch = NULL;
	} else {
		g_signal_connect  (priv->killswitch, "state-changed",
                       G_CALLBACK (killswitch_state_changed_cb), self);
	}
	
	priv->agent = bluetooth_agent_new ();
	bluetooth_plugin_manager_init ();

	/* set up the box */
	gtk_box_set_homogeneous (GTK_BOX (self), FALSE);
	gtk_box_set_spacing (GTK_BOX (self), 4);

	/* Add child widgetry */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (self), vbox, TRUE, TRUE, 4);

	priv->device_list_frame = nbtk_gtk_frame_new ();
	frame_title = gtk_label_new ("");
	gtk_frame_set_label_widget (GTK_FRAME (priv->device_list_frame), frame_title);
	set_frame_title (GTK_FRAME (priv->device_list_frame), _("Devices"));

	/* Device list */
	priv->chooser = g_object_new (BLUETOOTH_TYPE_CHOOSER,
				      "has-internal-device-filter", FALSE,
		      		      "show-device-category", FALSE,
				      NULL);
	
	priv->chooser_model = bluetooth_chooser_get_model (BLUETOOTH_CHOOSER (priv->chooser));
	type_column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (priv->chooser));
	
	/* Add the activity button */
	cell = mux_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (type_column, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (type_column, cell,
						 activity_to_text, self, NULL);
	g_signal_connect (cell, "activated", G_CALLBACK (activity_clicked), self);

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
	g_signal_connect (cell, "activated", G_CALLBACK (remove_clicked_cb), self);
	
	g_signal_connect (priv->chooser, "selected-device-changed",
			G_CALLBACK (selected_device_changed_cb), self);

	gtk_widget_show (priv->chooser);
	gtk_container_add (GTK_CONTAINER (priv->device_list_frame), priv->chooser);
	gtk_widget_show (priv->device_list_frame);
	gtk_box_pack_start (GTK_BOX (vbox), priv->device_list_frame,
                      FALSE, FALSE, 4);

	/* Add new button */
	add_new_button = gtk_button_new_with_label (_("Add a new device"));
	gtk_widget_show (add_new_button);
	g_signal_connect (add_new_button, "clicked",
			G_CALLBACK (set_scanning_view), self);
	gtk_box_pack_start (GTK_BOX (vbox), add_new_button, FALSE, FALSE, 4);

	/* Right column */
	//group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (self), vbox, FALSE, FALSE, 4);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	//gtk_size_group_add_widget (group, hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

	/* Power switch */
	priv->power_label = gtk_label_new (_("Turn Bluetooth"));
	gtk_widget_show (priv->power_label);
	gtk_box_pack_start (GTK_BOX (hbox), priv->power_label, FALSE, FALSE, 4);

	priv->power_switch = nbtk_gtk_light_switch_new ();
	if (priv->killswitch != NULL) {
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
	} else {
		gtk_widget_set_sensitive (priv->power_switch, FALSE);
	}
	gtk_widget_show (priv->power_switch);
	g_signal_connect (priv->power_switch, "switch-flipped",
                    G_CALLBACK (power_switch_toggled_cb), self);
	gtk_box_pack_start (GTK_BOX (hbox), priv->power_switch, FALSE, FALSE, 4);

	/* Filter combo */
	priv->filter = bluetooth_filter_widget_new ();
	bluetooth_filter_widget_set_title (BLUETOOTH_FILTER_WIDGET (priv->filter), _("Only show:"));
	bluetooth_filter_widget_bind_filter (BLUETOOTH_FILTER_WIDGET (priv->filter),
					     BLUETOOTH_CHOOSER (priv->chooser));
	gtk_box_pack_start (GTK_BOX (vbox), priv->filter, FALSE, FALSE, 4);

	/* Button for Send file/PIN options */
	priv->action_button = gtk_button_new_with_label (_("Send file from your computer"));
	gtk_widget_show (priv->action_button);
	//gtk_size_group_add_widget (group, priv->action_button);
	g_signal_connect (priv->action_button, "clicked",
                    G_CALLBACK (send_file_button_clicked_cb), self);
	gtk_box_pack_start (GTK_BOX (vbox), priv->action_button, FALSE, FALSE, 4);

	/* Scanning "help" label */
	scan_text = g_strdup (_("Searching for devices.\n\n"
                          	"Please make sure they're nearby and in 'discoverable' or "
	                        "'visible' mode. Check your devices manual if you have "
	                        "problems."));
	priv->scan_label = gtk_label_new (scan_text);
	//gtk_size_group_add_widget (group, priv->scan_label);
	gtk_label_set_line_wrap (GTK_LABEL (priv->scan_label), TRUE);
	g_free (scan_text);
	gtk_box_pack_start (GTK_BOX (vbox), priv->scan_label, FALSE, FALSE, 4);

	//g_object_unref (group);
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
