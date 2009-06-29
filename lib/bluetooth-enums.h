/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#ifndef __BLUETOOTH_ENUMS_H
#define __BLUETOOTH_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	BLUETOOTH_CATEGORY_ALL,
	BLUETOOTH_CATEGORY_PAIRED,
	BLUETOOTH_CATEGORY_TRUSTED,
	BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
	BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED,
	BLUETOOTH_CATEGORY_NUM_CATEGORIES /*< skip >*/
} BluetoothCategory;

typedef enum {
	BLUETOOTH_TYPE_ANY		= 1 << 0,
	BLUETOOTH_TYPE_PHONE		= 1 << 1,
	BLUETOOTH_TYPE_MODEM		= 1 << 2,
	BLUETOOTH_TYPE_COMPUTER		= 1 << 3,
	BLUETOOTH_TYPE_NETWORK		= 1 << 4,
	BLUETOOTH_TYPE_HEADSET		= 1 << 5,
	BLUETOOTH_TYPE_HEADPHONES	= 1 << 6,
	BLUETOOTH_TYPE_OTHER_AUDIO	= 1 << 7,
	BLUETOOTH_TYPE_KEYBOARD		= 1 << 8,
	BLUETOOTH_TYPE_MOUSE		= 1 << 9,
	BLUETOOTH_TYPE_CAMERA		= 1 << 10,
	BLUETOOTH_TYPE_PRINTER		= 1 << 11,
	BLUETOOTH_TYPE_JOYPAD		= 1 << 12,
	BLUETOOTH_TYPE_TABLET		= 1 << 13,
} BluetoothType;

#define _BLUETOOTH_TYPE_NUM_TYPES 14

#define BLUETOOTH_TYPE_INPUT (BLUETOOTH_TYPE_KEYBOARD | BLUETOOTH_TYPE_MOUSE | BLUETOOTH_TYPE_TABLET | BLUETOOTH_TYPE_JOYPAD)
#define BLUETOOTH_TYPE_AUDIO (BLUETOOTH_TYPE_HEADSET | BLUETOOTH_TYPE_HEADPHONES | BLUETOOTH_TYPE_OTHER_AUDIO)

typedef enum {
	BLUETOOTH_COLUMN_PROXY,
	BLUETOOTH_COLUMN_ADDRESS,
	BLUETOOTH_COLUMN_ALIAS,
	BLUETOOTH_COLUMN_NAME,
	BLUETOOTH_COLUMN_TYPE,
	BLUETOOTH_COLUMN_ICON,
	BLUETOOTH_COLUMN_RSSI,
	BLUETOOTH_COLUMN_DEFAULT,
	BLUETOOTH_COLUMN_PAIRED,
	BLUETOOTH_COLUMN_TRUSTED,
	BLUETOOTH_COLUMN_CONNECTED,
	BLUETOOTH_COLUMN_DISCOVERING,
	BLUETOOTH_COLUMN_LEGACYPAIRING,
	BLUETOOTH_COLUMN_POWERED,
	BLUETOOTH_COLUMN_SERVICES,
	BLUETOOTH_COLUMN_UUIDS,
	_BLUETOOTH_NUM_COLUMNS /*< skip >*/
} BluetoothColumn;

G_END_DECLS

#endif /* __BLUETOOTH_ENUMS_H */
