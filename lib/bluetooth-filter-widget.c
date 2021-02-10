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

#include "bluetooth-filter-widget.h"
#include "bluetooth-client.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

struct _BluetoothFilterWidget {
	GtkBox parent;

	GtkWidget *device_type_label, *device_type;
	GtkWidget *device_category_label, *device_category;
	GtkWidget *title;
	GtkWidget *chooser;
	GtkTreeModel *filter;

	/* Current filter */
	int device_type_filter;
	GtkTreeModel *device_type_filter_model;
	int device_category_filter;
	char *device_service_filter;

	guint show_device_type : 1;
	guint show_device_category : 1;
};

G_DEFINE_TYPE(BluetoothFilterWidget, bluetooth_filter_widget, GTK_TYPE_BOX)

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
set_combobox_from_filter (BluetoothFilterWidget *filter)
{
	GtkTreeIter iter;
	gboolean cont;

	/* Look for an exact matching filter first */
	cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filter->device_type_filter_model),
					      &iter);
	while (cont != FALSE) {
		int mask;

		gtk_tree_model_get (GTK_TREE_MODEL (filter->device_type_filter_model), &iter,
				    DEVICE_TYPE_FILTER_COL_MASK, &mask, -1);
		if (mask == filter->device_type_filter) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX(filter->device_type), &iter);
			return;
		}
		cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (filter->device_type_filter_model), &iter);
	}

	/* Then a fuzzy match */
	cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filter->device_type_filter_model),
					      &iter);
	while (cont != FALSE) {
		int mask;

		gtk_tree_model_get (GTK_TREE_MODEL (filter->device_type_filter_model), &iter,
				    DEVICE_TYPE_FILTER_COL_MASK, &mask,
				    -1);
		if (mask & filter->device_type_filter) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX(filter->device_type), &iter);
			return;
		}
		cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (filter->device_type_filter_model), &iter);
	}

	/* Then just set the any then */
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filter->device_type_filter_model), &iter);
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX(filter->device_type), &iter);
}

static void
filter_category_changed_cb (GtkComboBox *widget, gpointer data)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (data);

	filter->device_category_filter = gtk_combo_box_get_active (GTK_COMBO_BOX(filter->device_category));
	if (filter->filter)
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filter->filter));
	g_object_notify (G_OBJECT(filter), "device-category-filter");
}

static void
filter_type_changed_cb (GtkComboBox *widget, gpointer data)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (data);
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (widget, &iter) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (filter->device_type_filter_model), &iter,
			    DEVICE_TYPE_FILTER_COL_MASK, &(filter->device_type_filter),
			    -1);

	if (filter->filter)
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filter->filter));
	g_object_notify (G_OBJECT(filter), "device-type-filter");
}

/**
 * bluetooth_filter_widget_set_title:
 * @filter: a #BluetoothFilterWidget.
 * @title: Title for the #BluetoothFilterWidget.
 *
 * Used to set a different title for the #BluetoothFilterWidget than the default.
 *
 **/
void
bluetooth_filter_widget_set_title (BluetoothFilterWidget *filter, gchar *title)
{
	gtk_label_set_text (GTK_LABEL (filter->title), title);
}

static void
bluetooth_filter_widget_bind_chooser_single (BluetoothFilterWidget *filter,
					     BluetoothChooser *chooser,
					     const char *property)
{
	/* NOTE: We are binding the chooser as the source so that all of its
	 * properties are pushed to the filter.
	 * The bindings will be automatically removed when one of the
	 * objects goes away */
	g_object_bind_property ((gpointer) chooser, property,
				(gpointer) filter, property,
				G_BINDING_BIDIRECTIONAL);
}

/**
 * bluetooth_filter_widget_bind_filter:
 * @filter: a #BluetoothFilterWidget.
 * @chooser: The #BluetoothChooser widget to bind the filter to.
 *
 * Binds a #BluetoothFilterWidget to a #BluetoothChooser such that changing the
 * #BluetoothFilterWidget results in filters being applied on the #BluetoothChooser.
 * Any properties set on a bound #BluetoothChooser will also be set on the
 * #BluetoothFilterWidget.
 *
 **/
void
bluetooth_filter_widget_bind_filter (BluetoothFilterWidget *filter, BluetoothChooser *chooser)
{
	bluetooth_filter_widget_bind_chooser_single (filter, chooser, "device-type-filter");
	bluetooth_filter_widget_bind_chooser_single (filter, chooser, "device-category-filter");
	bluetooth_filter_widget_bind_chooser_single (filter, chooser, "show-device-type");
	bluetooth_filter_widget_bind_chooser_single (filter, chooser, "show-device-category");
}

