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

#include <adwaita.h>
#include <gtk/gtk.h>
#include "bluetooth-device.h"

#define BLUETOOTH_TYPE_SETTINGS_ROW (bluetooth_settings_row_get_type())
G_DECLARE_FINAL_TYPE (BluetoothSettingsRow, bluetooth_settings_row, BLUETOOTH, SETTINGS_ROW, AdwActionRow)

GtkWidget *bluetooth_settings_row_new_from_device (BluetoothDevice *device);
