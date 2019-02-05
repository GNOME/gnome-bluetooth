/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_HDY_COLUMN (bluetooth_hdy_column_get_type())

G_DECLARE_FINAL_TYPE (BluetoothHdyColumn, bluetooth_hdy_column, BLUETOOTH, HDY_COLUMN, GtkBin)

BluetoothHdyColumn *bluetooth_hdy_column_new (void);
gint bluetooth_hdy_column_get_maximum_width (BluetoothHdyColumn *self);
void bluetooth_hdy_column_set_maximum_width (BluetoothHdyColumn *self,
                                             gint                maximum_width);
gint bluetooth_hdy_column_get_linear_growth_width (BluetoothHdyColumn *self);
void bluetooth_hdy_column_set_linear_growth_width (BluetoothHdyColumn *self,
                                                   gint                linear_growth_width);

G_END_DECLS
