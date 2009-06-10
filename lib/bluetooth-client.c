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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>

#include "bluetooth-client.h"
#include "bluetooth-client-glue.h"

#include "marshal.h"

#ifdef DEBUG
#define DBG(fmt, arg...) printf("%s:%s() " fmt "\n", __FILE__, __FUNCTION__ , ## arg)
#else
#define DBG(fmt...)
#endif

#ifndef DBUS_TYPE_G_OBJECT_PATH_ARRAY
#define DBUS_TYPE_G_OBJECT_PATH_ARRAY \
	(dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))
#endif

#ifndef DBUS_TYPE_G_DICTIONARY
#define DBUS_TYPE_G_DICTIONARY \
	(dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#endif

#define BLUEZ_SERVICE	"org.bluez"

#define BLUEZ_MANAGER_PATH	"/"
#define BLUEZ_MANAGER_INTERFACE	"org.bluez.Manager"
#define BLUEZ_ADAPTER_INTERFACE	"org.bluez.Adapter"
#define BLUEZ_DEVICE_INTERFACE	"org.bluez.Device"

static char * detectable_interfaces[] = {
	"org.bluez.Headset",
	"org.bluez.AudioSink",
	"org.bluez.Input"
};

static char * connectable_interfaces[] = {
	"org.bluez.Audio",
	"org.bluez.Input"
};

/* Keep in sync with above */
#define BLUEZ_INPUT_INTERFACE	(connectable_interfaces[1])
#define BLUEZ_AUDIO_INTERFACE (connectable_interfaces[0])
#define BLUEZ_HEADSET_INTERFACE (detectable_interfaces[0])
#define BLUEZ_AUDIOSINK_INTERFACE (detectable_interfaces[1])

static DBusGConnection *connection = NULL;
static BluetoothClient *bluetooth_client = NULL;

#define BLUETOOTH_CLIENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_CLIENT, BluetoothClientPrivate))

typedef struct _BluetoothClientPrivate BluetoothClientPrivate;

struct _BluetoothClientPrivate {
	DBusGProxy *dbus;
	DBusGProxy *manager;
	GtkTreeStore *store;
	char *default_adapter;
	gboolean default_adapter_powered;
};

enum {
	PROP_0,
	PROP_DEFAULT_ADAPTER,
	PROP_DEFAULT_ADAPTER_POWERED,
};

G_DEFINE_TYPE(BluetoothClient, bluetooth_client, G_TYPE_OBJECT)

const gchar *bluetooth_type_to_string(guint type)
{
	switch (type) {
	case BLUETOOTH_TYPE_ANY:
		return N_("All types");
	case BLUETOOTH_TYPE_PHONE:
		return N_("Phone");
	case BLUETOOTH_TYPE_MODEM:
		return N_("Modem");
	case BLUETOOTH_TYPE_COMPUTER:
		return N_("Computer");
	case BLUETOOTH_TYPE_NETWORK:
		return N_("Network");
	case BLUETOOTH_TYPE_HEADSET:
		/* translators: a hands-free headset, a combination of a single speaker with a microphone */
		return N_("Headset");
	case BLUETOOTH_TYPE_HEADPHONES:
		return N_("Headphones");
	case BLUETOOTH_TYPE_OTHER_AUDIO:
		return N_("Audio device");
	case BLUETOOTH_TYPE_KEYBOARD:
		return N_("Keyboard");
	case BLUETOOTH_TYPE_MOUSE:
		return N_("Mouse");
	case BLUETOOTH_TYPE_CAMERA:
		return N_("Camera");
	case BLUETOOTH_TYPE_PRINTER:
		return N_("Printer");
	case BLUETOOTH_TYPE_JOYPAD:
		return N_("Joypad");
	case BLUETOOTH_TYPE_TABLET:
		return N_("Tablet");
	default:
		return N_("Unknown");
	}
}

gboolean
bluetooth_verify_address (const char *bdaddr)
{
	gboolean retval = TRUE;
	char **elems;
	guint i;

	g_return_val_if_fail (bdaddr != NULL, FALSE);

	if (strlen (bdaddr) != 17)
		return FALSE;

	elems = g_strsplit (bdaddr, ":", -1);
	if (elems == NULL)
		return FALSE;
	if (g_strv_length (elems) != 6) {
		g_strfreev (elems);
		return FALSE;
	}
	for (i = 0; i < 6; i++) {
		if (strlen (elems[i]) != 2 ||
		    g_ascii_isxdigit (elems[i][0]) == FALSE ||
		    g_ascii_isxdigit (elems[i][1]) == FALSE) {
			retval = FALSE;
			break;
		}
	}

	g_strfreev (elems);
	return retval;
}

static guint class_to_type(guint32 class)
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
		case 0x06:
			return BLUETOOTH_TYPE_HEADPHONES;
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
		if (class & 0x20)
			return BLUETOOTH_TYPE_CAMERA;
		break;
	}

	return 0;
}

