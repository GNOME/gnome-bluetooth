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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <bluetooth-enums.h>

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

/**
 * BluetoothClient:
 *
 * The <structname>BluetoothClient</structname> struct contains
 * only private fields and should not be directly accessed.
 */
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

GtkTreeModel *bluetooth_client_get_adapter_model(BluetoothClient *client);
GtkTreeModel *bluetooth_client_get_device_model(BluetoothClient *client);

void bluetooth_client_connect_service (BluetoothClient     *client,
				       const char          *path,
				       gboolean             connect,
				       GCancellable        *cancellable,
				       GAsyncReadyCallback  callback,
				       gpointer             user_data);

gboolean bluetooth_client_connect_service_finish (BluetoothClient *client,
						  GAsyncResult    *res,
						  GError         **error);
