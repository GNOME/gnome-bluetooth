/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
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

#include <glib/gi18n-lib.h>

#include "bluetooth-client.h"
#include "bluetooth-chooser.h"

enum {
	SELECTED_DEVICE_CHANGED,
	LAST_SIGNAL
};

static int selection_table_signals[LAST_SIGNAL] = { 0 };

#define BLUETOOTH_CHOOSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
										 BLUETOOTH_TYPE_CHOOSER, BluetoothChooserPrivate))

typedef struct _BluetoothChooserPrivate BluetoothChooserPrivate;

struct _BluetoothChooserPrivate {
	BluetoothClient *client;
	GtkTreeSelection *selection;
	GtkTreeModel *model, *filter, *adapter_model;
	GtkWidget *label;

	gulong default_adapter_changed_id;

	/* Widgets/UI bits that can be shown or hidden */
	GtkCellRenderer *bonded_cell;
	GtkCellRenderer *connected_cell;
	GtkWidget *treeview;
	GtkWidget *search_button;
	GtkWidget *device_type_label, *device_type;
	GtkWidget *device_category_label, *device_category;
	GtkWidget *filters_vbox;

	/* Current filter */
	int device_type_filter;
	GtkTreeModel *device_type_filter_model;
	int device_category_filter;

	guint show_paired : 1;
	guint show_connected : 1;
	guint show_search : 1;
	guint show_device_type : 1;
	guint show_device_category : 1;
	guint disco_rq : 1;
};

G_DEFINE_TYPE(BluetoothChooser, bluetooth_chooser, GTK_TYPE_VBOX)

enum {
	DEVICE_TYPE_FILTER_COL_NAME = 0,
	DEVICE_TYPE_FILTER_COL_MASK,
	DEVICE_TYPE_FILTER_NUM_COLS
};

static const char *
bluetooth_device_category_to_string (int type)
{
	switch (type) {
	case BLUETOOTH_CATEGORY_ALL:
		return N_("All categories");
	case BLUETOOTH_CATEGORY_PAIRED:
		return N_("Paired");
	case BLUETOOTH_CATEGORY_TRUSTED:
		return N_("Trusted");
	case BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED:
		return N_("Not paired or trusted");
	case BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED:
		return N_("Paired or trusted");
	default:
		return N_("Unknown");
	}
}

static void
bonded_to_icon (GtkTreeViewColumn *column, GtkCellRenderer *cell,
	      GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean bonded;

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PAIRED, &bonded, -1);

	g_object_set (cell, "icon-name", bonded ? "bluetooth-paired" : NULL, NULL);
}

static void
connected_to_icon (GtkTreeViewColumn *column, GtkCellRenderer *cell,
	      GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean connected;

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_CONNECTED, &connected, -1);

	g_object_set (cell, "icon-name", connected ? GTK_STOCK_CONNECT : NULL, NULL);
}

static void
type_to_text (GtkTreeViewColumn *column, GtkCellRenderer *cell,
	      GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	guint type;

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_TYPE, &type, -1);

	g_object_set (cell, "text", (type == 0) ? _("Unknown") : bluetooth_type_to_string (type), NULL);
}

/**
 * bluetooth_chooser_start_discovery:
 * @self: a #BluetoothChooser widget
 *
 * Starts a discovery on the default Bluetooth adapter. Note that this will
 * only work if the Search button is visible, as otherwise the user has no
 * visual feedback that the process is on-going.
 *
 * See also: #BluetoothChooser:show-search
 */
void
bluetooth_chooser_start_discovery (BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);

	g_return_if_fail (priv->show_search);

	if (bluetooth_client_start_discovery (priv->client) != FALSE)
		gtk_widget_set_sensitive (GTK_WIDGET(priv->search_button), FALSE);
	else
		priv->disco_rq = TRUE;
}

static char *
bluetooth_chooser_get_selected_device_data (BluetoothChooser *self, guint column)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkTreeIter iter;
	gchar *str;
	gboolean selected;

	selected = gtk_tree_selection_get_selected (priv->selection, NULL, &iter);
	if (selected == FALSE)
		return NULL;

	gtk_tree_model_get (priv->filter, &iter, column, &str, -1);
	return str;
}