typedef gboolean (*IterSearchFunc) (GtkTreeStore *store,
				GtkTreeIter *iter, gpointer user_data);

static gboolean iter_search(GtkTreeStore *store,
				GtkTreeIter *iter, GtkTreeIter *parent,
				IterSearchFunc func, gpointer user_data)
{
	gboolean cont, found = FALSE;

	if (parent == NULL)
		cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
									iter);
	else
		cont = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),
								iter, parent);

	while (cont == TRUE) {
		GtkTreeIter child;

		found = func(store, iter, user_data);
		if (found == TRUE)
			break;

		found = iter_search(store, &child, iter, func, user_data);
		if (found == TRUE) {
			*iter = child;
			break;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
	}

	return found;
}

static gboolean compare_path(GtkTreeStore *store,
					GtkTreeIter *iter, gpointer user_data)
{
	const gchar *path = user_data;
	DBusGProxy *object;
	gboolean found = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
					BLUETOOTH_COLUMN_PROXY, &object, -1);

	if (object != NULL) {
		found = g_str_equal(path, dbus_g_proxy_get_path(object));
		g_object_unref(object);
	}

	return found;
}

static gboolean
get_iter_from_path (GtkTreeStore *store,
		    GtkTreeIter *iter,
		    const char *path)
{
	return iter_search(store, iter, NULL, compare_path, (gpointer) path);
}

static gboolean
get_iter_from_proxy(GtkTreeStore *store,
		    GtkTreeIter *iter,
		    DBusGProxy *proxy)
{
	return iter_search(store, iter, NULL, compare_path, dbus_g_proxy_get_path (proxy));
}

static void
device_services_changed (DBusGProxy *iface, const char *property,
			 GValue *value, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	GtkTreePath *tree_path;
	GHashTable *table;
	const char *path;
	gboolean is_connected;

	if (g_str_equal (property, "Connected") != FALSE) {
		is_connected = g_value_get_boolean(value);
	} else if (g_str_equal (property, "State") != FALSE) {
		is_connected = (g_strcmp0(g_value_get_string (value), "connected") == 0);
	} else
		return;

	path = dbus_g_proxy_get_path (iface);
	if (get_iter_from_path (priv->store, &iter, path) == FALSE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL (priv->store), &iter,
			   BLUETOOTH_COLUMN_SERVICES, &table,
			   -1);

	g_hash_table_insert (table,
			     (gpointer) dbus_g_proxy_get_interface (iface),
			     GINT_TO_POINTER (is_connected));

	tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (priv->store), tree_path, &iter);
	gtk_tree_path_free (tree_path);
}

