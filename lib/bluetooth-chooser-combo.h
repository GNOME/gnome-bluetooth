/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * (C) Copyright 2007 Bastien Nocera <hadess@hadess.net>
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

#define BLUETOOTH_TYPE_CHOOSER_COMBO     (bluetooth_chooser_combo_get_type ())
#define BLUETOOTH_CHOOSER_COMBO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), BLUETOOTH_TYPE_CHOOSER_COMBO, BluetoothChooserCombo))
#define BLUETOOTH_IS_CHOOSER_COMBO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BLUETOOTH_TYPE_CHOOSER_COMBO))
#define BLUETOOTH_IS_CHOOSER_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
									 BLUETOOTH_TYPE_CHOOSER_COMBO))
#define BLUETOOTH_GET_CHOOSER_COMBO_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
				BLUETOOTH_TYPE_CHOOSER_COMBO, BluetoothChooserComboClass))

/**
* BLUETOOTH_CHOOSER_COMBO_FIRST_DEVICE:
*
* A convenience value used to select the first device regardless of its address.
**/
#define BLUETOOTH_CHOOSER_COMBO_FIRST_DEVICE "00:00:00:00:00:00"

typedef struct _BluetoothChooserComboPrivate BluetoothChooserComboPrivate;
typedef struct _BluetoothChooserComboClass BluetoothChooserComboClass;

/**
 * BluetoothChooserCombo:
 *
 * The <structname>BluetoothChooserCombo</structname> struct contains
 * only private fields and should not be directly accessed.
 */
typedef struct _BluetoothChooserCombo {
	GtkBox            parent;
	BluetoothChooserComboPrivate *priv;
} BluetoothChooserCombo;

struct _BluetoothChooserComboClass {
  GtkBoxClass parent_class;

  void (*chooser_created) (BluetoothChooserCombo *self, GtkWidget *chooser);
};

GType		bluetooth_chooser_combo_get_type	(void);

GtkWidget *	bluetooth_chooser_combo_new		(void);