/**
 * bluetooth_chooser_get_selected_device:
 * @self: a #BluetoothChooser widget
 *
 * Return value: the Bluetooth address for the currently selected device, or %NULL
 */
gchar *
bluetooth_chooser_get_selected_device (BluetoothChooser *self)
{
	return bluetooth_chooser_get_selected_device_data (self, BLUETOOTH_COLUMN_ADDRESS);
}

/**
 * bluetooth_chooser_get_selected_device_name:
 * @self: a #BluetoothChooser widget
 *
 * Return value: the name for the currently selected device, or %NULL
 */
gchar *
bluetooth_chooser_get_selected_device_name (BluetoothChooser *self)
{
	return bluetooth_chooser_get_selected_device_data (self, BLUETOOTH_COLUMN_NAME);
}

/**
 * bluetooth_chooser_get_selected_device_icon:
 * @self: a #BluetoothChooser widget
 *
 * Return value: the icon name to use to represent the currently selected device, or %NULL
 */
gchar *
bluetooth_chooser_get_selected_device_icon (BluetoothChooser *self)
{
	return bluetooth_chooser_get_selected_device_data (self, BLUETOOTH_COLUMN_ICON);
}

/**
 * bluetooth_chooser_get_selected_device_type:
 * @self: a #BluetoothChooser widget
 *
 * Return value: the type of the device selected, or '0' if unknown
 */
guint
bluetooth_chooser_get_selected_device_type (BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkTreeIter iter;
	guint type;
	gboolean selected;

	selected = gtk_tree_selection_get_selected (priv->selection, NULL, &iter);
	if (selected == FALSE)
		return 0;

	gtk_tree_model_get (priv->filter, &iter, BLUETOOTH_COLUMN_TYPE, &type, -1);
	return type;
}

/**
 * bluetooth_chooser_get_selected_device_is_connected:
 * @self: a #BluetoothChooser widget
 *
 * Return value: whether the selected device is conncted to this computer, will be %FALSE if no devices are selected
 */
gboolean
bluetooth_chooser_get_selected_device_is_connected (BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkTreeIter iter;
	gboolean selected, connected;

	selected = gtk_tree_selection_get_selected (priv->selection, NULL, &iter);
	if (selected == FALSE)
		return 0;

	gtk_tree_model_get (priv->filter, &iter, BLUETOOTH_COLUMN_CONNECTED, &connected, -1);
	return connected;
}

/**
 * bluetooth_chooser_set_title:
 * @self: a BluetoothChooser widget
 * @title: the widget header title
 */
void
bluetooth_chooser_set_title (BluetoothChooser  *self, const char *title)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);

	if (title == NULL) {
		gtk_widget_hide (priv->label);
	} else {
		char *str;

		str = g_strdup_printf ("<b>%s</b>", title);
		gtk_label_set_markup (GTK_LABEL(priv->label), str);
		g_free (str);
		gtk_widget_show (priv->label);
	}
}

static void
device_model_row_changed (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	char *name;

	/* Not the selection changing? */
	if (gtk_tree_selection_path_is_selected (priv->selection, path) == FALSE)
		return;

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_NAME, &name,
			    -1);
	/* Maybe it's the name that changed */
	if (name != NULL)
		g_object_notify (G_OBJECT (self), "device-selected-name");
	g_object_notify (G_OBJECT (self), "device-selected-is-connected");

	g_free (name);
}

static void
search_button_clicked (GtkButton *button, gpointer user_data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER(user_data);

	bluetooth_chooser_start_discovery (self);
}

static void
select_browse_device_callback (GtkTreeSelection *selection, gpointer user_data)
{
	BluetoothChooser *self = user_data;
	gchar *address;

	g_object_notify (G_OBJECT(self), "device-selected");
	address = bluetooth_chooser_get_selected_device (self);
	g_signal_emit (G_OBJECT (self),
		       selection_table_signals[SELECTED_DEVICE_CHANGED],
		       0, address);
	g_free (address);
}