static GHashTable *
device_list_nodes (DBusGProxy *device, BluetoothClient *client, gboolean connect_signal)
{
	GHashTable *table;
	guint i;

	if (device == NULL)
		return NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	for (i = 0; i < G_N_ELEMENTS (detectable_interfaces); i++) {
		DBusGProxy *iface;
		GHashTable *props;
		const char *iface_name;

		/* Don't add the input interface for devices that already have
		 * audio stuff */
		if (g_str_equal (detectable_interfaces[i], BLUEZ_INPUT_INTERFACE)
		    && g_hash_table_size (table) > 0)
			continue;
		/* Add org.bluez.Audio if the device supports headset or audiosink */
		if (g_str_equal (detectable_interfaces[i], BLUEZ_HEADSET_INTERFACE) ||
		    g_str_equal (detectable_interfaces[i], BLUEZ_AUDIOSINK_INTERFACE))
		    	iface_name = BLUEZ_AUDIO_INTERFACE;
		else
			iface_name = detectable_interfaces[i];

		/* And skip interface if it's already in the hash table */
		if (g_hash_table_lookup (table, iface_name) != NULL)
			continue;

		iface = dbus_g_proxy_new_from_proxy (device, detectable_interfaces[i], NULL);
		if (dbus_g_proxy_call (iface, "GetProperties", NULL,
				       G_TYPE_INVALID, dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
				       G_TYPE_INVALID) != FALSE) {
			GValue *value;
			gboolean is_connected;

			value = g_hash_table_lookup(props, "Connected");
			if (value != NULL) {
				is_connected = g_value_get_boolean(value);
			} else {
				const char *str = "disconnected";
				value = g_hash_table_lookup(props, "State");
				if (value != NULL)
					str = g_value_get_string(value);

				is_connected = (g_strcmp0(str, "connected") == 0);
			}

			g_hash_table_insert (table, (gpointer) iface_name, GINT_TO_POINTER (is_connected));

			if (connect_signal != FALSE) {
				dbus_g_proxy_add_signal(iface, "PropertyChanged",
							G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
				dbus_g_proxy_connect_signal(iface, "PropertyChanged",
							    G_CALLBACK(device_services_changed), client, NULL);
			}
		}
	}

	if (g_hash_table_size (table) == 0) {
		g_hash_table_destroy (table);
		return NULL;
	}

	return table;
}

/* Short names from Table 2 at:
 * https://www.bluetooth.org/Technical/AssignedNumbers/service_discovery.htm */
static const char *
uuid16_to_string (guint uuid16)
{
	switch (uuid16) {
	case 0x1103:
		return "DialupNetworking";
	case 0x1105:
		return "OBEXObjectPush";
	case 0x1106:
		return "OBEXFileTransfer";
	case 0x110B:
		return "AudioSink";
	case 0x1115:
		return "PANU";
	case 0x1116:
		return "NAP";
	case 0x1124:
		return "HumanInterfaceDeviceService";
	case 0x1000: /* ServiceDiscoveryServerServiceClassID */
	case 0x1200: /* PnPInformation */
		/* Those are ignored */
		return NULL;
	default:
		g_debug ("Unhandled UUID 0x%X", uuid16);
		return NULL;
	}
}

const char *
bluetooth_uuid_to_string (const char *uuid)
{
	char **parts;
	guint uuid16;

	parts = g_strsplit (uuid, "-", -1);
	if (parts == NULL || parts[0] == NULL) {
		g_strfreev (parts);
		return NULL;
	}

	uuid16 = g_ascii_strtoull (parts[0], NULL, 16);
	g_strfreev (parts);
	if (uuid16 == 0)
		return NULL;

	return uuid16_to_string (uuid16);
}

static char **
device_list_uuids (GValue *value)
{
	GPtrArray *ret;
	char **uuids;
	guint i;

	if (value == NULL)
		return NULL;

	uuids = g_value_get_boxed (value);
	if (uuids == NULL)
		return NULL;

	ret = g_ptr_array_new ();

	for (i = 0; uuids[i] != NULL; i++) {
		const char *uuid;

		uuid = bluetooth_uuid_to_string (uuids[i]);
		if (uuid == NULL)
			continue;
		g_ptr_array_add (ret, g_strdup (uuid));
	}
	if (ret->len == 0) {
		g_ptr_array_free (ret, TRUE);
		return NULL;
	}

	g_ptr_array_add (ret, NULL);

	return (char **) g_ptr_array_free (ret, FALSE);
}

static void device_changed(DBusGProxy *device, const char *property,
					GValue *value, gpointer user_data)
{
	BluetoothClient *client = BLUETOOTH_CLIENT(user_data);
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	DBG("client %p property %s", client, property);

	if (get_iter_from_proxy(priv->store, &iter, device) == FALSE)
		return;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_value_get_string(value);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_NAME, name, -1);
	} else if (g_str_equal(property, "Alias") == TRUE) {
		const gchar *alias = g_value_get_string(value);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_ALIAS, alias, -1);
	} else if (g_str_equal(property, "Icon") == TRUE) {
		const gchar *icon = g_value_get_string(value);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_ICON, icon, -1);
	} else if (g_str_equal(property, "Paired") == TRUE) {
		gboolean paired = g_value_get_boolean(value);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_PAIRED, paired, -1);
	} else if (g_str_equal(property, "Trusted") == TRUE) {
		gboolean trusted = g_value_get_boolean(value);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_TRUSTED, trusted, -1);
	} else if (g_str_equal(property, "Connected") == TRUE) {
		gboolean connected = g_value_get_boolean(value);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_CONNECTED, connected, -1);
	} else if (g_str_equal (property, "UUIDs") == TRUE) {
		GHashTable *services;
		char **uuids;

		services = device_list_nodes (device, client, TRUE);
		uuids = device_list_uuids (value);
		gtk_tree_store_set(priv->store, &iter,
				   BLUETOOTH_COLUMN_SERVICES, services,
				   BLUETOOTH_COLUMN_UUIDS, uuids, -1);
		if (services != NULL)
			g_hash_table_unref (services);
		g_strfreev (uuids);
	}
}

