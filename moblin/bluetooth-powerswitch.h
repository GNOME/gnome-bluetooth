/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2009  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2010       Intel Corp
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __BLUETOOTH_POWERSWITCH_H
#define __BLUETOOTH_POWERSWITCH_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	POWERSWITCH_STATE_NO_ADAPTER = -1,
	POWERSWITCH_STATE_OFF = FALSE,
	POWERSWITCH_STATE_ON = TRUE
} PowerswitchState;

#define BLUETOOTH_TYPE_POWERSWITCH (bluetooth_powerswitch_get_type())
#define BLUETOOTH_POWERSWITCH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
					BLUETOOTH_TYPE_POWERSWITCH, BluetoothPowerswitch))
#define BLUETOOTH_POWERSWITCH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					BLUETOOTH_TYPE_POWERSWITCH, BluetoothPowerswitchClass))
#define BLUETOOTH_IS_POWERSWITCH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
							BLUETOOTH_TYPE_POWERSWITCH))
#define BLUETOOTH_IS_POWERSWITCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
							BLUETOOTH_TYPE_POWERSWITCH))
#define BLUETOOTH_GET_POWERSWITCH_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					BLUETOOTH_TYPE_POWERSWITCH, BluetoothPowerswitchClass))

typedef struct _BluetoothPowerswitch BluetoothPowerswitch;
typedef struct _BluetoothPowerswitchClass BluetoothPowerswitchClass;
typedef struct _BluetoothPowerswitchPrivate BluetoothPowerswitchPrivate;

struct _BluetoothPowerswitch {
	GObject parent;
	BluetoothPowerswitchPrivate *priv;
};

struct _BluetoothPowerswitchClass {
	GObjectClass parent_class;

	void (*state_changed) (BluetoothPowerswitch *powerswitch, PowerswitchState state);
};

GType bluetooth_powerswitch_get_type(void);

BluetoothPowerswitch * bluetooth_powerswitch_new (void);

gboolean bluetooth_powerswitch_has_powerswitches (BluetoothPowerswitch *powerswitch);
void bluetooth_powerswitch_set_state (BluetoothPowerswitch *powerswitch, PowerswitchState state);
PowerswitchState bluetooth_powerswitch_get_state (BluetoothPowerswitch *powerswitch);

G_END_DECLS

#endif /* __BLUETOOTH_POWERSWITCH_H */