static void
bluetooth_filter_widget_init(BluetoothFilterWidget *filter)
{
	guint i;

	GtkWidget *label;
	GtkWidget *alignment;
	GtkWidget *table;
	GtkCellRenderer *renderer;

	g_object_set (G_OBJECT (filter), "orientation", GTK_ORIENTATION_VERTICAL, NULL);

	gtk_box_set_homogeneous (GTK_BOX (filter), FALSE);
	gtk_box_set_spacing (GTK_BOX (filter), 6);

	filter->title = gtk_label_new ("");
	/* This is the title of the filter section of the Bluetooth device chooser.
	 * It used to say Show Only Bluetooth Devices With... */
	bluetooth_filter_widget_set_title (filter, _("Show:"));
	gtk_widget_show (filter->title);
	gtk_box_pack_start (GTK_BOX (filter), filter->title, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (filter->title), 0, 0.5);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (filter), alignment, TRUE, TRUE, 0);

	table = gtk_grid_new ();
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_grid_set_row_spacing (GTK_GRID (table), 6);
	gtk_grid_set_column_spacing (GTK_GRID (table), 12);

	/* The device category filter */
	label = gtk_label_new_with_mnemonic (_("Device _category:"));
	gtk_widget_set_no_show_all (label, TRUE);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	filter->device_category_label = label;

	filter->device_category = gtk_combo_box_text_new ();
	gtk_widget_set_no_show_all (filter->device_category, TRUE);
	gtk_widget_show (filter->device_category);
	gtk_grid_attach (GTK_GRID (table), filter->device_category, 1, 0, 1, 1);
	gtk_widget_set_tooltip_text (filter->device_category, _("Select the device category to filter"));
	for (i = 0; i < BLUETOOTH_CATEGORY_NUM_CATEGORIES; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (filter->device_category),
					        _(bluetooth_device_category_to_string (i)));
	}
	g_signal_connect (G_OBJECT (filter->device_category), "changed",
			  G_CALLBACK (filter_category_changed_cb), filter);
	gtk_combo_box_set_active (GTK_COMBO_BOX (filter->device_category), filter->device_category_filter);
	if (filter->show_device_category) {
		gtk_widget_show (filter->device_category_label);
		gtk_widget_show (filter->device_category);
	}

	/* The device type filter */
	label = gtk_label_new_with_mnemonic (_("Device _type:"));
	gtk_widget_set_no_show_all (label, TRUE);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	filter->device_type_label = label;

	filter->device_type_filter_model = GTK_TREE_MODEL (gtk_list_store_new (DEVICE_TYPE_FILTER_NUM_COLS,
									     G_TYPE_STRING, G_TYPE_INT));
	filter->device_type = gtk_combo_box_new_with_model (filter->device_type_filter_model);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (filter->device_type), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (filter->device_type), renderer, "text", DEVICE_TYPE_FILTER_COL_NAME);

	gtk_widget_set_no_show_all (filter->device_type, TRUE);
	gtk_widget_show (filter->device_type);
	gtk_grid_attach (GTK_GRID (table), filter->device_type, 1, 1, 1, 1);
	gtk_widget_set_tooltip_text (filter->device_type, _("Select the device type to filter"));
	gtk_list_store_insert_with_values (GTK_LIST_STORE (filter->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _(bluetooth_type_to_filter_string (BLUETOOTH_TYPE_ANY)),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_ANY,
					   -1);
	gtk_list_store_insert_with_values (GTK_LIST_STORE (filter->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _("Input devices (mice, keyboards, etc.)"),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_INPUT,
					   -1);
	gtk_list_store_insert_with_values (GTK_LIST_STORE (filter->device_type_filter_model), NULL, G_MAXUINT32,
					   DEVICE_TYPE_FILTER_COL_NAME, _("Headphones, headsets and other audio devices"),
					   DEVICE_TYPE_FILTER_COL_MASK, BLUETOOTH_TYPE_AUDIO,
					   -1);
	/* The types match the types used in bluetooth-client.h */
	for (i = 1; i < _BLUETOOTH_TYPE_NUM_TYPES; i++) {
		int mask = 1 << i;
		if (mask & BLUETOOTH_TYPE_INPUT || mask & BLUETOOTH_TYPE_AUDIO)
			continue;
		gtk_list_store_insert_with_values (GTK_LIST_STORE (filter->device_type_filter_model), NULL, G_MAXUINT32,
						   DEVICE_TYPE_FILTER_COL_NAME, _(bluetooth_type_to_filter_string (mask)),
						   DEVICE_TYPE_FILTER_COL_MASK, mask,
						   -1);
	}
	g_signal_connect (G_OBJECT (filter->device_type), "changed",
			  G_CALLBACK(filter_type_changed_cb), filter);
	set_combobox_from_filter (filter);
	if (filter->show_device_type) {
		gtk_widget_show (filter->device_type_label);
		gtk_widget_show (filter->device_type);
	}

	/* The services filter */
	filter->device_service_filter = NULL;
}

static void
bluetooth_filter_widget_finalize (GObject *object)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (object);

	g_clear_pointer (&filter->device_service_filter, g_free);

	G_OBJECT_CLASS(bluetooth_filter_widget_parent_class)->finalize(object);
}

static void
bluetooth_filter_widget_dispose (GObject *object)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (object);

	g_clear_object (&filter->chooser);

	G_OBJECT_CLASS(bluetooth_filter_widget_parent_class)->dispose(object);
}