static void add_device(DBusGProxy *adapter, GtkTreeIter *parent,
					BluetoothClient *client,
					const char *path, GHashTable *hash)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	DBusGProxy *device;
	GValue *value;
	const gchar *address, *alias, *name, *icon;
	char **uuids;
	GHashTable *services;
	gboolean paired, trusted, connected, legacypairing;
	guint type;
	gint rssi;
	GtkTreeIter iter;
	gboolean cont;

	services = NULL;

	if (hash == NULL) {
		device = dbus_g_proxy_new_from_proxy(adapter,
						BLUEZ_DEVICE_INTERFACE, path);

		if (device != NULL)
			device_get_properties(device, &hash, NULL);
	} else
		device = NULL;

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Alias");
		alias = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		name = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Class");
		type = class_to_type(g_value_get_uint(value));

		value = g_hash_table_lookup(hash, "Icon");
		icon = value ? g_value_get_string(value) : "bluetooth";

		value = g_hash_table_lookup(hash, "RSSI");
		rssi = value ? g_value_get_int(value) : 0;

		value = g_hash_table_lookup(hash, "Paired");
		paired = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Trusted");
		trusted = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Connected");
		connected = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "UUIDs");
		uuids = device_list_uuids (value);

		value = g_hash_table_lookup(hash, "LegacyPairing");
		legacypairing = value ? g_value_get_boolean(value) : TRUE;
	} else {
		if (device)
			g_object_unref (device);
		return;
	}

	cont = gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->store),
								&iter, parent);

	while (cont == TRUE) {
		gchar *value;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
					BLUETOOTH_COLUMN_ADDRESS, &value, -1);

		if (g_ascii_strcasecmp(address, value) == 0) {
			gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_ADDRESS, address,
					BLUETOOTH_COLUMN_ALIAS, alias,
					BLUETOOTH_COLUMN_NAME, name,
					BLUETOOTH_COLUMN_TYPE, type,
					BLUETOOTH_COLUMN_ICON, icon,
					BLUETOOTH_COLUMN_RSSI, rssi,
					BLUETOOTH_COLUMN_UUIDS, uuids,
					BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
					-1);

			if (device != NULL) {
				services = device_list_nodes (device, client, FALSE);

				gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_PROXY, device,
					BLUETOOTH_COLUMN_CONNECTED, connected,
					BLUETOOTH_COLUMN_TRUSTED, trusted,
					BLUETOOTH_COLUMN_PAIRED, paired,
					BLUETOOTH_COLUMN_SERVICES, services,
					-1);
			}

			goto done;
		}
		g_free (value);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store), &iter);
	}

	services = device_list_nodes (device, client, TRUE);

	gtk_tree_store_insert_with_values(priv->store, &iter, parent, -1,
				BLUETOOTH_COLUMN_PROXY, device,
				BLUETOOTH_COLUMN_ADDRESS, address,
				BLUETOOTH_COLUMN_ALIAS, alias,
				BLUETOOTH_COLUMN_NAME, name,
				BLUETOOTH_COLUMN_TYPE, type,
				BLUETOOTH_COLUMN_ICON, icon,
				BLUETOOTH_COLUMN_RSSI, rssi,
				BLUETOOTH_COLUMN_PAIRED, paired,
				BLUETOOTH_COLUMN_TRUSTED, trusted,
				BLUETOOTH_COLUMN_CONNECTED, connected,
				BLUETOOTH_COLUMN_SERVICES, services,
				BLUETOOTH_COLUMN_UUIDS, uuids,
				BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
				-1);

done:
	if (device != NULL) {
		dbus_g_proxy_add_signal(device, "PropertyChanged",
				G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(device, "PropertyChanged",
				G_CALLBACK(device_changed), client, NULL);
		g_object_unref(device);
	}
	g_strfreev (uuids);
	if (services)
		g_hash_table_unref (services);
}

static void device_found(DBusGProxy *adapter, const char *address,
					GHashTable *hash, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	DBG("client %p address %s", client, address);

	if (get_iter_from_proxy(priv->store, &iter, adapter) == TRUE)
		add_device(adapter, &iter, client, NULL, hash);
}

static void device_created(DBusGProxy *adapter,
				const char *path, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	DBG("client %p path %s", client, path);

	if (get_iter_from_proxy(priv->store, &iter, adapter) == TRUE)
		add_device(adapter, &iter, client, path, NULL);
}

static void device_removed(DBusGProxy *adapter,
				const char *path, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	DBG("client %p path %s", client, path);

	if (get_iter_from_path(priv->store, &iter, path) == TRUE)
		gtk_tree_store_remove(priv->store, &iter);
}

static void adapter_changed(DBusGProxy *adapter, const char *property,
					GValue *value, gpointer user_data)
{
	BluetoothClient *client = BLUETOOTH_CLIENT(user_data);
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean notify = FALSE;

	DBG("client %p property %s", client, property);

	if (get_iter_from_proxy(priv->store, &iter, adapter) == FALSE)
		return;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_value_get_string(value);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_NAME, name, -1);
	} else if (g_str_equal(property, "Discovering") == TRUE) {
		gboolean discovering = g_value_get_boolean(value);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);
		notify = TRUE;
	}

	if (notify != FALSE) {
		GtkTreePath *path;

		/* Tell the world */
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (priv->store), path, &iter);
		gtk_tree_path_free (path);
	}
}

