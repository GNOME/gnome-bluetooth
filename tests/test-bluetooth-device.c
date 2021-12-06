/*
 * Copyright (C) 2021 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "bluetooth-device.h"

static void
test_device (void)
{
	BluetoothDevice *device;
	GDBusProxy *proxy;
	const char *uuids[] = {
		"OBEXFileTransfer",
		NULL
	};
	const char *new_uuids[] = {
		"AudioSource",
		NULL
	};

	proxy = g_object_new (G_TYPE_DBUS_PROXY, NULL);
	g_assert_nonnull (proxy);
	device = g_object_new (BLUETOOTH_TYPE_DEVICE,
			       "proxy", proxy,
			       "name", "Fake Name",
			       "alias", "Changed Fake Name",
			       "address", "00:11:22:33:44",
			       "type", BLUETOOTH_TYPE_KEYBOARD,
			       "icon", "phone",
			       "paired", TRUE,
			       "trusted", TRUE,
			       "connected", FALSE,
			       "legacy-pairing", FALSE,
			       "uuids", uuids,
			       NULL);
	g_object_set (G_OBJECT (device),
		      "uuids", new_uuids,
		      NULL);
	g_object_unref (device);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/bluetooth/device", test_device);

	return g_test_run ();
}
