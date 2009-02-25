/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
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

#ifndef __BLUETOOTH_DEVICE_SELECTION_H
#define __BLUETOOTH_DEVICE_SELECTION_H

#include <gtk/gtk.h>
#include <bluetooth-enums.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_DEVICE_SELECTION (bluetooth_device_selection_get_type())
#define BLUETOOTH_DEVICE_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
				BLUETOOTH_TYPE_DEVICE_SELECTION, BluetoothDeviceSelection))
#define BLUETOOTH_DEVICE_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
				BLUETOOTH_TYPE_DEVICE_SELECTION, BluetoothDeviceSelectionClass))
#define BLUETOOTH_IS_DEVICE_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						BLUETOOTH_TYPE_DEVICE_SELECTION))
#define BLUETOOTH_IS_DEVICE_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						BLUETOOTH_TYPE_DEVICE_SELECTION))
#define BLUETOOTH_GET_DEVICE_SELECTION_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
				BLUETOOTH_TYPE_DEVICE_SELECTION, BluetoothDeviceSelectionClass))

typedef struct _BluetoothDeviceSelection BluetoothDeviceSelection;
typedef struct _BluetoothDeviceSelectionClass BluetoothDeviceSelectionClass;

struct _BluetoothDeviceSelection {
	GtkVBox parent;
};

struct _BluetoothDeviceSelectionClass {
	GtkVBoxClass parent_class;

	void (*selected_device_changed) (BluetoothDeviceSelection *sel, gchar *device);
};

GType bluetooth_device_selection_get_type (void);

GtkWidget *bluetooth_device_selection_new (const gchar *title);

gchar *bluetooth_device_selection_get_selected_device (BluetoothDeviceSelection *sel);
gchar *bluetooth_device_selection_get_selected_device_name (BluetoothDeviceSelection *self);

void bluetooth_device_selection_start_discovery (BluetoothDeviceSelection *sel);

G_END_DECLS

#endif /* __BLUETOOTH_DEVICE_SELECTION_H */
