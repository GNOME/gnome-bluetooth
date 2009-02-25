/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#include <dbus/dbus-glib.h>

#include "bluetooth-instance.h"
#include "bluetooth-instance-glue.h"

G_DEFINE_TYPE(BluetoothInstance, bluetooth_instance, G_TYPE_OBJECT)

#define BLUETOOTH_INSTANCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_INSTANCE, BluetoothInstancePrivate))

typedef struct _BluetoothInstancePrivate BluetoothInstancePrivate;

struct _BluetoothInstancePrivate {
	GtkWindow *window;
	DBusGConnection *connection;
};

void bluetooth_instance_set_window(BluetoothInstance *self, GtkWindow *window)
{
	BluetoothInstancePrivate *priv = BLUETOOTH_INSTANCE_GET_PRIVATE(self);

	priv->window = window;
}

gboolean bluetooth_instance_present(BluetoothInstance *self, GError **error)
{
	BluetoothInstancePrivate *priv = BLUETOOTH_INSTANCE_GET_PRIVATE(self);

	if (priv->window)
		gtk_window_present(priv->window);

	return TRUE;
}

static void bluetooth_instance_init(BluetoothInstance *self)
{
}

static void bluetooth_instance_finalize(GObject *self)
{
	BluetoothInstancePrivate *priv = BLUETOOTH_INSTANCE_GET_PRIVATE(self);

	if (priv->connection)
		dbus_g_connection_unref(priv->connection);
}

static void bluetooth_instance_class_init(BluetoothInstanceClass *klass)
{
	g_type_class_add_private(klass, sizeof(BluetoothInstancePrivate));

	G_OBJECT_CLASS(klass)->finalize = bluetooth_instance_finalize;

	dbus_g_object_type_install_info(BLUETOOTH_TYPE_INSTANCE,
				&dbus_glib_bluetooth_instance_object_info);
}

BluetoothInstance *bluetooth_instance_new(const gchar *name)
{
	BluetoothInstance *self;
	BluetoothInstancePrivate *priv;
	DBusGConnection *conn;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar *path, *service;
	guint result;

	service = g_strdup_printf("org.bluez.%s", name);
	if (service == NULL)
		return NULL;

	conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (error != NULL) {
		g_printerr("Can't get session bus: %s\n", error->message);
		g_error_free(error);
		g_free(service);
		return NULL;
	}

	proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE_DBUS,
					DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	if (dbus_g_proxy_call(proxy, "RequestName", NULL,
			G_TYPE_STRING, service, G_TYPE_UINT, 0, G_TYPE_INVALID,
			G_TYPE_UINT, &result, G_TYPE_INVALID) == FALSE) {
		g_printerr("Can't get unique name on session bus\n");
		g_object_unref(proxy);
		dbus_g_connection_unref(conn);
		g_free(service);
		return NULL;
	}

	g_object_unref(proxy);

	path = g_strdup_printf("/org/bluez/%s", name);

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		proxy = dbus_g_proxy_new_for_name(conn, service, path,
							"org.bluez.Instance");

		dbus_g_proxy_call_no_reply(proxy, "Present",
					G_TYPE_INVALID, G_TYPE_INVALID);

		g_object_unref(G_OBJECT(proxy));
		dbus_g_connection_unref(conn);
		g_free(service);
		g_free(path);
		return NULL;
	}

	self = g_object_new(BLUETOOTH_TYPE_INSTANCE, NULL);

	priv = BLUETOOTH_INSTANCE_GET_PRIVATE(self);
	priv->connection = conn;

	dbus_g_connection_register_g_object(conn, path, G_OBJECT(self));

	g_free(service);
	g_free(path);

	return self;
}
