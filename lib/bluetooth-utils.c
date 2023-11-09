/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009-2011  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
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

/**
 * SECTION:bluetooth-utils
 * @short_description: Bluetooth utility functions
 * @stability: Stable
 * @include: bluetooth-utils.h
 *
 * Those helper functions are used throughout the Bluetooth
 * management utilities.
 **/

#include "config.h"

#include <glib/gi18n-lib.h>

#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

/**
 * bluetooth_type_to_string:
 * @type: a #BluetoothType
 *
 * Returns a human-readable string representation of @type usable for display to users. Do not free the return value.
 * The returned string is already translated with gettext().
 *
 * Return value: a string.
 **/
const gchar *
bluetooth_type_to_string (guint type)
{
	switch (type) {
	case BLUETOOTH_TYPE_PHONE:
		return _("Phone");
	case BLUETOOTH_TYPE_MODEM:
		return _("Modem");
	case BLUETOOTH_TYPE_COMPUTER:
		return _("Computer");
	case BLUETOOTH_TYPE_NETWORK:
		return _("Network");
	case BLUETOOTH_TYPE_HEADSET:
		/* translators: a hands-free headset, a combination of a single speaker with a microphone */
		return _("Headset");
	case BLUETOOTH_TYPE_HEADPHONES:
		return _("Headphones");
	case BLUETOOTH_TYPE_OTHER_AUDIO:
		return _("Audio device");
	case BLUETOOTH_TYPE_KEYBOARD:
		return _("Keyboard");
	case BLUETOOTH_TYPE_MOUSE:
		return _("Mouse");
	case BLUETOOTH_TYPE_CAMERA:
		return _("Camera");
	case BLUETOOTH_TYPE_PRINTER:
		return _("Printer");
	case BLUETOOTH_TYPE_JOYPAD:
		return _("Joypad");
	case BLUETOOTH_TYPE_TABLET:
		return _("Tablet");
	case BLUETOOTH_TYPE_VIDEO:
		return _("Video device");
	case BLUETOOTH_TYPE_REMOTE_CONTROL:
		return _("Remote control");
	case BLUETOOTH_TYPE_SCANNER:
		return _("Scanner");
	case BLUETOOTH_TYPE_DISPLAY:
		return _("Display");
	case BLUETOOTH_TYPE_WEARABLE:
		return _("Wearable");
	case BLUETOOTH_TYPE_TOY:
		return _("Toy");
	case BLUETOOTH_TYPE_SPEAKERS:
		return _("Speakers");
	default:
		return _("Unknown");
	}
}

/**
 * bluetooth_verify_address:
 * @bdaddr: a string representing a Bluetooth address
 *
 * Returns whether the string is a valid Bluetooth address. This does not contact the device in any way.
 *
 * Return value: %TRUE if the address is valid, %FALSE if not.
 **/
gboolean
bluetooth_verify_address (const char *bdaddr)
{
	guint i;

	g_return_val_if_fail (bdaddr != NULL, FALSE);

	if (strlen (bdaddr) != 17)
		return FALSE;

	for (i = 0; i < 17; i++) {
		if (((i + 1) % 3) == 0) {
			if (bdaddr[i] != ':')
				return FALSE;
			continue;
		}
		if (g_ascii_isxdigit (bdaddr[i]) == FALSE)
			return FALSE;
	}

	return TRUE;
}

/**
 * bluetooth_class_to_type:
 * @class: a Bluetooth device class
 *
 * Returns the type of device corresponding to the given @class value.
 *
 * Return value: a #BluetoothType.
 **/
