/*
 *
 *  Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>

#define BLUETOOTH_TYPE_SETTINGS_WIDGET (bluetooth_settings_widget_get_type())
G_DECLARE_FINAL_TYPE (BluetoothSettingsWidget, bluetooth_settings_widget, BLUETOOTH, SETTINGS_WIDGET, GtkBox)

GtkWidget *bluetooth_settings_widget_new (void);

gboolean bluetooth_settings_widget_get_default_adapter_powered (BluetoothSettingsWidget *widget);
void bluetooth_settings_widget_set_default_adapter_powered (BluetoothSettingsWidget *widget,
                                                            gboolean                 powered);
