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
#define BLUEZ_INPUT_INTERFACE	"org.bluez.Input"

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
};

enum {
	PROP_0,
	PROP_DEFAULT_ADAPTER,
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
		return N_("Headset");
	case BLUETOOTH_TYPE_HEADPHONE:
		return N_("Headphone");
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
			return BLUETOOTH_TYPE_HEADPHONE;
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

	return BLUETOOTH_TYPE_ANY;
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

static gboolean compare_proxy(GtkTreeStore *store,
					GtkTreeIter *iter, gpointer user_data)
{
	DBusGProxy *proxy = user_data;
	DBusGProxy *object;
	gboolean found = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
					BLUETOOTH_COLUMN_PROXY, &object, -1);

	if (object != NULL) {
		found = g_str_equal(dbus_g_proxy_get_path(proxy),
						dbus_g_proxy_get_path(object));
		g_object_unref(object);
	}

	return found;
}

static gboolean get_iter_from_proxy(GtkTreeStore *store,
					GtkTreeIter *iter, DBusGProxy *proxy)
{
	return iter_search(store, iter, NULL, compare_proxy, proxy);
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

static gboolean get_iter_from_path(GtkTreeStore *store,
					GtkTreeIter *iter, const char *path)
{
	return iter_search(store, iter, NULL, compare_path, (gpointer) path);
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
	gboolean paired, trusted, connected;
	guint type;
	gint rssi;
	GtkTreeIter iter;
	gboolean cont;

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
	} else {
		address = NULL;
		alias = NULL;
		name = NULL;
		type = BLUETOOTH_TYPE_ANY;
		icon = NULL;
		rssi = 0;
		paired = FALSE;
		trusted = FALSE;
		connected = FALSE;
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
					BLUETOOTH_COLUMN_RSSI, rssi, -1);

			if (device != NULL) {
				gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_PROXY, device,
					BLUETOOTH_COLUMN_CONNECTED, connected,
					BLUETOOTH_COLUMN_TRUSTED, trusted,
					BLUETOOTH_COLUMN_PAIRED, paired, -1);
			}

			goto done;
		}
		g_free (value);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store), &iter);
	}

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
				BLUETOOTH_COLUMN_CONNECTED, connected, -1);

done:
	if (device != NULL) {
		dbus_g_proxy_add_signal(device, "PropertyChanged",
				G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(device, "PropertyChanged",
				G_CALLBACK(device_changed), client, NULL);

		g_object_unref(device);
	}
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

	DBG("client %p property %s", client, property);

	if (get_iter_from_proxy(priv->store, &iter, adapter) == FALSE)
		return;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_value_get_string(value);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_NAME, name, -1);
	} else if (g_str_equal(property, "Discovering") == TRUE) {
		gboolean discovering = g_value_get_boolean(value);
		GtkTreePath *path;

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);

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
	GPtrArray *array = NULL;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address, *name;
	gboolean discovering;

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
	} else {
		address = NULL;
		name = NULL;
		discovering = FALSE;
	}

	gtk_tree_store_insert_with_values(priv->store, &iter, NULL, -1,
				BLUETOOTH_COLUMN_PROXY, adapter,
				BLUETOOTH_COLUMN_ADDRESS, address,
				BLUETOOTH_COLUMN_NAME, name,
				BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);

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

	adapter_list_devices(adapter, &array, NULL);
	if (array != NULL) {
		int i;

		for (i = 0; i < array->len; i++) {
			gchar *path = g_ptr_array_index(array, i);
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
		gboolean found;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
					BLUETOOTH_COLUMN_PROXY, &adapter, -1);

		adapter_path = dbus_g_proxy_get_path(adapter);

		found = g_str_equal(path, adapter_path);

		g_object_unref(adapter);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_DEFAULT, found, -1);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	/* Record the new default adapter */
	g_free(priv->default_adapter);
	priv->default_adapter = g_strdup(path);
	g_object_notify (G_OBJECT (client), "default-adapter");
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
					G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

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

GtkTreeModel *bluetooth_client_get_model(BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeModel *model;

	DBG("client %p", client);

	model = g_object_ref(priv->store);

	return model;
}

GtkTreeModel *bluetooth_client_get_filter_model(BluetoothClient *client,
				GtkTreeModelFilterVisibleFunc func,
				gpointer data, GDestroyNotify destroy)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeModel *model;

	DBG("client %p", client);

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

GtkTreeModel *bluetooth_client_get_adapter_model(BluetoothClient *client)
{
	DBG("client %p", client);

	return bluetooth_client_get_filter_model(client, adapter_filter,
								NULL, NULL);
}

GtkTreeModel *bluetooth_client_get_device_model(BluetoothClient *client,
							DBusGProxy *adapter)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean cont, found = FALSE;

	DBG("client %p", client);

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
		devdata->func(path, devdata->data);

	g_object_unref(proxy);
}

gboolean bluetooth_client_create_device(BluetoothClient *client,
			const char *address, const char *agent,
			BluetoothClientCreateDeviceFunc func, gpointer data)
{
	CreateDeviceData *devdata;
	DBusGProxy *adapter;
	DBusGProxyCall *call;

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
} ConnectData;

static void connect_input_callback(DBusGProxy *proxy,
					DBusGProxyCall *call, void *user_data)
{
	ConnectData *conndata = user_data;
	GError *error = NULL;

	dbus_g_proxy_end_call(proxy, call, &error, G_TYPE_INVALID);

	if (error != NULL)
		g_error_free(error);

	if (conndata->func)
		conndata->func(conndata->data);

	g_object_unref(proxy);
}

gboolean bluetooth_client_connect_input(BluetoothClient *client,
				const char *device,
				BluetoothClientConnectFunc func, gpointer data)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	ConnectData *conndata;
	DBusGProxy *proxy;
	DBusGProxyCall *call;

	DBG("client %p", client);

	proxy = dbus_g_proxy_new_from_proxy(priv->manager,
						BLUEZ_INPUT_INTERFACE, device);
	if (proxy == NULL)
		return FALSE;

	conndata = g_try_new0(ConnectData, 1);
	if (conndata == NULL) {
		g_object_unref(proxy);
		return FALSE;
	}

	conndata->func = func;
	conndata->data = data;

	call = dbus_g_proxy_begin_call(proxy, "Connect",
				connect_input_callback, conndata, g_free,
							G_TYPE_INVALID);

	return TRUE;
}
