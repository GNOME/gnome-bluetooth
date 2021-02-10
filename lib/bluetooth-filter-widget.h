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

#pragma once

#include <gtk/gtk.h>
#include <bluetooth-enums.h>
#include <bluetooth-chooser.h>

G_DECLARE_FINAL_TYPE(BluetoothFilterWidget, bluetooth_filter_widget, BLUETOOTH, FILTER_WIDGET, GtkBox)
#define BLUETOOTH_TYPE_FILTER_WIDGET (bluetooth_filter_widget_get_type())

GtkWidget *bluetooth_filter_widget_new (void);

void bluetooth_filter_widget_set_title (BluetoothFilterWidget *filter, gchar *title);

void bluetooth_filter_widget_bind_filter (BluetoothFilterWidget *filter, BluetoothChooser *chooser);