BluetoothType
bluetooth_class_to_type (guint32 class)
{
	switch ((class & 0x1f00) >> 8) {
	case 0x01:
		return BLUETOOTH_TYPE_COMPUTER;
	case 0x02:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x05:
			return BLUETOOTH_TYPE_PHONE;
		case 0x04:
			return BLUETOOTH_TYPE_MODEM;
		}
		break;
	case 0x03:
		return BLUETOOTH_TYPE_NETWORK;
	case 0x04:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			return BLUETOOTH_TYPE_HEADSET;
		case 0x05:
			return BLUETOOTH_TYPE_SPEAKERS;
		case 0x06:
			return BLUETOOTH_TYPE_HEADPHONES;
		case 0x0b: /* VCR */
		case 0x0c: /* Video Camera */
		case 0x0d: /* Camcorder */
			return BLUETOOTH_TYPE_VIDEO;
		default:
			return BLUETOOTH_TYPE_OTHER_AUDIO;
		}
		break;
	case 0x05:
		switch ((class & 0xc0) >> 6) {
		case 0x00:
			switch ((class & 0x1e) >> 2) {
			case 0x01:
			case 0x02:
				return BLUETOOTH_TYPE_JOYPAD;
			case 0x03:
				return BLUETOOTH_TYPE_REMOTE_CONTROL;
			}
			break;
		case 0x01:
			return BLUETOOTH_TYPE_KEYBOARD;
		case 0x02:
			switch ((class & 0x1e) >> 2) {
			case 0x05:
				return BLUETOOTH_TYPE_TABLET;
			default:
				return BLUETOOTH_TYPE_MOUSE;
			}
		}
		break;
	case 0x06:
		if (class & 0x80)
			return BLUETOOTH_TYPE_PRINTER;
		if (class & 0x40)
			return BLUETOOTH_TYPE_SCANNER;
		if (class & 0x20)
			return BLUETOOTH_TYPE_CAMERA;
		if (class & 0x10)
			return BLUETOOTH_TYPE_DISPLAY;
		break;
	case 0x07:
		return BLUETOOTH_TYPE_WEARABLE;
	case 0x08:
		return BLUETOOTH_TYPE_TOY;
	}

	return 0;
}

/**
 * bluetooth_appearance_to_type:
 * @appearance: a Bluetooth device appearance
 *
 * Returns the type of device corresponding to the given @appearance value,
 * as usually found in the GAP service.
 *
 * Return value: a #BluetoothType.
 **/
BluetoothType
bluetooth_appearance_to_type (guint16 appearance)
{
	switch ((appearance & 0xffc0) >> 6) {
	case 0x01:
		return BLUETOOTH_TYPE_PHONE;
	case 0x02:
		return BLUETOOTH_TYPE_COMPUTER;
	case 0x05:
		return BLUETOOTH_TYPE_DISPLAY;
	case 0x0a:
		return BLUETOOTH_TYPE_OTHER_AUDIO;
	case 0x0b:
		return BLUETOOTH_TYPE_SCANNER;
	case 0x0f: /* HID Generic */
		switch (appearance & 0x3f) {
		case 0x01:
			return BLUETOOTH_TYPE_KEYBOARD;
		case 0x02:
			return BLUETOOTH_TYPE_MOUSE;
		case 0x03:
		case 0x04:
			return BLUETOOTH_TYPE_JOYPAD;
		case 0x05:
			return BLUETOOTH_TYPE_TABLET;
		case 0x08:
			return BLUETOOTH_TYPE_SCANNER;
		}
		break;
	case 0x21:
		return BLUETOOTH_TYPE_SPEAKERS;
	case 0x25: /* Audio */
		switch (appearance & 0x3f) {
		case 0x01:
		case 0x02:
		case 0x04:
			return BLUETOOTH_TYPE_HEADSET;
		case 0x03:
			return BLUETOOTH_TYPE_HEADPHONES;
		default:
			return BLUETOOTH_TYPE_OTHER_AUDIO;
		}
		break;
	}

	return 0;

}

/*
 * The profile UUID list is provided by the Bluetooth SIG:
 * https://www.bluetooth.com/specifications/assigned-numbers/
 */
