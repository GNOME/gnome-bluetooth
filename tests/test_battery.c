/*
 * Copyright (C) 2022 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <bluetooth-client.h>
#include <bluetooth-device.h>
#include <libupower-glib/upower.h>

static void
remove_upower_device (GDBusConnection *bus,
		      const char      *object_path)
{
	g_autoptr(GVariant) ret = NULL;
	g_autoptr(GError) error = NULL;

	ret = g_dbus_connection_call_sync (bus,
					   "org.freedesktop.UPower",
					   "/org/freedesktop/UPower",
					   "org.freedesktop.DBus.Mock",
					   "RemoveDevice",
					   g_variant_new ("(o)", object_path),
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE,
					   -1,
					   NULL,
					   &error);
	g_assert_no_error (error);
	g_assert_nonnull (ret);
}

#define DEFAULT_MESON_TIMEOUT 30
#define MAX_TEST_TIME         DEFAULT_MESON_TIMEOUT / 10

static gboolean
timeout_cb (gpointer user_data)
{
	char *str = user_data;
	g_assert_cmpstr (NULL, ==, str);
	exit (1);
	return TRUE;
}

int main (int argc, char **argv)
{
	BluetoothClient *client;
	GListStore *list_store;
	guint id;

	client = bluetooth_client_new ();
	list_store = bluetooth_client_get_devices (client);

	/* Wait for bluez */
	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (g_list_model_get_n_items (G_LIST_MODEL (list_store)) != 2)
		g_main_context_iteration (NULL, TRUE);
	g_source_remove (id);
	g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list_store)), ==, 2);

	BluetoothDevice *device;
	BluetoothBatteryType battery_type;
	double battery_percentage;
	device = g_list_model_get_item (G_LIST_MODEL (list_store), 0);
	/* Wait for upower */
	g_object_get (G_OBJECT (device), "battery-type", &battery_type, NULL);
	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (battery_type != BLUETOOTH_BATTERY_TYPE_PERCENTAGE) {
		g_main_context_iteration (NULL, TRUE);
		g_object_get (G_OBJECT (device), "battery-type", &battery_type, NULL);
	}
	g_source_remove (id);
	g_object_get (G_OBJECT (device),
		      "battery-type", &battery_type,
		      "battery-percentage", &battery_percentage,
		      NULL);
	g_assert_cmpuint (battery_type, ==, BLUETOOTH_BATTERY_TYPE_PERCENTAGE);
	g_assert_cmpfloat (battery_percentage, ==, 66.0);
	g_object_unref (G_OBJECT (device));

	UpDeviceLevel battery_level;
	device = g_list_model_get_item (G_LIST_MODEL (list_store), 1);
	g_object_get (G_OBJECT (device),
		      "battery-type", &battery_type,
		      "battery-percentage", &battery_percentage,
		      "battery-level", &battery_level,
		      NULL);
	g_assert_cmpuint (battery_type, ==, BLUETOOTH_BATTERY_TYPE_COARSE);
	g_assert_cmpfloat (battery_percentage, ==, 55.0);
	g_assert_cmpuint (battery_level, ==, UP_DEVICE_LEVEL_NORMAL);
	g_object_unref (G_OBJECT (device));

	g_clear_object (&client);
	g_clear_object (&list_store);

	/* Start use-after-free test */
	BluetoothClient *client2;
	GListStore *list_store2;

	client2 = bluetooth_client_new ();
	list_store2 = bluetooth_client_get_devices (client2);

	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (g_list_model_get_n_items (G_LIST_MODEL (list_store2)) != 2)
		g_main_context_iteration (NULL, TRUE);
	g_source_remove (id);
	g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list_store2)), ==, 2);

	BluetoothDevice *device2;
	device2 = g_list_model_get_item (G_LIST_MODEL (list_store2), 0);
	g_object_get (G_OBJECT (device2), "battery-type", &battery_type, NULL);
	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (battery_type != BLUETOOTH_BATTERY_TYPE_PERCENTAGE) {
		g_main_context_iteration (NULL, TRUE);
		g_object_get (G_OBJECT (device2), "battery-type", &battery_type, NULL);
	}
	g_source_remove (id);

	GDBusConnection *bus;
	bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
	remove_upower_device (bus, "/org/freedesktop/UPower/devices/mouse_dev_11_22_33_44_55_66");
	g_object_get (G_OBJECT (device2), "battery-type", &battery_type, NULL);
	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (battery_type != BLUETOOTH_BATTERY_TYPE_NONE) {
		g_main_context_iteration (NULL, TRUE);
		g_object_get (G_OBJECT (device2), "battery-type", &battery_type, NULL);
	}
	g_source_remove (id);
	g_object_unref (G_OBJECT (device2));

	g_dbus_connection_call_sync (bus,
				     "org.bluez",
				     "/org/bluez/hci0",
				     "org.bluez.Adapter1",
				     "RemoveDevice",
				     g_variant_new ("(o)", "/org/bluez/hci0/dev_11_22_33_44_55_67"),
				     NULL,
				     G_DBUS_CALL_FLAGS_NONE,
				     -1,
				     NULL,
				     NULL);
	remove_upower_device (bus, "/org/freedesktop/UPower/devices/mouse_dev_11_22_33_44_55_67");
	id = g_timeout_add (MAX_TEST_TIME * 1000, timeout_cb, (gpointer) G_STRLOC);
	while (g_list_model_get_n_items (G_LIST_MODEL (list_store2)) != 1)
		g_main_context_iteration (NULL, TRUE);
	g_source_remove (id);
	g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list_store2)), ==, 1);

	g_object_unref (G_OBJECT (list_store2));
	g_object_unref (G_OBJECT (client2));

	return 0;
}
