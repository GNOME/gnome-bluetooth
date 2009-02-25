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

#ifndef __BLUETOOTH_INSTANCE_H
#define __BLUETOOTH_INSTANCE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_INSTANCE (bluetooth_instance_get_type())
#define BLUETOOTH_INSTANCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
				BLUETOOTH_TYPE_INSTANCE, BluetoothInstance))
#define BLUETOOTH_INSTANCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
				BLUETOOTH_TYPE_INSTANCE, BluetoothInstanceClass))
#define BLUETOOTH_IS_INSTANCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						BLUETOOTH_TYPE_INSTANCE))
#define BLUETOOTH_IS_INSTANCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						BLUETOOTH_TYPE_INSTANCE))
#define BLUETOOTH_GET_INSTANCE_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
				BLUETOOTH_TYPE_INSTANCE, BluetoothInstanceClass))

typedef struct _BluetoothInstance BluetoothInstance;
typedef struct _BluetoothInstanceClass BluetoothInstanceClass;

struct _BluetoothInstance {
	GObject parent;
};

struct _BluetoothInstanceClass {
	GObjectClass parent_class;
};

GType bluetooth_instance_get_type(void);
BluetoothInstance *bluetooth_instance_new(const gchar *name);
void bluetooth_instance_set_window(BluetoothInstance *self, GtkWindow *window);
gboolean bluetooth_instance_present(BluetoothInstance *self, GError **error);

G_END_DECLS

#endif /* __BLUETOOTH_INSTANCE_H */