static void adapter_added(DBusGProxy *manager,
				const char *path, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	DBusGProxy *adapter;
	GPtrArray *devices;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *name;
	gboolean discovering, powered;

	DBG("client %p path %s", client, path);

	adapter = dbus_g_proxy_new_from_proxy(manager,
					BLUEZ_ADAPTER_INTERFACE, path);

	adapter_get_properties(adapter, &hash, NULL);
	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Name");
		name = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Discovering");
		discovering = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Powered");
		powered = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Devices");
		devices = value ? g_value_get_boxed (value) : NULL;
	} else {
		address = NULL;
		name = NULL;
		discovering = FALSE;
		powered = FALSE;
		devices = NULL;
	}

	gtk_tree_store_insert_with_values(priv->store, &iter, NULL, -1,
					  BLUETOOTH_COLUMN_PROXY, adapter,
					  BLUETOOTH_COLUMN_ADDRESS, address,
					  BLUETOOTH_COLUMN_NAME, name,
					  BLUETOOTH_COLUMN_DISCOVERING, discovering,
					  BLUETOOTH_COLUMN_POWERED, powered,
					  -1);

	dbus_g_proxy_add_signal(adapter, "PropertyChanged",
				G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(adapter, "PropertyChanged",
				G_CALLBACK(adapter_changed), client, NULL);

	dbus_g_proxy_add_signal(adapter, "DeviceCreated",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(adapter, "DeviceCreated",
				G_CALLBACK(device_created), client, NULL);

	dbus_g_proxy_add_signal(adapter, "DeviceRemoved",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(adapter, "DeviceRemoved",
				G_CALLBACK(device_removed), client, NULL);

	dbus_g_proxy_add_signal(adapter, "DeviceFound",
			G_TYPE_STRING, DBUS_TYPE_G_DICTIONARY, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(adapter, "DeviceFound",
				G_CALLBACK(device_found), client, NULL);

	if (devices != NULL) {
		int i;

		for (i = 0; i < devices->len; i++) {
			gchar *path = g_ptr_array_index(devices, i);
			device_created(adapter, path, client);
			g_free(path);
		}
	}

	g_object_unref(adapter);
}

static void adapter_removed(DBusGProxy *manager,
				const char *path, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	DBG("client %p path %s", client, path);

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		DBusGProxy *adapter;
		const char *adapter_path;
		gboolean found;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
					BLUETOOTH_COLUMN_PROXY, &adapter, -1);

		adapter_path = dbus_g_proxy_get_path(adapter);

		found = g_str_equal(path, adapter_path);
		if (found == TRUE) {
			g_signal_handlers_disconnect_by_func(adapter,
						adapter_changed, client);
			g_signal_handlers_disconnect_by_func(adapter,
						device_created, client);
			g_signal_handlers_disconnect_by_func(adapter,
						device_removed, client);
			g_signal_handlers_disconnect_by_func(adapter,
						device_found, client);
		}

		g_object_unref(adapter);

		if (found == TRUE) {
			cont = gtk_tree_store_remove(priv->store, &iter);
			continue;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	/* No adapters left in the tree? */
	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL(priv->store), NULL) == 0) {
		g_free(priv->default_adapter);
		priv->default_adapter = NULL;
		g_object_notify (G_OBJECT (client), "default-adapter");
	}
}

static void default_adapter_changed(DBusGProxy *manager,
				const char *path, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	DBG("client %p path %s", client, path);

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		DBusGProxy *adapter;
		const char *adapter_path;
		gboolean found, powered;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_PROXY, &adapter,
				   BLUETOOTH_COLUMN_POWERED, &powered, -1);

		adapter_path = dbus_g_proxy_get_path(adapter);

		found = g_str_equal(path, adapter_path);

		g_object_unref(adapter);

		if (found != FALSE)
			priv->default_adapter_powered = powered;

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_DEFAULT, found, -1);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	/* Record the new default adapter */
	g_free(priv->default_adapter);
	priv->default_adapter = g_strdup(path);
	g_object_notify (G_OBJECT (client), "default-adapter");
	g_object_notify (G_OBJECT (client), "default-adapter-powered");
}

static void name_owner_changed(DBusGProxy *dbus, const char *name,
			const char *prev, const char *new, gpointer user_data)
{
	BluetoothClient *client = user_data;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	if (g_str_equal(name, BLUEZ_SERVICE) == FALSE || *new != '\0')
		return;

	DBG("client %p name %s", client, name);

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE)
		cont = gtk_tree_store_remove(priv->store, &iter);
}

