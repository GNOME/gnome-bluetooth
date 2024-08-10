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


#define BLUETOOTH_TYPE_PAIRING_DIALOG (bluetooth_pairing_dialog_get_type())
G_DECLARE_DERIVABLE_TYPE (BluetoothPairingDialog, bluetooth_pairing_dialog, BLUETOOTH, PAIRING_DIALOG, AdwDialog)

struct _BluetoothPairingDialogClass
{
  AdwDialogClass parent_class;
};

typedef enum {
	BLUETOOTH_PAIRING_MODE_PIN_QUERY,
	BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION,
	BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL,
	BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD,
	BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE,
	BLUETOOTH_PAIRING_MODE_PIN_MATCH,
	BLUETOOTH_PAIRING_MODE_YES_NO,
	BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH
} BluetoothPairingMode;

GType bluetooth_pairing_dialog_get_type (void);

GtkWidget *bluetooth_pairing_dialog_new (void);

void bluetooth_pairing_dialog_set_mode (BluetoothPairingDialog *self,
					BluetoothPairingMode    mode,
					const char             *pin,
					const char             *name);
BluetoothPairingMode bluetooth_pairing_dialog_get_mode (BluetoothPairingDialog *self);

void bluetooth_pairing_dialog_set_pin_entered (BluetoothPairingDialog *self,
					       guint                   entered);

char *bluetooth_pairing_dialog_get_pin (BluetoothPairingDialog *self);