static gboolean
filter_type_func (GtkTreeModel *model, GtkTreeIter *iter, BluetoothChooserPrivate *priv)
{
	int type;

	if (priv->device_type_filter == BLUETOOTH_TYPE_ANY)
		return TRUE;

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_TYPE, &type, -1);
	return (type & priv->device_type_filter);
}

static gboolean
filter_category_func (GtkTreeModel *model, GtkTreeIter *iter, BluetoothChooserPrivate *priv)
{
	gboolean bonded, trusted;

	if (priv->device_category_filter == BLUETOOTH_CATEGORY_ALL)
		return TRUE;

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_PAIRED, &bonded,
			    BLUETOOTH_COLUMN_TRUSTED, &trusted,
			    -1);

	if (priv->device_category_filter == BLUETOOTH_CATEGORY_PAIRED)
		return bonded;
	if (priv->device_category_filter == BLUETOOTH_CATEGORY_TRUSTED)
		return trusted;
	if (priv->device_category_filter == BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED)
		return (!bonded && !trusted);
	if (priv->device_category_filter == BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED)
		return (bonded || trusted);

	g_assert_not_reached ();

	return FALSE;
}

static gboolean
filter_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);

	return filter_type_func (model, iter, priv) && filter_category_func (model, iter, priv);
}

static void
filter_type_changed_cb (GtkComboBox *widget, gpointer data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (widget, &iter) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->device_type_filter_model), &iter,
			    DEVICE_TYPE_FILTER_COL_MASK, &(priv->device_type_filter),
			    -1);

	if (priv->filter)
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
	g_object_notify (G_OBJECT(self), "device-type-filter");
}

static void
set_combobox_from_filter (BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkTreeIter iter;
	gboolean cont;

	/* Look for an exact matching filter first */
	cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->device_type_filter_model),
					      &iter);
	while (cont != FALSE) {
		int mask;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->device_type_filter_model), &iter,
				    DEVICE_TYPE_FILTER_COL_MASK, &mask, -1);
		if (mask == priv->device_type_filter) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX(priv->device_type), &iter);
			return;
		}
		cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->device_type_filter_model), &iter);
	}

	/* Then a fuzzy match */
	cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->device_type_filter_model),
					      &iter);
	while (cont != FALSE) {
		int mask;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->device_type_filter_model), &iter,
				    DEVICE_TYPE_FILTER_COL_MASK, &mask,
				    -1);
		if (mask & priv->device_type_filter) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX(priv->device_type), &iter);
			return;
		}
		cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->device_type_filter_model), &iter);
	}

	/* Then just set the any then */
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->device_type_filter_model), &iter);
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX(priv->device_type), &iter);
}

static void
filter_category_changed_cb (GtkComboBox *widget, gpointer data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);

	priv->device_category_filter = gtk_combo_box_get_active (GTK_COMBO_BOX(priv->device_category));
	if (priv->filter)
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
	g_object_notify (G_OBJECT(self), "device-category-filter");
}

static void
adapter_model_row_changed (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	gboolean discovering, is_default, powered;

	/* Not an adapter changing? */
	if (gtk_tree_path_get_depth (path) != 1)
		return;

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_DEFAULT, &is_default,
			    BLUETOOTH_COLUMN_DISCOVERING, &discovering,
			    BLUETOOTH_COLUMN_POWERED, &powered,
			    -1);

	if (is_default == FALSE)
		return;
	gtk_widget_set_sensitive (GTK_WIDGET(priv->search_button), !discovering && powered);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->treeview), powered);
}

