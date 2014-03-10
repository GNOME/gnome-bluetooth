/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
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

#include "bluetooth-settings-row.h"
#include "bluetooth-enums.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

#define BLUETOOTH_SETTINGS_ROW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), BLUETOOTH_TYPE_SETTINGS_ROW, BluetoothSettingsRowPrivate))

typedef struct _BluetoothSettingsRowPrivate BluetoothSettingsRowPrivate;

struct _BluetoothSettingsRowPrivate {
	/* Widget */
	GtkWidget *label;
	GtkWidget *status;
	GtkWidget *spinner;

	/* Properties */
	GDBusProxy *proxy;
	gboolean paired;
	gboolean trusted;
	BluetoothType type;
	gboolean connected;
	char *name;
	char *bdaddr;
	gboolean legacy_pairing;

	gboolean pairing;
};

enum {
	PROP_0,
	PROP_PROXY,
	PROP_PAIRED,
	PROP_TRUSTED,
	PROP_TYPE,
	PROP_CONNECTED,
	PROP_NAME,
	PROP_ADDRESS,
	PROP_PAIRING,
	PROP_LEGACY_PAIRING
};

G_DEFINE_TYPE(BluetoothSettingsRow, bluetooth_settings_row, GTK_TYPE_LIST_BOX_ROW)

static void
label_might_change (BluetoothSettingsRow *self)
{
	BluetoothSettingsRowPrivate *priv = BLUETOOTH_SETTINGS_ROW_GET_PRIVATE (self);

	if (!priv->paired && !priv->trusted)
		gtk_label_set_text (GTK_LABEL (priv->status), _("Not Set Up"));
	else if (priv->connected)
		gtk_label_set_text (GTK_LABEL (priv->status), _("Connected"));
	else
		gtk_label_set_text (GTK_LABEL (priv->status), _("Disconnected"));

	if (priv->pairing)
		gtk_widget_show (priv->spinner);
	else
		gtk_widget_show (priv->status);
}

static void
bluetooth_settings_row_init (BluetoothSettingsRow *self)
{
	BluetoothSettingsRowPrivate *priv = BLUETOOTH_SETTINGS_ROW_GET_PRIVATE (self);
	GtkWidget *box;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
	gtk_container_add (GTK_CONTAINER (self), box);

	/* Name is already escaped */
	priv->label = gtk_label_new ("Placeholder Name");
	gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0, 0.5);
	gtk_widget_set_margin_start (priv->label, 20);
	gtk_widget_set_margin_end (priv->label, 20);
	gtk_widget_set_margin_top (priv->label, 6);
	gtk_widget_set_margin_bottom (priv->label, 6);
	gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);

	/* Spinner */
	priv->spinner = gtk_spinner_new ();
	g_object_bind_property (priv->spinner, "visible",
				priv->spinner, "active", 0);
	gtk_widget_set_margin_start (priv->spinner, 24);
	gtk_widget_set_margin_end (priv->spinner, 24);
	gtk_box_pack_start (GTK_BOX (box), priv->spinner, FALSE, TRUE, 0);
	gtk_widget_set_no_show_all (priv->spinner, TRUE);
	g_object_set_data (G_OBJECT (self), "spinner", priv->spinner);

	/* Placeholder text */
	priv->status = gtk_label_new (_("Not Set Up"));

	gtk_widget_set_no_show_all (priv->status, TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->status), 1, 0.5);
	gtk_widget_set_margin_start (priv->status, 24);
	gtk_widget_set_margin_end (priv->status, 24);
	gtk_box_pack_start (GTK_BOX (box), priv->status, FALSE, TRUE, 0);
	g_object_bind_property (priv->spinner, "visible",
				priv->status, "visible", G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL);

	gtk_widget_show (priv->status);
	gtk_widget_show_all (GTK_WIDGET (self));
}

static void
bluetooth_settings_row_finalize (GObject *object)
{
	BluetoothSettingsRowPrivate *priv = BLUETOOTH_SETTINGS_ROW_GET_PRIVATE (object);

	g_clear_object (&priv->proxy);
	g_clear_pointer (&priv->name, g_free);
	g_clear_pointer (&priv->bdaddr, g_free);

	G_OBJECT_CLASS(bluetooth_settings_row_parent_class)->finalize(object);
}

