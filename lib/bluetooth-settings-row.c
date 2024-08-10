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

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>

#include "bluetooth-settings-row.h"
#include "bluetooth-enums.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

struct _BluetoothSettingsRow {
	AdwActionRow parent_instance;

	/* Widget */
	GtkWidget *status;
	GtkWidget *spinner;

	/* Properties */
	GDBusProxy *proxy;
	BluetoothDevice *device;
	gboolean paired;
	gboolean trusted;
	BluetoothType type;
	gboolean connected;
	char *name;
	char *alias;
	char *bdaddr;
	gboolean legacy_pairing;
	gint64 time_created;

	gboolean pairing;
};

enum {
	PROP_0,
	PROP_PROXY,
	PROP_DEVICE,
	PROP_PAIRED,
	PROP_TRUSTED,
	PROP_TYPE,
	PROP_CONNECTED,
	PROP_NAME,
	PROP_ALIAS,
	PROP_ADDRESS,
	PROP_PAIRING,
	PROP_LEGACY_PAIRING,
	PROP_TIME_CREATED
};

G_DEFINE_TYPE(BluetoothSettingsRow, bluetooth_settings_row, ADW_TYPE_ACTION_ROW)

static void
label_might_change (BluetoothSettingsRow *self)
{
	if (!self->paired && !self->trusted)
		gtk_label_set_text (GTK_LABEL (self->status), _("Not Set Up"));
	else if (self->connected)
		gtk_label_set_text (GTK_LABEL (self->status), _("Connected"));
	else
		gtk_label_set_text (GTK_LABEL (self->status), _("Disconnected"));

	if (self->pairing) {
		gtk_widget_set_visible (self->status, FALSE);

		gtk_widget_set_visible (self->spinner, TRUE);
	} else {
		gtk_widget_set_visible (self->spinner, FALSE);

		gtk_widget_set_visible (self->status, TRUE);
	}
}

static void
bluetooth_settings_row_init (BluetoothSettingsRow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	self->time_created = g_get_monotonic_time();

	label_might_change (self);
}

static void
bluetooth_settings_row_finalize (GObject *object)
{
	BluetoothSettingsRow *self = BLUETOOTH_SETTINGS_ROW (object);

	g_clear_object (&self->proxy);
	g_clear_object (&self->device);
	g_clear_pointer (&self->name, g_free);
	g_clear_pointer (&self->alias, g_free);
	g_clear_pointer (&self->bdaddr, g_free);

	G_OBJECT_CLASS(bluetooth_settings_row_parent_class)->finalize(object);
}

static void
bluetooth_settings_row_get_property (GObject        *object,
				     guint           property_id,
				     GValue         *value,
				     GParamSpec     *pspec)
{
	BluetoothSettingsRow *self = BLUETOOTH_SETTINGS_ROW (object);

	switch (property_id) {
	case PROP_PROXY:
		g_value_set_object (value, self->proxy);
		break;
	case PROP_DEVICE:
		g_value_set_object (value, self->device);
		break;
	case PROP_PAIRED:
		g_value_set_boolean (value, self->paired);
		break;
	case PROP_TRUSTED:
		g_value_set_boolean (value, self->trusted);
		break;
	case PROP_TYPE:
		g_value_set_flags (value, self->type);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, self->connected);
		break;
	case PROP_NAME:
		g_value_set_string (value, self->name);
		break;
	case PROP_ALIAS:
		g_value_set_string (value, self->alias);
		break;
	case PROP_ADDRESS:
		g_value_set_string (value, self->bdaddr);
		break;
	case PROP_PAIRING:
		g_value_set_boolean (value, self->pairing);
		break;
	case PROP_LEGACY_PAIRING:
		g_value_set_boolean (value, self->legacy_pairing);
		break;
	case PROP_TIME_CREATED:
		g_value_set_int64 (value, self->time_created);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
update_row (BluetoothSettingsRow *self)
{
	if (self->name == NULL) {
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
				    bluetooth_type_to_string (self->type));
		gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
	} else {
		g_autofree char *escaped = NULL;
		if (self->alias != NULL)
			escaped = g_markup_escape_text (self->alias, -1);
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), escaped);
		gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	}
}