static void default_adapter_changed (GObject    *gobject,
				     GParamSpec *arg1,
				     gpointer    data)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER (data);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	char *adapter;

	g_object_get (gobject, "default-adapter", &adapter, NULL);

	if (adapter == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->treeview), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET(priv->search_button), FALSE);
		gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview), NULL);
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (adapter == NULL)
		return;

	g_free (adapter);

	priv->model = bluetooth_client_get_device_model (priv->client, NULL);
	if (priv->model) {
		priv->filter = gtk_tree_model_filter_new (priv->model, NULL);
		gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter),
							filter_func, self, NULL);
		gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview), priv->filter);
		g_signal_connect (priv->filter, "row-changed",
				  G_CALLBACK (device_model_row_changed), self);
		g_object_unref (priv->filter);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->treeview), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET(priv->search_button), TRUE);

		/* Start a disovery if it was requested before we
		 * had an adapter available */
		if (priv->disco_rq != FALSE) {
			bluetooth_chooser_start_discovery (self);
			priv->disco_rq = FALSE;
		}
	}
}

static GtkWidget *
create_treeview (BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	GtkWidget *scrolled, *tree;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Create the scrolled window */
	scrolled = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(scrolled),
					     GTK_SHADOW_OUT);

	/* Create the tree view */
	tree = gtk_tree_view_new ();

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(tree), TRUE);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW(tree), TRUE);

	g_object_set (tree, "show-expanders", FALSE, NULL);

	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_set_title (column, _("Device"));

	/* The type icon */
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_set_spacing (column, 4);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "icon-name", BLUETOOTH_COLUMN_ICON, NULL);

	/* The device name */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", BLUETOOTH_COLUMN_ALIAS, NULL);

	/* The connected icon */
	priv->connected_cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, priv->connected_cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (column, priv->connected_cell,
						 connected_to_icon, NULL, NULL);
	g_object_set (G_OBJECT (priv->connected_cell), "visible", priv->show_connected, NULL);

	/* The bonded icon */
	priv->bonded_cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_end (column, priv->bonded_cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (column, priv->bonded_cell,
						 bonded_to_icon, NULL, NULL);
	g_object_set (G_OBJECT (priv->bonded_cell), "visible", priv->show_paired, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);

	gtk_tree_view_column_set_min_width (GTK_TREE_VIEW_COLUMN(column), 280);

	gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW(tree), -1,
						    _("Type"), gtk_cell_renderer_text_new(),
						    type_to_text, NULL, NULL);

	priv->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree));

	gtk_tree_selection_set_mode (priv->selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT(priv->selection), "changed",
			  G_CALLBACK(select_browse_device_callback), self);

	/* Set the model, and filter */
	priv->model = bluetooth_client_get_device_model (priv->client, NULL);
	if (priv->model) {
		priv->filter = gtk_tree_model_filter_new (priv->model, NULL);
		gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter),
							filter_func, self, NULL);
		gtk_tree_view_set_model (GTK_TREE_VIEW(tree), priv->filter);
		g_signal_connect (priv->filter, "row-changed",
				  G_CALLBACK (device_model_row_changed), self);
		g_object_unref (priv->filter);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (tree), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->search_button), FALSE);
	}

	gtk_container_add (GTK_CONTAINER(scrolled), tree);
	priv->treeview = tree;

	return scrolled;
}