#define BLUETOOTH_UUID_SPP		0x1101
#define BLUETOOTH_UUID_DUN		0x1103
#define BLUETOOTH_UUID_IRMC		0x1104
#define BLUETOOTH_UUID_OPP		0x1105
#define BLUETOOTH_UUID_FTP		0x1106
#define BLUETOOTH_UUID_HSP		0x1108
#define BLUETOOTH_UUID_A2DP_SOURCE	0x110A
#define BLUETOOTH_UUID_A2DP_SINK	0x110B
#define BLUETOOTH_UUID_AVRCP_TARGET	0x110C
#define BLUETOOTH_UUID_A2DP		0x110D
#define BLUETOOTH_UUID_AVRCP_CONTROL	0x110E
#define BLUETOOTH_UUID_HSP_AG		0x1112
#define BLUETOOTH_UUID_PAN_PANU		0x1115
#define BLUETOOTH_UUID_PAN_NAP		0x1116
#define BLUETOOTH_UUID_PAN_GN		0x1117
#define BLUETOOTH_UUID_HFP_HF		0x111E
#define BLUETOOTH_UUID_HFP_AG		0x111F
#define BLUETOOTH_UUID_HID		0x1124
#define BLUETOOTH_UUID_SAP		0x112d
#define BLUETOOTH_UUID_PBAP		0x112F
#define BLUETOOTH_UUID_GENERIC_AUDIO	0x1203
#define BLUETOOTH_UUID_SDP		0x1000
#define BLUETOOTH_UUID_PNP		0x1200
#define BLUETOOTH_UUID_GENERIC_NET	0x1201
#define BLUETOOTH_UUID_VDP_SOURCE	0x1303

#define BLUETOOTH_LE_UUID_BATTERY                0x180F
#define BLUETOOTH_LE_UUID_HUMAN_INTERFACE_DEVICE 0x1812

static const char *
uuid16_custom_to_string (guint uuid16, const char *uuid)
{
	switch (uuid16) {
	case 0x2:
		return "SyncMLClient";
	case 0x5601:
		return "Nokia SyncML Server";
	default:
		g_debug ("Unhandled custom UUID %s (0x%x)", uuid, uuid16);
		return NULL;
	}
}

/* From "16-bit UUID Numbers Document" listed at:
 * https://www.bluetooth.com/specifications/assigned-numbers/ */
static const char *
uuid16_to_string (guint uuid16, const char *uuid)
{
	switch (uuid16) {
	case BLUETOOTH_UUID_SPP:
		return "SerialPort";
	case BLUETOOTH_UUID_DUN:
		return "DialupNetworking";
	case BLUETOOTH_UUID_IRMC:
		return "IrMCSync";
	case BLUETOOTH_UUID_OPP:
		return "OBEXObjectPush";
	case BLUETOOTH_UUID_FTP:
		return "OBEXFileTransfer";
	case BLUETOOTH_UUID_HSP:
		return "HSP";
	case BLUETOOTH_UUID_A2DP_SOURCE:
		return "AudioSource";
	case BLUETOOTH_UUID_A2DP_SINK:
		return "AudioSink";
	case BLUETOOTH_UUID_AVRCP_TARGET:
		return "A/V_RemoteControlTarget";
	case BLUETOOTH_UUID_A2DP:
		return "AdvancedAudioDistribution";
	case BLUETOOTH_UUID_AVRCP_CONTROL:
		return "A/V_RemoteControl";
	case BLUETOOTH_UUID_HSP_AG:
		return "Headset_-_AG";
	case BLUETOOTH_UUID_PAN_PANU:
		return "PANU";
	case BLUETOOTH_UUID_PAN_NAP:
		return "NAP";
	case BLUETOOTH_UUID_PAN_GN:
		return "GN";
	case BLUETOOTH_UUID_HFP_HF:
		return "Handsfree";
	case BLUETOOTH_UUID_HFP_AG:
		return "HandsfreeAudioGateway";
	case BLUETOOTH_UUID_HID:
		return "HumanInterfaceDeviceService";
	case BLUETOOTH_UUID_SAP:
		return "SIM_Access";
	case BLUETOOTH_UUID_PBAP:
		return "Phonebook_Access_-_PSE";
	case BLUETOOTH_UUID_GENERIC_AUDIO:
		return "GenericAudio";
	case BLUETOOTH_UUID_SDP: /* ServiceDiscoveryServerServiceClassID */
	case BLUETOOTH_UUID_PNP: /* PnPInformation */
		/* Those are ignored */
		return NULL;
	case BLUETOOTH_UUID_GENERIC_NET:
		return "GenericNetworking";
	case BLUETOOTH_UUID_VDP_SOURCE:
		return "VideoSource";
	case 0x8e771303:
	case 0x8e771301:
		return "SEMC HLA";
	case 0x8e771401:
		return "SEMC Watch Phone";
	case BLUETOOTH_LE_UUID_BATTERY:
		return "Battery";
	case BLUETOOTH_LE_UUID_HUMAN_INTERFACE_DEVICE:
		return "Human Interface Device";
	default:
		g_debug ("Unhandled UUID %s (0x%x)", uuid, uuid16);
		return NULL;
	}
}