enum {
	PROP_0,
	PROP_SHOW_DEVICE_TYPE,
	PROP_SHOW_DEVICE_CATEGORY,
	PROP_DEVICE_TYPE_FILTER,
	PROP_DEVICE_CATEGORY_FILTER,
	PROP_DEVICE_SERVICE_FILTER
};

static void
bluetooth_filter_widget_set_property (GObject *object, guint prop_id,
					 const GValue *value, GParamSpec *pspec)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (object);

	switch (prop_id) {
	case PROP_SHOW_DEVICE_TYPE:
		filter->show_device_type = g_value_get_boolean (value);
		g_object_set (G_OBJECT (filter->device_type_label), "visible", filter->show_device_type, NULL);
		g_object_set (G_OBJECT (filter->device_type), "visible", filter->show_device_type, NULL);
		break;
	case PROP_SHOW_DEVICE_CATEGORY:
		filter->show_device_category = g_value_get_boolean (value);
		g_object_set (G_OBJECT (filter->device_category_label), "visible", filter->show_device_category, NULL);
		g_object_set (G_OBJECT (filter->device_category), "visible", filter->show_device_category, NULL);
		break;
	case PROP_DEVICE_TYPE_FILTER:
		filter->device_type_filter = g_value_get_int (value);
		set_combobox_from_filter (BLUETOOTH_FILTER_WIDGET (object));
		break;
	case PROP_DEVICE_CATEGORY_FILTER:
		filter->device_category_filter = g_value_get_enum (value);
		gtk_combo_box_set_active (GTK_COMBO_BOX(filter->device_category), filter->device_category_filter);
		break;
	case PROP_DEVICE_SERVICE_FILTER:
		g_free (filter->device_service_filter);
		filter->device_service_filter = g_value_dup_string (value);
		if (filter->filter)
			gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filter->filter));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
bluetooth_filter_widget_get_property (GObject *object, guint prop_id,
					 GValue *value, GParamSpec *pspec)
{
	BluetoothFilterWidget *filter = BLUETOOTH_FILTER_WIDGET (object);

	switch (prop_id) {
	case PROP_SHOW_DEVICE_TYPE:
		g_value_set_boolean (value, filter->show_device_type);
		break;
	case PROP_SHOW_DEVICE_CATEGORY:
		g_value_set_boolean (value, filter->show_device_category);
		break;
	case PROP_DEVICE_TYPE_FILTER:
		g_value_set_int (value, filter->device_type_filter);
		break;
	case PROP_DEVICE_CATEGORY_FILTER:
		g_value_set_enum (value, filter->device_category_filter);
		break;
	case PROP_DEVICE_SERVICE_FILTER:
		g_value_set_string (value, filter->device_service_filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
bluetooth_filter_widget_class_init (BluetoothFilterWidgetClass *klass)
{
	/* Use to calculate the maximum value for the
	 * device-type-filter value */
	guint i;
	int max_filter_val;

	G_OBJECT_CLASS(klass)->dispose = bluetooth_filter_widget_dispose;
	G_OBJECT_CLASS(klass)->finalize = bluetooth_filter_widget_finalize;

	G_OBJECT_CLASS(klass)->set_property = bluetooth_filter_widget_set_property;
	G_OBJECT_CLASS(klass)->get_property = bluetooth_filter_widget_get_property;

	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_DEVICE_TYPE, g_param_spec_boolean ("show-device-type",
										      "show-device-type", "Whether to show the device type filter", TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_SHOW_DEVICE_CATEGORY, g_param_spec_boolean ("show-device-category",
											  "show-device-category", "Whether to show the device category filter", TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	for (i = 0, max_filter_val = 0 ; i < _BLUETOOTH_TYPE_NUM_TYPES; i++)
		max_filter_val += 1 << i;
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_TYPE_FILTER, g_param_spec_int ("device-type-filter",
										    "device-type-filter", "A bitmask of #BluetoothType to show", 1, max_filter_val, 1, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_CATEGORY_FILTER, g_param_spec_enum ("device-category-filter",
											 "device-category-filter", "The #BluetoothCategory to show", BLUETOOTH_TYPE_CATEGORY, BLUETOOTH_CATEGORY_ALL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_DEVICE_SERVICE_FILTER, g_param_spec_string ("device-service-filter",
											  "device-service-filter", "A string representing the service to filter for", NULL, G_PARAM_WRITABLE));
}

/**
 * bluetooth_filter_widget_new:
 *
 * Creates a new #BluetoothFilterWidget which can be bound to a #BluetoothChooser to
 * control filtering of that #BluetoothChooser.
 * Usually used in conjunction with a #BluetoothChooser which has the "has-internal-filter"
 * property set to FALSE.
 *
 * Return value: A #BluetoothFilterWidget widget
 * 
 * Note: Must call bluetooth_filter_widget_bind_filter () to bind the #BluetoothFilterWidget
 * to a #BluetoothChooser.
 **/
GtkWidget *
bluetooth_filter_widget_new (void)
{
	return g_object_new(BLUETOOTH_TYPE_FILTER_WIDGET,
			    NULL);
}