static void
bluetooth_chooser_init(BluetoothChooser *self)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(self);
	char *str;
	int i;

	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *scrolled_window;
	GtkWidget *table;
	GtkCellRenderer *renderer;

	priv->show_paired = FALSE;
	priv->show_search = FALSE;

	priv->client = bluetooth_client_new ();

	/* Setup the widget itself */
	gtk_box_set_spacing (GTK_BOX(self), 18);
	gtk_container_set_border_width (GTK_CONTAINER(self), 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (self), vbox, TRUE, TRUE, 0);

	/* The top level label */
	priv->label = gtk_label_new ("");
	gtk_widget_set_no_show_all (priv->label, TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), priv->label, FALSE, TRUE, 0);
	gtk_label_set_use_markup (GTK_LABEL (priv->label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0, 0.5);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

	/* The treeview label */
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	hbox = gtk_hbox_new (FALSE, 24);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	/* Setup the adapter disco mode callback for the search button */
	priv->adapter_model = bluetooth_client_get_adapter_model (priv->client);
	g_signal_connect (priv->adapter_model, "row-changed",
			  G_CALLBACK (adapter_model_row_changed), self);

	/* The search button */
	priv->search_button = gtk_button_new_with_mnemonic (_("S_earch"));
	gtk_widget_set_no_show_all (priv->search_button, TRUE);
	gtk_box_pack_end (GTK_BOX (hbox), priv->search_button, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT(priv->search_button), "clicked",
			  G_CALLBACK(search_button_clicked), self);
	gtk_widget_set_tooltip_text (priv->search_button, _("Search for Bluetooth devices"));
	if (priv->show_search)
		gtk_widget_show (priv->search_button);
	//FIXME check whether the default adapter is discovering right now

	/* The treeview */
	scrolled_window = create_treeview (self);
	gtk_widget_show_all (scrolled_window);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (self), vbox, FALSE, TRUE, 0);
	priv->filters_vbox = vbox;
	gtk_widget_set_no_show_all (priv->filters_vbox, TRUE);

	/* The filters */
	str = g_strdup_printf ("<b>%s</b>", _("Show Only Bluetooth Devices With..."));
	label = gtk_label_new (str);
	g_free (str);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

	table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);

	/* The device category filter */
	label = gtk_label_new_with_mnemonic (_("Device _category:"));
	gtk_widget_set_no_show_all (label, TRUE);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	priv->device_category_label = label;

	priv->device_category = gtk_combo_box_new_text ();
	gtk_widget_set_no_show_all (priv->device_category, TRUE);
	gtk_widget_show (priv->device_category);
	gtk_table_attach (GTK_TABLE (table), priv->device_category, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_widget_set_tooltip_text (priv->device_category, _("Select the device category to filter above list"));
	for (i = 0; i < BLUETOOTH_CATEGORY_NUM_CATEGORIES; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX(priv->device_category),
					   _(bluetooth_device_category_to_string (i)));
	}
	g_signal_connect (G_OBJECT (priv->device_category), "changed",
			  G_CALLBACK(filter_category_changed_cb), self);
	gtk_combo_box_set_active (GTK_COMBO_BOX(priv->device_category), priv->device_category_filter);
	if (priv->show_device_category) {
		gtk_widget_show (priv->device_category_label);
		gtk_widget_show (priv->device_category);
	}

	/* The device type filter */
	label = gtk_label_new_with_mnemonic (_("Device _type:"));
	gtk_widget_set_no_show_all (label, TRUE);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	priv->device_type_label = label;

	priv->device_type_filter_model = GTK_TREE_MODEL (gtk_list_store_new (DEVICE_TYPE_FILTER_NUM_COLS,
									     G_TYPE_STRING, G_TYPE_INT));
	priv->device_type = gtk_combo_box_new_with_model (priv->device_type_filter_model);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->device_type), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->device_type), renderer, "text", DEVICE_TYPE_FILTER_COL_NAME);

	gtk_widget_set_no_show_all (priv->device_type, TRUE);
	gtk_widget_show (priv->device_type);
	gtk_table_attach (GTK_TABLE (table), priv->device_type, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_widget_set_tooltip_text (priv->device_type, _("Select the device type to filter above list"));
	gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _(bluetooth_type_to_string (BLUETOOTH_TYPE_ANY)),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_ANY,
					   -1);
	gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _("Input devices (mice, keyboards, ...)"),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_INPUT,
					   -1);
	gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _("Headphones, headsets and other audio devices"),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_AUDIO,
					   -1);
	/* The types match the types used in bluetooth-client.h */
	for (i = 1; i < _BLUETOOTH_TYPE_NUM_TYPES; i++) {
		int mask = 1 << i;
		if (mask & BLUETOOTH_TYPE_INPUT || mask & BLUETOOTH_TYPE_AUDIO)
			continue;
		gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->device_type_filter_model), NULL, G_MAXUINT32,
						   DEVICE_TYPE_FILTER_COL_NAME, _(bluetooth_type_to_string (mask)),
						   DEVICE_TYPE_FILTER_COL_MASK, mask,
						   -1);
	}
	g_signal_connect (G_OBJECT (priv->device_type), "changed",
			  G_CALLBACK(filter_type_changed_cb), self);
	set_combobox_from_filter (self);
	if (priv->show_device_type) {
		gtk_widget_show (priv->device_type_label);
		gtk_widget_show (priv->device_type);
	}

	/* if filters are not visible hide the vbox */
	if (!priv->show_device_type && !priv->show_device_category)
		gtk_widget_hide (priv->filters_vbox);

	priv->default_adapter_changed_id = g_signal_connect (priv->client, "notify::default-adapter",
							     G_CALLBACK (default_adapter_changed), self);
}

