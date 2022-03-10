/*
 * Copyright (C) 2021 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib-object.h>
#include <libupower-glib/upower.h>

#include "bluetooth-device.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

enum {
	PROP_PROXY = 1,
	PROP_ADDRESS,
	PROP_ALIAS,
	PROP_NAME,
	PROP_TYPE,
	PROP_ICON,
	PROP_PAIRED,
	PROP_TRUSTED,
	PROP_CONNECTED,
	PROP_LEGACYPAIRING,
	PROP_UUIDS,
	PROP_CONNECTABLE,
	PROP_BATTERY_TYPE,
	PROP_BATTERY_PERCENTAGE,
	PROP_BATTERY_LEVEL
};

struct _BluetoothDevice {
	GObject parent;

	GDBusProxy *proxy;
	char *address;
	char *alias;
	char *name;
	BluetoothType type;
	char *icon;
	gboolean paired;
	gboolean trusted;
	gboolean connected;
	gboolean legacy_pairing;
	char **uuids;
	gboolean connectable;
	BluetoothBatteryType battery_type;
	double battery_percentage;
	UpDeviceLevel battery_level;
};

static const char *connectable_uuids[] = {
	"HSP",
	"AudioSource",
	"AudioSink",
	"A/V_RemoteControlTarget",
	"A/V_RemoteControl",
	"Headset_-_AG",
	"Handsfree",
	"HandsfreeAudioGateway",
	"HumanInterfaceDeviceService",
	"Human Interface Device",
	"MIDI",
};

G_DEFINE_TYPE(BluetoothDevice, bluetooth_device, G_TYPE_OBJECT)

static void
update_connectable (BluetoothDevice *device)
{
	gboolean new_connectable = FALSE;

	if (device->uuids) {
		guint i;

		for (i = 0; i < G_N_ELEMENTS (connectable_uuids); i++) {
			if (g_strv_contains ((const char * const*) device->uuids, connectable_uuids[i])) {
				new_connectable = TRUE;
				break;
			}
		}
	}

	if (new_connectable != device->connectable) {
		device->connectable = new_connectable;
		g_object_notify (G_OBJECT (device), "connectable");
	}
}

static void
bluetooth_device_get_property (GObject        *object,
			       guint           property_id,
			       GValue         *value,
			       GParamSpec     *pspec)
{
	BluetoothDevice *device = BLUETOOTH_DEVICE (object);

	switch (property_id) {
	case PROP_PROXY:
		g_value_set_object (value, device->proxy);
		break;
	case PROP_ADDRESS:
		g_value_set_string (value, device->address);
		break;
	case PROP_ALIAS:
		g_value_set_string (value, device->alias);
		break;
	case PROP_NAME:
		g_value_set_string (value, device->name);
		break;
	case PROP_TYPE:
		g_value_set_flags (value, device->type);
		break;
	case PROP_ICON:
		g_value_set_string (value, device->icon);
		break;
	case PROP_PAIRED:
		g_value_set_boolean (value, device->paired);
		break;
	case PROP_TRUSTED:
		g_value_set_boolean (value, device->trusted);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, device->connected);
		break;
	case PROP_LEGACYPAIRING:
		g_value_set_boolean (value, device->legacy_pairing);
		break;
	case PROP_UUIDS:
		g_value_set_boxed (value, device->uuids);
		break;
	case PROP_CONNECTABLE:
		g_value_set_boolean (value, device->connectable);
		break;
	case PROP_BATTERY_TYPE:
		g_value_set_enum (value, device->battery_type);
		break;
	case PROP_BATTERY_PERCENTAGE:
		g_value_set_double (value, device->battery_percentage);
		break;
	case PROP_BATTERY_LEVEL:
		g_value_set_uint (value, device->battery_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
bluetooth_device_set_property (GObject        *object,
			       guint           property_id,
			       const GValue   *value,
			       GParamSpec     *pspec)
{
	BluetoothDevice *device = BLUETOOTH_DEVICE (object);

	switch (property_id) {
	case PROP_PROXY:
		g_clear_object (&device->proxy);
		device->proxy = g_value_dup_object (value);
		break;
	case PROP_ADDRESS:
		g_clear_pointer (&device->address, g_free);
		device->address = g_value_dup_string (value);
		break;
	case PROP_ALIAS:
		g_clear_pointer (&device->alias, g_free);
		device->alias = g_value_dup_string (value);
		break;
	case PROP_NAME:
		g_clear_pointer (&device->name, g_free);
		device->name = g_value_dup_string (value);
		break;
	case PROP_TYPE:
		device->type = g_value_get_flags (value);
		break;
	case PROP_ICON:
		g_clear_pointer (&device->icon, g_free);
		device->icon = g_value_dup_string (value);
		break;
	case PROP_PAIRED:
		device->paired = g_value_get_boolean (value);
		break;
	case PROP_TRUSTED:
		device->trusted = g_value_get_boolean (value);
		break;
	case PROP_CONNECTED:
		device->connected = g_value_get_boolean (value);
		break;
	case PROP_LEGACYPAIRING:
		device->legacy_pairing = g_value_get_boolean (value);
		break;
	case PROP_UUIDS:
		g_clear_pointer (&device->uuids, g_strfreev);
		device->uuids = g_value_dup_boxed (value);
		update_connectable (device);
		break;
	case PROP_BATTERY_TYPE:
		device->battery_type = g_value_get_enum (value);
		break;
	case PROP_BATTERY_PERCENTAGE:
		device->battery_percentage = g_value_get_double (value);
		break;
	case PROP_BATTERY_LEVEL:
		device->battery_level = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void bluetooth_device_finalize(GObject *object)
{
	BluetoothDevice *device = BLUETOOTH_DEVICE (object);

	g_clear_object (&device->proxy);
	g_clear_pointer (&device->address, g_free);
	g_clear_pointer (&device->alias, g_free);
	g_clear_pointer (&device->name, g_free);
	g_clear_pointer (&device->icon, g_free);
	g_clear_pointer (&device->uuids, g_strfreev);

	G_OBJECT_CLASS(bluetooth_device_parent_class)->finalize (object);
}

static void bluetooth_device_class_init(BluetoothDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = bluetooth_device_finalize;
	object_class->get_property = bluetooth_device_get_property;
	object_class->set_property = bluetooth_device_set_property;

	g_object_class_install_property (object_class, PROP_PROXY,
					 g_param_spec_object ("proxy", NULL, "Proxy",
							      G_TYPE_DBUS_PROXY, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ADDRESS,
					 g_param_spec_string ("address", NULL, "Address",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ALIAS,
					 g_param_spec_string ("alias", NULL, "Alias",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, "Name",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_flags ("type", NULL, "Type",
							     BLUETOOTH_TYPE_TYPE, BLUETOOTH_TYPE_ANY, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ICON,
					 g_param_spec_string ("icon", NULL, "Icon",
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_PAIRED,
					 g_param_spec_boolean ("paired", NULL, "Paired",
							       FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TRUSTED,
					 g_param_spec_boolean ("trusted", NULL, "Trusted",
							       FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_CONNECTED,
					 g_param_spec_boolean ("connected", NULL, "Connected",
							       FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_LEGACYPAIRING,
					 g_param_spec_boolean ("legacy-pairing", NULL, "Legacy Pairing",
							       FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_UUIDS,
					 g_param_spec_boxed ("uuids", NULL, "UUIDs",
							     G_TYPE_STRV, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_CONNECTABLE,
					 g_param_spec_boolean ("connectable", NULL, "Connectable",
							       FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_BATTERY_TYPE,
					 g_param_spec_enum ("battery-type", NULL, "Battery Type",
							    BLUETOOTH_TYPE_BATTERY_TYPE, BLUETOOTH_BATTERY_TYPE_NONE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_BATTERY_PERCENTAGE,
					 g_param_spec_double ("battery-percentage", NULL, "Battery Percentage",
							    0.0, 100.f, 0.0, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_BATTERY_LEVEL,
					 g_param_spec_uint ("battery-level", NULL, "Battery Level",
							    UP_DEVICE_LEVEL_UNKNOWN, UP_DEVICE_LEVEL_LAST - 1, UP_DEVICE_LEVEL_UNKNOWN,
							    G_PARAM_READWRITE));
}

static void
bluetooth_device_init (BluetoothDevice *device)
{
}

const char *
bluetooth_device_get_object_path (BluetoothDevice *device)
{
	g_return_val_if_fail (BLUETOOTH_IS_DEVICE (device), NULL);

	if (device->proxy == NULL)
		return NULL;
	return g_dbus_proxy_get_object_path (device->proxy);
}

#define BOOL_STR(x) (x ? "True" : "False")

char *
bluetooth_device_to_string (BluetoothDevice *device)
{
	GString *str;

	g_return_val_if_fail (BLUETOOTH_IS_DEVICE (device), NULL);

	str = g_string_new (NULL);

	g_string_append_printf (str, "Device: %s (%s)\n", device->alias, device->address);
	g_string_append_printf (str, "\tD-Bus Path: %s\n", device->proxy ? g_dbus_proxy_get_object_path (device->proxy) : "(none)");
	g_string_append_printf (str, "\tType: %s Icon: %s\n", bluetooth_type_to_string (device->type), device->icon);
	g_string_append_printf (str, "\tPaired: %s Trusted: %s Connected: %s\n", BOOL_STR(device->paired), BOOL_STR(device->trusted), BOOL_STR(device->connected));
	if (device->battery_type == BLUETOOTH_BATTERY_TYPE_PERCENTAGE)
		g_string_append_printf (str, "\tBattery: %.02g%%\n", device->battery_percentage);
	else if (device->battery_type == BLUETOOTH_BATTERY_TYPE_COARSE)
		g_string_append_printf (str, "\tBattery: %s\n", up_device_level_to_string (device->battery_level));
	if (device->uuids != NULL) {
		guint i;
		g_string_append_printf (str, "\tUUIDs: ");
		for (i = 0; device->uuids[i] != NULL; i++)
			g_string_append_printf (str, "%s ", device->uuids[i]);
	}

	return g_string_free (str, FALSE);
}

void
bluetooth_device_dump (BluetoothDevice *device)
{
	g_autofree char *str = NULL;

	g_return_if_fail (BLUETOOTH_IS_DEVICE (device));

	str = bluetooth_device_to_string (device);
	g_print ("%s\n", str);
}