static void
bluetooth_settings_row_get_property (GObject        *object,
				     guint           property_id,
				     GValue         *value,
				     GParamSpec     *pspec)
{
	BluetoothSettingsRowPrivate *priv = BLUETOOTH_SETTINGS_ROW_GET_PRIVATE (object);

	switch (property_id) {
	case PROP_PROXY:
		g_value_set_object (value, priv->proxy);
		break;
	case PROP_PAIRED:
		g_value_set_boolean (value, priv->paired);
		break;
	case PROP_TRUSTED:
		g_value_set_boolean (value, priv->trusted);
		break;
	case PROP_TYPE:
		g_value_set_flags (value, priv->type);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_ADDRESS:
		g_value_set_string (value, priv->bdaddr);
		break;
	case PROP_PAIRING:
		g_value_set_boolean (value, priv->pairing);
		break;
	case PROP_LEGACY_PAIRING:
		g_value_set_boolean (value, priv->legacy_pairing);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
bluetooth_settings_row_set_property (GObject        *object,
				     guint           property_id,
				     const GValue   *value,
				     GParamSpec     *pspec)
{
	BluetoothSettingsRow *self = BLUETOOTH_SETTINGS_ROW (object);
	BluetoothSettingsRowPrivate *priv = BLUETOOTH_SETTINGS_ROW_GET_PRIVATE (self);

	switch (property_id) {
	case PROP_PROXY:
		g_clear_object (&priv->proxy);
		priv->proxy = g_value_dup_object (value);
		break;
	case PROP_PAIRED:
		priv->paired = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_TRUSTED:
		priv->trusted = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_TYPE:
		priv->type = g_value_get_flags (value);
		if (priv->name == NULL) {
			gtk_label_set_text (GTK_LABEL (priv->label),
					    bluetooth_type_to_string (priv->type));
		}
		break;
	case PROP_CONNECTED:
		priv->connected = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_NAME:
		g_free (priv->name);
		priv->name = g_value_dup_string (value);
		if (priv->name != NULL)
			gtk_label_set_text (GTK_LABEL (priv->label), priv->name);
		break;
	case PROP_ADDRESS:
		g_free (priv->bdaddr);
		priv->bdaddr = g_value_dup_string (value);
		break;
	case PROP_PAIRING:
		priv->pairing = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_LEGACY_PAIRING:
		priv->legacy_pairing = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
bluetooth_settings_row_class_init (BluetoothSettingsRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	g_type_class_add_private (klass, sizeof (BluetoothSettingsRowPrivate));

	object_class->finalize = bluetooth_settings_row_finalize;
	object_class->get_property = bluetooth_settings_row_get_property;
	object_class->set_property = bluetooth_settings_row_set_property;

	g_object_class_install_property (object_class, PROP_PROXY,
					 g_param_spec_object ("proxy", NULL,
							      "The D-Bus object path of the device",
							      G_TYPE_DBUS_PROXY, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_PAIRED,
					 g_param_spec_boolean ("paired", NULL,
							      "Paired",
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TRUSTED,
					 g_param_spec_boolean ("trusted", NULL,
							      "Trusted",
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_flags ("type", NULL,
							      "Type",
							      BLUETOOTH_TYPE_TYPE, BLUETOOTH_TYPE_ANY, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_CONNECTED,
					 g_param_spec_boolean ("connected", NULL,
							      "Connected",
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL,
							      "Name",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ADDRESS,
					 g_param_spec_string ("address", NULL,
							      "Address",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_PAIRING,
					 g_param_spec_boolean ("pairing", NULL,
							      "Pairing",
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_LEGACY_PAIRING,
					 g_param_spec_boolean ("legacy-pairing", NULL,
							      "Legacy pairing",
							      FALSE, G_PARAM_READWRITE));
}

/**
 * bluetooth_settings_row_new:
 *
 * Returns a new #BluetoothSettingsRow widget.
 *
 * Return value: A #BluetoothSettingsRow widget
 **/
GtkWidget *
bluetooth_settings_row_new (void)
{
	return g_object_new (BLUETOOTH_TYPE_SETTINGS_ROW, NULL);
}
