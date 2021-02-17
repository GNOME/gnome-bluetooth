/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * (C) Copyright 2007, 2021 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE(BluetoothChooserCombo, bluetooth_chooser_combo, BLUETOOTH, CHOOSER_COMBO, GtkBox)
#define BLUETOOTH_TYPE_CHOOSER_COMBO (bluetooth_chooser_combo_get_type ())

/**
* BLUETOOTH_CHOOSER_COMBO_FIRST_DEVICE:
*
* A convenience value used to select the first device regardless of its address.
**/
#define BLUETOOTH_CHOOSER_COMBO_FIRST_DEVICE "00:00:00:00:00:00"

GtkWidget *	bluetooth_chooser_combo_new		(void);
