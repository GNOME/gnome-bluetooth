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

#ifndef __BLUETOOTH_CLIENT_H
#define __BLUETOOTH_CLIENT_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_CLIENT (bluetooth_client_get_type())
#define BLUETOOTH_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
					BLUETOOTH_TYPE_CLIENT, BluetoothClient))
#define BLUETOOTH_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					BLUETOOTH_TYPE_CLIENT, BluetoothClientClass))
#define BLUETOOTH_IS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
							BLUETOOTH_TYPE_CLIENT))
#define BLUETOOTH_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
							BLUETOOTH_TYPE_CLIENT))
#define BLUETOOTH_GET_CLIENT_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					BLUETOOTH_TYPE_CLIENT, BluetoothClientClass))

typedef struct _BluetoothClient BluetoothClient;
typedef struct _BluetoothClientClass BluetoothClientClass;

struct _BluetoothClient {
	GObject parent;
};

struct _BluetoothClientClass {
	GObjectClass parent_class;
};

GType bluetooth_client_get_type(void);

BluetoothClient *bluetooth_client_new(void);

GtkTreeModel *bluetooth_client_get_model(BluetoothClient *client);
GtkTreeModel *bluetooth_client_get_filter_model(BluetoothClient *client,
				GtkTreeModelFilterVisibleFunc func,
				gpointer data, GDestroyNotify destroy);
GtkTreeModel *bluetooth_client_get_adapter_model(BluetoothClient *client);
GtkTreeModel *bluetooth_client_get_device_model(BluetoothClient *client,
							DBusGProxy *adapter);
GtkTreeModel *bluetooth_client_get_device_filter_model(BluetoothClient *client,
		DBusGProxy *adapter, GtkTreeModelFilterVisibleFunc func,
				gpointer data, GDestroyNotify destroy);

DBusGProxy *bluetooth_client_get_default_adapter(BluetoothClient *client);

gboolean bluetooth_client_start_discovery(BluetoothClient *client);
gboolean bluetooth_client_stop_discovery(BluetoothClient *client);

typedef void (*BluetoothClientCreateDeviceFunc) (const char *path, gpointer data);

gboolean bluetooth_client_create_device(BluetoothClient *client,
			const char *address, const char *agent,
			BluetoothClientCreateDeviceFunc func, gpointer data);

gboolean bluetooth_client_set_trusted(BluetoothClient *client,
					const char *device, gboolean trusted);

typedef void (*BluetoothClientConnectFunc) (gpointer data);

gboolean bluetooth_client_connect_input(BluetoothClient *client,
				const char *device,
				BluetoothClientConnectFunc, gpointer data);

enum {
	BLUETOOTH_COLUMN_PROXY,
	BLUETOOTH_COLUMN_ADDRESS,
	BLUETOOTH_COLUMN_ALIAS,
	BLUETOOTH_COLUMN_NAME,
	BLUETOOTH_COLUMN_TYPE,
	BLUETOOTH_COLUMN_ICON,
	BLUETOOTH_COLUMN_RSSI,
	BLUETOOTH_COLUMN_DEFAULT,
	BLUETOOTH_COLUMN_PAIRED,
	BLUETOOTH_COLUMN_TRUSTED,
	BLUETOOTH_COLUMN_CONNECTED,
	BLUETOOTH_COLUMN_DISCOVERING,
	_BLUETOOTH_NUM_COLUMNS
};

enum {
	BLUETOOTH_TYPE_ANY		= 1 << 0,
	BLUETOOTH_TYPE_PHONE		= 1 << 1,
	BLUETOOTH_TYPE_MODEM		= 1 << 2,
	BLUETOOTH_TYPE_COMPUTER		= 1 << 3,
	BLUETOOTH_TYPE_NETWORK		= 1 << 4,
	BLUETOOTH_TYPE_HEADSET		= 1 << 5,
	BLUETOOTH_TYPE_HEADPHONE	= 1 << 6,
	BLUETOOTH_TYPE_KEYBOARD		= 1 << 7,
	BLUETOOTH_TYPE_MOUSE		= 1 << 8,
	BLUETOOTH_TYPE_CAMERA		= 1 << 9,
	BLUETOOTH_TYPE_PRINTER		= 1 << 10,
	BLUETOOTH_TYPE_JOYPAD		= 1 << 11,
	BLUETOOTH_TYPE_TABLET		= 1 << 12,
};

#define _BLUETOOTH_TYPE_NUM_TYPES 12

#define BLUETOOTH_TYPE_INPUT (BLUETOOTH_TYPE_KEYBOARD | BLUETOOTH_TYPE_MOUSE)

const gchar *bluetooth_type_to_string(guint type);

G_END_DECLS

#endif /* __BLUETOOTH_CLIENT_H */