static void
bluetooth_settings_row_set_property (GObject        *object,
				     guint           property_id,
				     const GValue   *value,
				     GParamSpec     *pspec)
{
	BluetoothSettingsRow *self = BLUETOOTH_SETTINGS_ROW (object);

	switch (property_id) {
	case PROP_PROXY:
		g_clear_object (&self->proxy);
		self->proxy = g_value_dup_object (value);
		break;
	case PROP_DEVICE:
		g_assert (!self->device);
		self->device = g_value_dup_object (value);
		break;
	case PROP_PAIRED:
		self->paired = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_TRUSTED:
		self->trusted = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_TYPE:
		self->type = g_value_get_flags (value);
		update_row (self);
		break;
	case PROP_CONNECTED:
		self->connected = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_NAME:
		g_free (self->name);
		self->name = g_value_dup_string (value);
		update_row (self);
		break;
	case PROP_ALIAS:
		g_free (self->alias);
		self->alias = g_value_dup_string (value);
		update_row (self);
		break;
	case PROP_ADDRESS:
		g_free (self->bdaddr);
		self->bdaddr = g_value_dup_string (value);
		break;
	case PROP_PAIRING:
		self->pairing = g_value_get_boolean (value);
		label_might_change (self);
		break;
	case PROP_LEGACY_PAIRING:
		self->legacy_pairing = g_value_get_boolean (value);
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
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	object_class->finalize = bluetooth_settings_row_finalize;
	object_class->get_property = bluetooth_settings_row_get_property;
	object_class->set_property = bluetooth_settings_row_set_property;

	g_object_class_install_property (object_class, PROP_PROXY,
					 g_param_spec_object ("proxy", NULL,
							      "The D-Bus proxy object of the device",
							      G_TYPE_DBUS_PROXY, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_DEVICE,
					 g_param_spec_object ("device", NULL,
							      "a BluetoothDevice object",
							      BLUETOOTH_TYPE_DEVICE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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
	g_object_class_install_property (object_class, PROP_ALIAS,
					 g_param_spec_string ("alias", NULL,
							      "Alias",
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
	g_object_class_install_property (object_class, PROP_TIME_CREATED,
					 g_param_spec_int64 ("time-created", NULL,
							    "Time Created",
							    G_MININT64, G_MAXINT64, 0, G_PARAM_READABLE));

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/bluetooth/bluetooth-settings-row.ui");
	gtk_widget_class_bind_template_child (widget_class, BluetoothSettingsRow, spinner);
	gtk_widget_class_bind_template_child (widget_class, BluetoothSettingsRow, status);
}

static gboolean
name_to_visible (GBinding     *binding,
		 const GValue *from_value,
		 GValue       *to_value,
		 gpointer      user_data)
{
	const char *name;

	name = g_value_get_string (from_value);
	g_value_set_boolean (to_value, name != NULL);
	return TRUE;
}

/**
 * bluetooth_settings_row_new_from_device:
 *
 * Returns a new #BluetoothSettingsRow widget populated from
 * the #BluetoothDevice passed.
 *
 * Return value: A #BluetoothSettingsRow widget
 **/
GtkWidget *
bluetooth_settings_row_new_from_device (BluetoothDevice *device)
{
	g_autoptr(GDBusProxy) proxy = NULL;
	GtkWidget *row;
	const char *props[] = {
		"name",
		"alias",
		"paired",
		"trusted",
		"connected",
		"address",
		"type",
		"legacy-pairing",
		"proxy"
	};
	guint i;

	g_return_val_if_fail (BLUETOOTH_IS_DEVICE (device), NULL);

	g_object_get (G_OBJECT (device), "proxy", &proxy, NULL);
	row = g_object_new (BLUETOOTH_TYPE_SETTINGS_ROW,
			    "device", device,
			    NULL);

	for (i = 0; i < G_N_ELEMENTS (props); i++) {
		g_object_bind_property (G_OBJECT (device), props[i],
					G_OBJECT (row), props[i],
					G_BINDING_SYNC_CREATE);
	}
	g_object_bind_property_full (G_OBJECT (device), "name",
				     G_OBJECT (row), "visible",
				     G_BINDING_SYNC_CREATE,
				     name_to_visible, NULL,
				     NULL, NULL);

	return row;
}