static void bluetooth_client_init(BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GPtrArray *array = NULL;
	gchar *default_path = NULL;

	DBG("client %p", client);

	priv->store = gtk_tree_store_new(_BLUETOOTH_NUM_COLUMNS, G_TYPE_OBJECT,
					 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_UINT, G_TYPE_STRING, G_TYPE_INT,
					 G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN, G_TYPE_HASH_TABLE, G_TYPE_STRV);

	priv->dbus = dbus_g_proxy_new_for_name(connection, DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal(priv->dbus, "NameOwnerChanged",
					G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(priv->dbus, "NameOwnerChanged",
				G_CALLBACK(name_owner_changed), client, NULL);

	priv->manager = dbus_g_proxy_new_for_name(connection, BLUEZ_SERVICE,
				BLUEZ_MANAGER_PATH, BLUEZ_MANAGER_INTERFACE);

	dbus_g_proxy_add_signal(priv->manager, "AdapterAdded",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(priv->manager, "AdapterAdded",
				G_CALLBACK(adapter_added), client, NULL);

	dbus_g_proxy_add_signal(priv->manager, "AdapterRemoved",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(priv->manager, "AdapterRemoved",
				G_CALLBACK(adapter_removed), client, NULL);

	dbus_g_proxy_add_signal(priv->manager, "DefaultAdapterChanged",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(priv->manager, "DefaultAdapterChanged",
			G_CALLBACK(default_adapter_changed), client, NULL);

	manager_list_adapters(priv->manager, &array, NULL);
	if (array != NULL) {
		int i;

		for (i = 0; i < array->len; i++) {
			gchar *path = g_ptr_array_index(array, i);
			adapter_added(priv->manager, path, client);
			g_free(path);
		}
	}

	manager_default_adapter(priv->manager, &default_path, NULL);
	if (default_path != NULL) {
		default_adapter_changed(priv->manager, default_path, client);
		g_free(default_path);
	}
}

static void
bluetooth_client_get_property (GObject        *object,
			       guint           property_id,
			       GValue         *value,
			       GParamSpec     *pspec)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(object);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER:
		g_value_set_string (value, priv->default_adapter);
		break;
	case PROP_DEFAULT_ADAPTER_POWERED:
		g_value_set_boolean (value, priv->default_adapter_powered);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void bluetooth_client_finalize(GObject *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);

	DBG("client %p", client);

	g_signal_handlers_disconnect_by_func(priv->dbus,
					name_owner_changed, client);
	g_object_unref(priv->dbus);

	g_signal_handlers_disconnect_by_func(priv->manager,
						adapter_added, client);
	g_signal_handlers_disconnect_by_func(priv->manager,
						adapter_removed, client);
	g_signal_handlers_disconnect_by_func(priv->manager,
					default_adapter_changed, client);
	g_object_unref(priv->manager);

	g_object_unref(priv->store);

	g_free (priv->default_adapter);
	priv->default_adapter = NULL;

	G_OBJECT_CLASS(bluetooth_client_parent_class)->finalize(client);
}

static void bluetooth_client_class_init(BluetoothClientClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GError *error = NULL;

	DBG("class %p", klass);

	g_type_class_add_private(klass, sizeof(BluetoothClientPrivate));

	object_class->finalize = bluetooth_client_finalize;
	object_class->get_property = bluetooth_client_get_property;


	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER,
					 g_param_spec_string ("default-adapter", NULL, NULL,
							      NULL, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_POWERED,
					 g_param_spec_boolean ("default-adapter-powered", NULL, NULL,
					 		       FALSE, G_PARAM_READABLE));

	dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
						G_TYPE_NONE, G_TYPE_STRING,
						G_TYPE_VALUE, G_TYPE_INVALID);

	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

	if (error != NULL) {
		g_printerr("Connecting to system bus failed: %s\n",
							error->message);
		g_error_free(error);
	}
}

BluetoothClient *bluetooth_client_new(void)
{
	if (bluetooth_client != NULL)
		return g_object_ref(bluetooth_client);

	bluetooth_client = BLUETOOTH_CLIENT(g_object_new(BLUETOOTH_TYPE_CLIENT,
									NULL));

	DBG("client %p", bluetooth_client);

	return bluetooth_client;
}

GtkTreeModel *bluetooth_client_get_model (BluetoothClient *client)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	DBG("client %p", client);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = g_object_ref(priv->store);

	return model;
}

GtkTreeModel *bluetooth_client_get_filter_model (BluetoothClient *client,
						 GtkTreeModelFilterVisibleFunc func,
						 gpointer data, GDestroyNotify destroy)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	DBG("client %p", client);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store), NULL);

	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
							func, data, destroy);

	return model;
}

static gboolean adapter_filter(GtkTreeModel *model,
					GtkTreeIter *iter, gpointer user_data)
{
	DBusGProxy *proxy;
	gboolean active;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PROXY, &proxy, -1);

	if (proxy == NULL)
		return FALSE;

	active = g_str_equal(BLUEZ_ADAPTER_INTERFACE,
					dbus_g_proxy_get_interface(proxy));

	g_object_unref(proxy);

	return active;
}

GtkTreeModel *bluetooth_client_get_adapter_model (BluetoothClient *client)
{
	DBG("client %p", client);

	return bluetooth_client_get_filter_model (client, adapter_filter,
						  NULL, NULL);
}