static void
bluetooth_chooser_finalize (GObject *object)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(object);

	if (priv->client) {
		g_signal_handler_disconnect (G_OBJECT(priv->client), priv->default_adapter_changed_id);
		priv->default_adapter_changed_id = 0;

		bluetooth_client_stop_discovery (priv->client);
		g_object_unref (priv->client);
		priv->client = NULL;
	}
	if (priv->adapter_model) {
		g_object_unref (priv->adapter_model);
		priv->adapter_model = NULL;
	}
	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	G_OBJECT_CLASS(bluetooth_chooser_parent_class)->finalize(object);
}

enum {
	PROP_0,
	PROP_TITLE,
	PROP_DEVICE_SELECTED,
	PROP_DEVICE_SELECTED_ICON,
	PROP_DEVICE_SELECTED_NAME,
	PROP_DEVICE_SELECTED_TYPE,
	PROP_DEVICE_SELECTED_IS_CONNECTED,
	PROP_DEVICE_SELECTED_PROXY,
	PROP_SHOW_PAIRING,
	PROP_SHOW_CONNECTED,
	PROP_SHOW_SEARCH,
	PROP_SHOW_DEVICE_TYPE,
	PROP_SHOW_DEVICE_CATEGORY,
	PROP_DEVICE_TYPE_FILTER,
	PROP_DEVICE_CATEGORY_FILTER
};

