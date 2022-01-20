/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2009-2021  Red Hat Inc.
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

#include <gio/gio.h>
#include <bluetooth-device.h>
#include <bluetooth-enums.h>

void bluetooth_client_setup_device (BluetoothClient          *client,
				    const char               *path,
				    gboolean                  pair,
				    GCancellable             *cancellable,
				    GAsyncReadyCallback       callback,
				    gpointer                  user_data);
gboolean bluetooth_client_setup_device_finish (BluetoothClient  *client,
					       GAsyncResult     *res,
					       char            **path,
					       GError          **error);

void bluetooth_client_cancel_setup_device (BluetoothClient     *client,
					   const char          *path,
					   GCancellable        *cancellable,
					   GAsyncReadyCallback  callback,
					   gpointer             user_data);
gboolean bluetooth_client_cancel_setup_device_finish (BluetoothClient *client,
						      GAsyncResult     *res,
						      char            **path,
						      GError          **error);

gboolean bluetooth_client_set_trusted(BluetoothClient *client,
					const char *device, gboolean trusted);

GDBusProxy *_bluetooth_client_get_default_adapter (BluetoothClient *client);
