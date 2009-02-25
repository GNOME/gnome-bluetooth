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

G_BEGIN_DECLS

enum {
	BLUETOOTH_CATEGORY_ALL,
	BLUETOOTH_CATEGORY_PAIRED,
	BLUETOOTH_CATEGORY_TRUSTED,
	BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
	BLUETOOTH_CATEGORY_NUM_CATEGORIES
};

enum {
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
};

#define _BLUETOOTH_TYPE_NUM_TYPES 12

#define BLUETOOTH_TYPE_INPUT (BLUETOOTH_TYPE_KEYBOARD | BLUETOOTH_TYPE_MOUSE)

G_END_DECLS

#endif /* __BLUETOOTH_ENUMS_H */