static void
bluetooth_chooser_set_property (GObject *object, guint prop_id,
					 const GValue *value, GParamSpec *pspec)
{
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_TITLE:
		bluetooth_chooser_set_title (BLUETOOTH_CHOOSER (object), g_value_get_string (value));
		break;
	case PROP_SHOW_PAIRING:
		priv->show_paired = g_value_get_boolean (value);
		if (priv->bonded_cell != NULL)
			g_object_set (G_OBJECT (priv->bonded_cell), "visible", priv->show_paired, NULL);
		break;
	case PROP_SHOW_CONNECTED:
		priv->show_connected = g_value_get_boolean (value);
		if (priv->connected_cell != NULL)
			g_object_set (G_OBJECT (priv->connected_cell), "visible", priv->show_connected, NULL);
		break;
	case PROP_SHOW_SEARCH:
		priv->show_search = g_value_get_boolean (value);
		g_object_set (G_OBJECT (priv->search_button), "visible", priv->show_search, NULL);
		break;
	case PROP_SHOW_DEVICE_TYPE:
		priv->show_device_type = g_value_get_boolean (value);
		g_object_set (G_OBJECT (priv->device_type_label), "visible", priv->show_device_type, NULL);
		g_object_set (G_OBJECT (priv->device_type), "visible", priv->show_device_type, NULL);
		if (priv->show_device_type || priv->show_device_category)
			g_object_set (G_OBJECT (priv->filters_vbox), "visible", TRUE, NULL);
		else
			g_object_set (G_OBJECT (priv->filters_vbox), "visible", FALSE, NULL);
		break;
	case PROP_SHOW_DEVICE_CATEGORY:
		priv->show_device_category = g_value_get_boolean (value);
		g_object_set (G_OBJECT (priv->device_category_label), "visible", priv->show_device_category, NULL);
		g_object_set (G_OBJECT (priv->device_category), "visible", priv->show_device_category, NULL);
		if (priv->show_device_type || priv->show_device_category)
			g_object_set (G_OBJECT (priv->filters_vbox), "visible", TRUE, NULL);
		else
			g_object_set (G_OBJECT (priv->filters_vbox), "visible", FALSE, NULL);
		break;
	case PROP_DEVICE_TYPE_FILTER:
		priv->device_type_filter = g_value_get_int (value);
		set_combobox_from_filter (BLUETOOTH_CHOOSER (object));
		break;
	case PROP_DEVICE_CATEGORY_FILTER:
		priv->device_category_filter = g_value_get_int (value);
		gtk_combo_box_set_active (GTK_COMBO_BOX(priv->device_category), priv->device_category_filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
bluetooth_chooser_get_property (GObject *object, guint prop_id,
					 GValue *value, GParamSpec *pspec)
{
	BluetoothChooser *self = BLUETOOTH_CHOOSER(object);
	BluetoothChooserPrivate *priv = BLUETOOTH_CHOOSER_GET_PRIVATE(object);

	switch (prop_id) {
	case PROP_DEVICE_SELECTED:
		g_value_take_string (value, bluetooth_chooser_get_selected_device (self));
		break;
	case PROP_DEVICE_SELECTED_NAME:
		g_value_take_string (value, bluetooth_chooser_get_selected_device_name (self));
		break;
	case PROP_DEVICE_SELECTED_ICON:
		g_value_take_string (value, bluetooth_chooser_get_selected_device_icon (self));
		break;
	case PROP_DEVICE_SELECTED_TYPE:
		g_value_set_uint (value, bluetooth_chooser_get_selected_device_type (self));
		break;
	case PROP_DEVICE_SELECTED_IS_CONNECTED:
		g_value_set_boolean (value, bluetooth_chooser_get_selected_device_is_connected (self));
		break;
	case PROP_DEVICE_SELECTED_PROXY: {
		GtkTreeIter iter;
		GObject *proxy = NULL;

		if (gtk_tree_selection_get_selected (priv->selection, NULL, &iter) != FALSE)
			gtk_tree_model_get (priv->filter, &iter, BLUETOOTH_COLUMN_PROXY, &proxy, -1);
		g_value_take_object (value, proxy);
		break;
	}
	case PROP_SHOW_PAIRING:
		g_value_set_boolean (value, priv->show_paired);
		break;
	case PROP_SHOW_CONNECTED:
		g_value_set_boolean (value, priv->show_connected);
		break;
	case PROP_SHOW_SEARCH:
		g_value_set_boolean (value, priv->show_search);
		break;
	case PROP_SHOW_DEVICE_TYPE:
		g_value_set_boolean (value, priv->show_device_type);
		break;
	case PROP_SHOW_DEVICE_CATEGORY:
		g_value_set_boolean (value, priv->show_device_category);
		break;
	case PROP_DEVICE_TYPE_FILTER:
		g_value_set_int (value, priv->device_type_filter);
		break;
	case PROP_DEVICE_CATEGORY_FILTER:
		g_value_set_int (value, priv->device_category_filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
bluetooth_chooser_class_init (BluetoothChooserClass *klass)
{
	/* Use to calculate the maximum value for the 
	 * device-type-filter value */
	guint i;
	int max_filter_val;

	g_type_class_add_private(klass, sizeof(BluetoothChooserPrivate));

	G_OBJECT_CLASS(klass)->finalize = bluetooth_chooser_finalize;

	G_OBJECT_CLASS(klass)->set_property = bluetooth_chooser_set_property;
	G_OBJECT_CLASS(klass)->get_property = bluetooth_chooser_get_property;

	/**
	 * BluetoothChooser::selected-device-changed:
	 *
	 * @bluetoothchooser: a #BluetoothChooser widget
	 * @arg1: the Bluetooth address for the currently selected device, or %NULL
	 *
	 * The #BluetoothChooser:selected-device-changed signal is launched when the
	 * selected device is changed, it will be %NULL is a device was unselected.
	 **/
	selection_table_signals[SELECTED_DEVICE_CHANGED] =
		g_signal_new ("selected-device-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BluetoothChooserClass, selected_device_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * BluetoothChooser:title:
	 *
	 * The widget header title.
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_TITLE, g_param_spec_string ("title",
									  NULL, NULL, NULL, G_PARAM_WRITABLE));
	/**
	 * BluetoothChooser:device-selected:
	 *
	 * the Bluetooth address for the currently selected device, or %NULL
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED, g_param_spec_string ("device-selected",
										    NULL, NULL, NULL, G_PARAM_READABLE));
	/**
	 * BluetoothChooser:device-selected-icon:
	 *
	 * the icon name to use to represent the currently selected device, or %NULL
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED_ICON, g_param_spec_string ("device-selected-icon",
										    NULL, NULL, NULL, G_PARAM_READABLE));
	/**
	 * BluetoothChooser:device-selected-name:
	 *
	 * the name for the currently selected device
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED_NAME, g_param_spec_string ("device-selected-name",
										    NULL, NULL, NULL, G_PARAM_READABLE));
	/**
	 * BluetoothChooser:device-selected-type:
	 *
	 * the currently selected device's type, or 0
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED_TYPE, g_param_spec_uint ("device-selected-type", NULL, NULL,
										       1, 1 << (_BLUETOOTH_TYPE_NUM_TYPES - 1), 1, G_PARAM_READABLE));
	/**
	 * BluetoothChooser:device-selected-is-connected:
	 *
	 * whether the selected device is conncted to this computer, will be %FALSE if no devices are selected
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED_IS_CONNECTED, g_param_spec_boolean ("device-selected-is-connected", NULL, NULL,
												  FALSE, G_PARAM_READABLE));
	/* Left blank intentionally */
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SELECTED_PROXY, g_param_spec_object ("device-selected-proxy", NULL, NULL,
												  G_TYPE_OBJECT, G_PARAM_READABLE));
	/**
	 * BluetoothChooser:show-pairing:
	 *
	 * Whether to show the pairing column in the tree.
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_PAIRING, g_param_spec_boolean ("show-pairing",
										  NULL, NULL, FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:show-connected:
	 *
	 * Whether to show the connected column in the tree.
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_CONNECTED, g_param_spec_boolean ("show-connected",
										    NULL, NULL, FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:show-search:
	 *
	 * Whether to show the Search button, this is necessary if you want to programmatically
	 * start a discovery, using bluetooth_chooser_start_discovery()
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_SEARCH, g_param_spec_boolean ("show-search",
										 NULL, NULL, FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:show-device-type:
	 *
	 * Whether to show the device type filter
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_DEVICE_TYPE, g_param_spec_boolean ("show-device-type",
										      NULL, NULL, TRUE, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:show-device-category:
	 *
	 * Whether to show the device category filter
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_DEVICE_CATEGORY, g_param_spec_boolean ("show-device-category",
											  NULL, NULL, TRUE, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:device-type-filter:
	 *
	 * FIXME
	 **/
	for (i = 0, max_filter_val = 0 ; i < _BLUETOOTH_TYPE_NUM_TYPES; i++)
		max_filter_val += 1 << i;
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_TYPE_FILTER, g_param_spec_int ("device-type-filter", NULL, NULL,
										    1, max_filter_val, 1, G_PARAM_READWRITE));
	/**
	 * BluetoothChooser:device-category-filter:
	 *
	 * FIXME
	 **/
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_CATEGORY_FILTER, g_param_spec_int ("device-category-filter", NULL, NULL,
					 						0, BLUETOOTH_CATEGORY_NUM_CATEGORIES, 0, G_PARAM_READWRITE));
}

/**
 * bluetooth_chooser_new:
 * @title: the widget header title
 *
 * Return value: A #BluetoothChooser widget
 **/
GtkWidget *
bluetooth_chooser_new (const gchar *title)
{
	return g_object_new(BLUETOOTH_TYPE_CHOOSER,
			    "title", title,
			    NULL);
}

