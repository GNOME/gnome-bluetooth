/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2009  Bastien Nocera <hadess@hadess.net>
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

#ifndef __BLUETOOTH_KILLSWITCH_H
#define __BLUETOOTH_KILLSWITCH_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	KILLSWITCH_STATE_UNKNOWN = -1,
	KILLSWITCH_STATE_KILLED = 0,
	KILLSWITCH_STATE_NOT_KILLED,
	KILLSWITCH_STATE_MIXED,
} KillswitchState;

#define BLUETOOTH_TYPE_KILLSWITCH (bluetooth_killswitch_get_type())
#define BLUETOOTH_KILLSWITCH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
					BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitch))
#define BLUETOOTH_KILLSWITCH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitchClass))
#define BLUETOOTH_IS_KILLSWITCH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
							BLUETOOTH_TYPE_KILLSWITCH))
#define BLUETOOTH_IS_KILLSWITCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
							BLUETOOTH_TYPE_KILLSWITCH))
#define BLUETOOTH_GET_KILLSWITCH_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitchClass))

typedef struct _BluetoothKillswitch BluetoothKillswitch;
typedef struct _BluetoothKillswitchClass BluetoothKillswitchClass;

struct _BluetoothKillswitch {
	GObject parent;
};

struct _BluetoothKillswitchClass {
	GObjectClass parent_class;

	void (*state_changed) (BluetoothKillswitch *killswitch, KillswitchState state);
};

GType bluetooth_killswitch_get_type(void);

BluetoothKillswitch * bluetooth_killswitch_new (void);

gboolean bluetooth_killswitch_has_killswitches (BluetoothKillswitch *killswitch);

void bluetooth_killswitch_update_state (BluetoothKillswitch *killswitch);
void bluetooth_killswitch_set_state (BluetoothKillswitch *killswitch, KillswitchState state);
KillswitchState bluetooth_killswitch_get_state (BluetoothKillswitch *killswitch);

G_END_DECLS

#endif /* __BLUETOOTH_KILLSWITCH_H */
