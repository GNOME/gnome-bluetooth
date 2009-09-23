#ifndef __BLUETOOTH_CHOOSER_PRIVATE_H
#define __BLUETOOTH_CHOOSER_PRIVATE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <bluetooth-enums.h>

G_BEGIN_DECLS

GtkTreeModel *bluetooth_chooser_get_model (BluetoothChooser *self);
GtkTreeViewColumn *bluetooth_chooser_get_device_column (BluetoothChooser *self);

G_END_DECLS

#endif /* __BLUETOOTH_CHOOSER_PRIVATE_H */
