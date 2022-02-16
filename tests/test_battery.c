/*
 * Copyright (C) 2022 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <bluetooth-client.h>
#include <bluetooth-device.h>
#include <libupower-glib/upower.h>

int main (int argc, char **argv)
{
	BluetoothClient *client;
	GListStore *list_store;

	client = bluetooth_client_new ();
	list_store = bluetooth_client_get_devices (client);

	/* Wait for bluez */
	while (g_list_model_get_n_items (G_LIST_MODEL (list_store)) != 2)
		g_main_context_iteration (NULL, TRUE);
	g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list_store)), ==, 2);

	BluetoothDevice *device;
	BluetoothBatteryType battery_type;
	double battery_percentage;
	device = g_list_model_get_item (G_LIST_MODEL (list_store), 0);
	/* Wait for upower */
	g_object_get (G_OBJECT (device), "battery-type", &battery_type, NULL);
	while (battery_type != BLUETOOTH_BATTERY_TYPE_PERCENTAGE) {
		g_main_context_iteration (NULL, TRUE);
		g_object_get (G_OBJECT (device), "battery-type", &battery_type, NULL);
	}
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

	return 0;
}