GtkTreeModel *bluetooth_client_get_device_model (BluetoothClient *client,
						 DBusGProxy *adapter)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean cont, found = FALSE;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	DBG("client %p", client);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		DBusGProxy *proxy;
		gboolean is_default;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				BLUETOOTH_COLUMN_PROXY, &proxy,
				BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

		if (adapter == NULL && is_default == TRUE)
			found = TRUE;

		if (proxy == adapter)
			found = TRUE;

		g_object_unref(proxy);

		if (found == TRUE)
			break;

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	if (found == TRUE) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->store),
									&iter);
		model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store),
									path);
		gtk_tree_path_free(path);
	} else
		model = NULL;

	return model;
}

GtkTreeModel *bluetooth_client_get_device_filter_model(BluetoothClient *client,
		DBusGProxy *adapter, GtkTreeModelFilterVisibleFunc func,
				gpointer data, GDestroyNotify destroy)
{
	GtkTreeModel *model;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	DBG("client %p", client);

	model = bluetooth_client_get_device_model(client, adapter);

	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
							func, data, destroy);

	return model;
}

DBusGProxy *bluetooth_client_get_default_adapter(BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	DBG("client %p", client);

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		DBusGProxy *adapter;
		gboolean is_default;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				BLUETOOTH_COLUMN_PROXY, &adapter,
				BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

		if (is_default == TRUE)
			return adapter;

		g_object_unref(adapter);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	return NULL;
}

gboolean bluetooth_client_start_discovery(BluetoothClient *client)
{
	DBusGProxy *adapter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);

	DBG("client %p", client);

	adapter = bluetooth_client_get_default_adapter(client);
	if (adapter == NULL)
		return FALSE;

	adapter_start_discovery(adapter, NULL);

	g_object_unref(adapter);

	return TRUE;
}

gboolean bluetooth_client_stop_discovery(BluetoothClient *client)
{
	DBusGProxy *adapter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);

	DBG("client %p", client);

	adapter = bluetooth_client_get_default_adapter(client);
	if (adapter == NULL)
		return FALSE;

	adapter_stop_discovery(adapter, NULL);

	g_object_unref(adapter);

	return TRUE;
}

typedef struct {
	BluetoothClientCreateDeviceFunc func;
	gpointer data;
} CreateDeviceData;

static void create_device_callback(DBusGProxy *proxy,
					DBusGProxyCall *call, void *user_data)
{
	CreateDeviceData *devdata = user_data;
	GError *error = NULL;
	char *path = NULL;

	dbus_g_proxy_end_call(proxy, call, &error,
			DBUS_TYPE_G_OBJECT_PATH, &path, G_TYPE_INVALID);

	if (error != NULL) {
		path = NULL;
		g_error_free(error);
	}

	if (devdata->func)
		devdata->func(bluetooth_client, path, devdata->data);

	g_object_unref(proxy);
}

gboolean bluetooth_client_create_device(BluetoothClient *client,
			const char *address, const char *agent,
			BluetoothClientCreateDeviceFunc func, gpointer data)
{
	CreateDeviceData *devdata;
	DBusGProxy *adapter;
	DBusGProxyCall *call;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	DBG("client %p", client);

	adapter = bluetooth_client_get_default_adapter(client);
	if (adapter == NULL)
		return FALSE;

	devdata = g_try_new0(CreateDeviceData, 1);
	if (devdata == NULL)
		return FALSE;

	devdata->func = func;
	devdata->data = data;

	if (agent != NULL)
		call = dbus_g_proxy_begin_call_with_timeout(adapter,
				"CreatePairedDevice", create_device_callback,
				devdata, g_free, 90 * 1000,
				G_TYPE_STRING, address,
				DBUS_TYPE_G_OBJECT_PATH, agent,
				G_TYPE_STRING, "DisplayOnly", G_TYPE_INVALID);
	else
		call = dbus_g_proxy_begin_call_with_timeout(adapter,
				"CreateDevice", create_device_callback,
				devdata, g_free, 60 * 1000,
				G_TYPE_STRING, address, G_TYPE_INVALID);

	return TRUE;
}

gboolean bluetooth_client_set_trusted(BluetoothClient *client,
					const char *device, gboolean trusted)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	DBusGProxy *proxy;
	GValue value = { 0 };

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	DBG("client %p", client);

	proxy = dbus_g_proxy_new_from_proxy(priv->manager,
					BLUEZ_DEVICE_INTERFACE, device);
	if (proxy == NULL)
		return FALSE;

	g_value_init(&value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&value, trusted);

	dbus_g_proxy_call(proxy, "SetProperty", NULL,
					G_TYPE_STRING, "Trusted",
					G_TYPE_VALUE, &value,
					G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&value);

	g_object_unref(proxy);

	return TRUE;
}

typedef struct {
	BluetoothClientConnectFunc func;
	gpointer data;
	/* used for disconnect */
	GList *services;
} ConnectData;