static const char *
uuid128_to_string (const char *uuid)
{
	struct {
		const char *uuid;
		const char *str;
	} uuids128[] = {
		{ "03B80E5A-EDE8-4B33-A751-6CE34EC4C700", "MIDI" },
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (uuids128); i++) {
		if (g_ascii_strcasecmp (uuids128[i].uuid, uuid) == 0)
			return uuids128[i].str;
	}
	return NULL;
}

/**
 * bluetooth_uuid_to_string:
 * @uuid: a string representing a Bluetooth UUID
 *
 * Returns a string representing a human-readable (but not usable for display to users) version of the @uuid. Do not free the return value.
 *
 * Return value: a string.
 **/
const char *
bluetooth_uuid_to_string (const char *uuid)
{
	const char *ret;
	char **parts;
	guint uuid16;
	gboolean is_custom = FALSE;

	ret = uuid128_to_string (uuid);
	if (ret)
		return ret;
	if (g_str_has_suffix (uuid, "-0000-1000-8000-0002ee000002") != FALSE)
		is_custom = TRUE;

	parts = g_strsplit (uuid, "-", -1);
	if (parts == NULL || parts[0] == NULL) {
		g_strfreev (parts);
		return NULL;
	}

	uuid16 = g_ascii_strtoull (parts[0], NULL, 16);
	g_strfreev (parts);
	if (uuid16 == 0)
		return NULL;

	if (is_custom == FALSE)
		return uuid16_to_string (uuid16, uuid);
	return uuid16_custom_to_string (uuid16, uuid);
}

/**
 * bluetooth_send_to_address:
 * @address: Remote device to use
 * @alias: Remote device's name
 * @error: a #GError
 *
 * Start a GUI application for transferring files over Bluetooth.
 *
 * Return value: %TRUE on success, %FALSE on error.
 **/
gboolean
bluetooth_send_to_address (const char  *address,
			   const char  *alias,
			   GError     **error)
{
	GPtrArray *a;
	g_auto(GStrv) args = NULL;

	g_return_val_if_fail (address != NULL, FALSE);
	g_return_val_if_fail (bluetooth_verify_address (address), FALSE);

	a = g_ptr_array_new ();
	g_ptr_array_add (a, g_strdup ("bluetooth-sendto"));
	g_ptr_array_add (a, g_strdup_printf ("--device=%s", address));
	if (alias != NULL)
		g_ptr_array_add (a, g_strdup_printf ("--name=%s", alias));
	g_ptr_array_add (a, NULL);
	args = (GStrv) g_ptr_array_free (a, FALSE);

	return g_spawn_async (NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
}