static void connect_callback(DBusGProxy *proxy,
			     DBusGProxyCall *call, void *user_data)
{
	ConnectData *conndata = user_data;
	GError *error = NULL;
	gboolean retval = TRUE;

	dbus_g_proxy_end_call(proxy, call, &error, G_TYPE_INVALID);

	if (error != NULL) {
		retval = FALSE;
		g_error_free(error);
	}

	if (conndata->func)
		conndata->func(bluetooth_client, retval, conndata->data);

	g_object_unref(proxy);
}

gboolean bluetooth_client_connect_service(BluetoothClient *client,
					  const char *device,
					  BluetoothClientConnectFunc func,
					  gpointer data)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	ConnectData *conndata;
	DBusGProxy *proxy;
	GHashTable *table;
	GtkTreeIter iter;
	const char *iface_name;
	guint i;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	DBG("client %p", client);

	if (get_iter_from_path (priv->store, &iter, device) == FALSE)
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL (priv->store), &iter,
			   BLUETOOTH_COLUMN_SERVICES, &table,
			   -1);
	if (table == NULL)
		return FALSE;

	conndata = g_new0 (ConnectData, 1);

	iface_name = NULL;
	for (i = 0; i < G_N_ELEMENTS (connectable_interfaces); i++) {
		if (g_hash_table_lookup_extended (table, connectable_interfaces[i], NULL, NULL) != FALSE) {
			iface_name = connectable_interfaces[i];
			break;
		}
	}
	g_hash_table_unref (table);

	if (iface_name == NULL) {
		g_printerr("No supported services on the '%s' device\n", device);
		g_free (conndata);
		return FALSE;
	}

	proxy = dbus_g_proxy_new_from_proxy(priv->manager,
						iface_name, device);
	if (proxy == NULL) {
		g_free (conndata);
		return FALSE;
	}

	conndata->func = func;
	conndata->data = data;

	dbus_g_proxy_begin_call(proxy, "Connect",
				connect_callback, conndata, g_free,
				G_TYPE_INVALID);

	return TRUE;
}

static void disconnect_callback(DBusGProxy *proxy,
				DBusGProxyCall *call,
				void *user_data)
{
	ConnectData *conndata = user_data;

	dbus_g_proxy_end_call(proxy, call, NULL, G_TYPE_INVALID);

	if (conndata->services != NULL) {
		DBusGProxy *service;

		service = dbus_g_proxy_new_from_proxy (proxy,
						       conndata->services->data, NULL);

		conndata->services = g_list_remove (conndata->services, conndata->services->data);

		dbus_g_proxy_begin_call(service, "Disconnect",
					disconnect_callback, conndata, NULL,
					G_TYPE_INVALID);

		g_object_unref (proxy);

		return;
	}

	if (conndata->func)
		conndata->func(bluetooth_client, TRUE, conndata->data);

	g_object_unref(proxy);
	g_free (conndata);
}

static int
service_to_index (const char *service)
{
	guint i;

	g_return_val_if_fail (service != NULL, -1);

	for (i = 0; i < G_N_ELEMENTS (connectable_interfaces); i++) {
		if (g_str_equal (connectable_interfaces[i], service) != FALSE)
			return i;
	}

	g_assert_not_reached ();

	return -1;
}

static int
rev_sort_services (const char *servicea, const char *serviceb)
{
	int a, b;

	a = service_to_index (servicea);
	b = service_to_index (serviceb);

	if (a < b)
		return 1;
	if (a > b)
		return -1;
	return 0;
}

gboolean bluetooth_client_disconnect_service (BluetoothClient *client,
					      const char *device,
					      BluetoothClientConnectFunc func,
					      gpointer data)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	ConnectData *conndata;
	DBusGProxy *proxy;
	GHashTable *table;
	GtkTreeIter iter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	DBG("client %p", client);

	if (get_iter_from_path (priv->store, &iter, device) == FALSE)
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL (priv->store), &iter,
			   BLUETOOTH_COLUMN_PROXY, &proxy,
			   BLUETOOTH_COLUMN_SERVICES, &table,
			   -1);
	if (proxy == NULL) {
		if (table != NULL)
			g_hash_table_unref (table);
		return FALSE;
	}

	conndata = g_new0 (ConnectData, 1);

	conndata->func = func;
	conndata->data = data;

	if (table == NULL) {
		dbus_g_proxy_begin_call(proxy, "Disconnect",
					disconnect_callback, conndata, NULL,
					G_TYPE_INVALID);
	} else {
		DBusGProxy *service;

		conndata->services = g_hash_table_get_keys (table);
		g_hash_table_unref (table);
		conndata->services = g_list_sort (conndata->services, (GCompareFunc) rev_sort_services);

		service = dbus_g_proxy_new_from_proxy (priv->manager,
						       conndata->services->data, device);

		conndata->services = g_list_remove (conndata->services, conndata->services->data);

		dbus_g_proxy_begin_call(service, "Disconnect",
					disconnect_callback, conndata, NULL,
					G_TYPE_INVALID);
	}

	return TRUE;
}

