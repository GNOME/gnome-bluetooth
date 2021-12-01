/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
 *  Copyright (C) 2013       Intel Corporation.
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
 * SECTION:bluetooth-client
 * @short_description: Bluetooth client object
 * @stability: Stable
 * @include: bluetooth-client.h
 *
 * The #BluetoothClient object is used to query the state of Bluetooth
 * devices and adapters.
 **/

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "bluetooth-client-glue.h"
#include "bluetooth-device.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"
#include "pin.h"

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_MANAGER_PATH		"/"
#define BLUEZ_ADAPTER_INTERFACE		"org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE		"org.bluez.Device1"

struct _BluetoothClient {
	GObject parent;

	GListStore *list_store;
	Adapter1 *default_adapter;
	GDBusObjectManager *manager;
	GCancellable *cancellable;
	GtkTreeStore *store;
	guint num_adapters;
	/* Discoverable during discovery? */
	gboolean disco_during_disco;
	gboolean discovery_started;
};

enum {
	PROP_0,
	PROP_NUM_ADAPTERS,
	PROP_DEFAULT_ADAPTER,
	PROP_DEFAULT_ADAPTER_POWERED,
	PROP_DEFAULT_ADAPTER_SETUP_MODE,
	PROP_DEFAULT_ADAPTER_NAME,
	PROP_DEFAULT_ADAPTER_ADDRESS
};

enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static const char *connectable_uuids[] = {
	"HSP",
	"AudioSource",
	"AudioSink",
	"A/V_RemoteControlTarget",
	"A/V_RemoteControl",
	"Headset_-_AG",
	"Handsfree",
	"HandsfreeAudioGateway",
	"HumanInterfaceDeviceService",
};

G_DEFINE_TYPE(BluetoothClient, bluetooth_client, G_TYPE_OBJECT)

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

static gboolean
compare_path (GtkTreeStore *store,
	      GtkTreeIter *iter,
	      gpointer user_data)
{
	const gchar *path = user_data;
	g_autoptr(GDBusProxy) object = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
			    BLUETOOTH_COLUMN_PROXY, &object,
			    -1);

	return (object != NULL &&
		g_str_equal (path, g_dbus_proxy_get_object_path (object)));
}

static gboolean
compare_address (GtkTreeStore *store,
		 GtkTreeIter *iter,
		 gpointer user_data)
{
	const char *address = user_data;
	g_autofree char *tmp_address = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
			    BLUETOOTH_COLUMN_ADDRESS, &tmp_address, -1);
	return (g_strcmp0 (address, tmp_address) == 0);
}

static gboolean
get_iter_from_path (GtkTreeStore *store,
		    GtkTreeIter  *iter,
		    const char   *path)
{
	g_return_val_if_fail (path != NULL, FALSE);
	return iter_search(store, iter, NULL, compare_path, (gpointer) path);
}

static gboolean
get_iter_from_proxy(GtkTreeStore *store,
		    GtkTreeIter *iter,
		    GDBusProxy *proxy)
{
	g_return_val_if_fail (proxy != NULL, FALSE);
	return iter_search(store, iter, NULL, compare_path,
			   (gpointer) g_dbus_proxy_get_object_path (proxy));
}

static gboolean
get_iter_from_address (GtkTreeStore *store,
		       GtkTreeIter  *iter,
		       const char   *address,
		       GDBusProxy   *adapter)
{
	GtkTreeIter parent_iter;

	g_return_val_if_fail (address != NULL, FALSE);
	g_return_val_if_fail (adapter != NULL, FALSE);

	if (get_iter_from_proxy (store, &parent_iter, adapter) == FALSE)
		return FALSE;

	return iter_search (store, iter, &parent_iter, compare_address, (gpointer) address);
}

static BluetoothDevice *
get_device_for_path (BluetoothClient *client,
		     const char      *path)
{
	guint n_items, i;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
	for (i = 0; i < n_items; i++) {
		g_autoptr(BluetoothDevice) d = NULL;

		d = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
		if (g_str_equal (path, bluetooth_device_get_object_path (d))) {
			return g_steal_pointer (&d);
		}
	}
	return NULL;
}

static char **
device_list_uuids (const gchar * const *uuids)
{
	GPtrArray *ret;
	guint i;

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

gboolean
bluetooth_client_get_connectable(const char **uuids)
{
	int i, j;

	for (i = 0; uuids && uuids[i] != NULL; i++) {
		for (j = 0; j < G_N_ELEMENTS (connectable_uuids); j++) {
			if (g_str_equal (connectable_uuids[j], uuids[i]))
				return TRUE;
		}
	}

	return FALSE;
}

static const char *
phone_oui_to_icon_name (const char *bdaddr)
{
	char *vendor;
	const char *ret = NULL;

	vendor = oui_to_vendor (bdaddr);
	if (vendor == NULL)
		return NULL;

	if (strstr (vendor, "Apple") != NULL)
		ret = "phone-apple-iphone";
	else if (strstr (vendor, "Samsung") != NULL)
		ret = "phone-samsung-galaxy-s";
	else if (strstr (vendor, "Google") != NULL)
		ret = "phone-google-nexus-one";
	g_free (vendor);

	return ret;
}

static const char *
icon_override (const char    *bdaddr,
	       BluetoothType  type)
{
	/* audio-card, you're ugly */
	switch (type) {
	case BLUETOOTH_TYPE_HEADSET:
		return "audio-headset";
	case BLUETOOTH_TYPE_HEADPHONES:
		return "audio-headphones";
	case BLUETOOTH_TYPE_OTHER_AUDIO:
		return "audio-speakers";
	case BLUETOOTH_TYPE_PHONE:
		return phone_oui_to_icon_name (bdaddr);
	case BLUETOOTH_TYPE_DISPLAY:
		return "video-display";
	case BLUETOOTH_TYPE_SCANNER:
		return "scanner";
	case BLUETOOTH_TYPE_REMOTE_CONTROL:
	case BLUETOOTH_TYPE_WEARABLE:
	case BLUETOOTH_TYPE_TOY:
		/* FIXME */
	default:
		return NULL;
	}
}

static void
device_resolve_type_and_icon (Device1 *device, BluetoothType *type, const char **icon)
{
	g_return_if_fail (type);
	g_return_if_fail (icon);

	if (g_strcmp0 (device1_get_name (device), "ION iCade Game Controller") == 0 ||
	    g_strcmp0 (device1_get_name (device), "8Bitdo Zero GamePad") == 0) {
		*type = BLUETOOTH_TYPE_JOYPAD;
		*icon = "input-gaming";
		return;
	}

	if (*type == 0 || *type == BLUETOOTH_TYPE_ANY)
		*type = bluetooth_appearance_to_type (device1_get_appearance (device));
	if (*type == 0 || *type == BLUETOOTH_TYPE_ANY)
		*type = bluetooth_class_to_type (device1_get_class (device));

	*icon = icon_override (device1_get_address (device), *type);

	if (!*icon)
		*icon = device1_get_icon (device);

	if (!*icon)
		*icon = "bluetooth";
}

static void
device_notify_cb (Device1         *device1,
		  GParamSpec      *pspec,
		  BluetoothClient *client)
{
	const char *property = g_param_spec_get_name (pspec);
	GtkTreeIter iter;
	g_autoptr(BluetoothDevice) device = NULL;
	const char *device_path;

	if (get_iter_from_proxy (client->store, &iter, G_DBUS_PROXY (device1)) == FALSE)
		return;

	device_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (device1));
	device = get_device_for_path (client, device_path);
	if (!device) {
		g_debug ("Device %s was not known, so property '%s' not applied", device_path, property);
		return;
	}

	g_debug ("Property '%s' changed on device '%s'", property, device_path);

	if (g_strcmp0 (property, "name") == 0) {
		const gchar *name = device1_get_name (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_NAME, name, -1);
		g_object_set (G_OBJECT (device), "name", name, NULL);
	} else if (g_strcmp0 (property, "alias") == 0) {
		const gchar *alias = device1_get_alias (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_ALIAS, alias, -1);
		g_object_set (G_OBJECT (device), "alias", alias, NULL);
	} else if (g_strcmp0 (property, "paired") == 0) {
		gboolean paired = device1_get_paired (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_PAIRED, paired, -1);
		g_object_set (G_OBJECT (device), "paired", paired, NULL);
	} else if (g_strcmp0 (property, "trusted") == 0) {
		gboolean trusted = device1_get_trusted (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_TRUSTED, trusted, -1);
		g_object_set (G_OBJECT (device), "trusted", trusted, NULL);
	} else if (g_strcmp0 (property, "connected") == 0) {
		gboolean connected = device1_get_connected (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_CONNECTED, connected, -1);
		g_object_set (G_OBJECT (device), "connected", connected, NULL);
	} else if (g_strcmp0 (property, "uuids") == 0) {
		g_auto(GStrv) uuids = NULL;

		uuids = device_list_uuids (device1_get_uuids (device1));

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_UUIDS, uuids, -1);
		g_object_set (G_OBJECT (device), "uuids", uuids, NULL);
	} else if (g_strcmp0 (property, "legacy-pairing") == 0) {
		gboolean legacypairing = device1_get_legacy_pairing (device1);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
				    -1);
		g_object_set (G_OBJECT (device), "legacy-pairing", legacypairing, NULL);
	} else if (g_strcmp0 (property, "icon") == 0 ||
		   g_strcmp0 (property, "class") == 0 ||
		   g_strcmp0 (property, "appearance") == 0) {
		BluetoothType type = BLUETOOTH_TYPE_ANY;
		const char *icon = NULL;

		device_resolve_type_and_icon (device1, &type, &icon);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_TYPE, type,
				    BLUETOOTH_COLUMN_ICON, icon,
				    -1);
		g_object_set (G_OBJECT (device),
			      "type", type,
			      "icon", icon,
			      NULL);
	} else {
		g_debug ("Unhandled property: %s", property);
	}
}

static void
device_added (GDBusObjectManager   *manager,
	      Device1              *device,
	      BluetoothClient      *client,
	      gboolean              coldplug)
{
	g_autoptr (GDBusProxy) adapter = NULL;
	const char *adapter_path, *address, *alias, *name, *icon;
	g_auto(GStrv) uuids = NULL;
	gboolean default_adapter;
	gboolean paired, trusted, connected;
	int legacypairing;
	BluetoothType type = BLUETOOTH_TYPE_ANY;
	GtkTreeIter iter, parent;

	g_signal_connect_object (G_OBJECT (device), "notify",
				 G_CALLBACK (device_notify_cb), client, 0);

	adapter_path = device1_get_adapter (device);
	address = device1_get_address (device);
	alias = device1_get_alias (device);
	name = device1_get_name (device);
	paired = device1_get_paired (device);
	trusted = device1_get_trusted (device);
	connected = device1_get_connected (device);
	uuids = device_list_uuids (device1_get_uuids (device));
	legacypairing = device1_get_legacy_pairing (device);

	device_resolve_type_and_icon (device, &type, &icon);

	g_debug ("Inserting device '%s' on adapter '%s'", address, adapter_path);

	if (get_iter_from_path (client->store, &parent, adapter_path) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL(client->store), &parent,
			    BLUETOOTH_COLUMN_PROXY, &adapter,
			    BLUETOOTH_COLUMN_DEFAULT, &default_adapter,
			    -1);

	if (get_iter_from_address (client->store, &iter, address, adapter) == FALSE) {
		gtk_tree_store_insert_with_values (client->store, &iter, &parent, -1,
						   BLUETOOTH_COLUMN_ADDRESS, address,
						   BLUETOOTH_COLUMN_ALIAS, alias,
						   BLUETOOTH_COLUMN_NAME, name,
						   BLUETOOTH_COLUMN_TYPE, type,
						   BLUETOOTH_COLUMN_ICON, icon,
						   BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
						   BLUETOOTH_COLUMN_UUIDS, uuids,
						   BLUETOOTH_COLUMN_PAIRED, paired,
						   BLUETOOTH_COLUMN_CONNECTED, connected,
						   BLUETOOTH_COLUMN_TRUSTED, trusted,
						   BLUETOOTH_COLUMN_PROXY, device,
						   -1);
	} else {
		gtk_tree_store_set(client->store, &iter,
				   BLUETOOTH_COLUMN_ADDRESS, address,
				   BLUETOOTH_COLUMN_ALIAS, alias,
				   BLUETOOTH_COLUMN_NAME, name,
				   BLUETOOTH_COLUMN_TYPE, type,
				   BLUETOOTH_COLUMN_ICON, icon,
				   BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
				   BLUETOOTH_COLUMN_UUIDS, uuids,
				   BLUETOOTH_COLUMN_PAIRED, paired,
				   BLUETOOTH_COLUMN_CONNECTED, connected,
				   BLUETOOTH_COLUMN_TRUSTED, trusted,
				   BLUETOOTH_COLUMN_PROXY, device,
				   -1);
	}

	if (default_adapter && !coldplug) {
		BluetoothDevice *device_obj;

		device_obj = g_object_new (BLUETOOTH_TYPE_DEVICE,
					   "address", address,
					   "alias", alias,
					   "name", name,
					   "type", type,
					   "icon", icon,
					   "legacy-pairing", legacypairing,
					   "uuids", uuids,
					   "paired", paired,
					   "connected", connected,
					   "trusted", trusted,
					   "proxy", device,
					   NULL);
		g_list_store_append (client->list_store, device_obj);
		g_signal_emit (G_OBJECT (client), signals[DEVICE_ADDED], 0, device_obj);
	}
}

static void
device_removed (const char      *path,
		BluetoothClient *client)
{
	GtkTreeIter iter;
	guint i, n_items;

	g_debug ("Removing device '%s'", path);

	if (get_iter_from_path(client->store, &iter, path) == TRUE) {
		/* Note that removal can also happen from adapter_removed. */
		g_signal_emit (G_OBJECT (client), signals[DEVICE_REMOVED], 0, path);
		gtk_tree_store_remove(client->store, &iter);
	}

	n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
	for (i = 0; i < n_items; i++) {
		g_autoptr(BluetoothDevice) device = NULL;

		device = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
		if (g_str_equal (path, bluetooth_device_get_object_path (device))) {
			g_list_store_remove (client->list_store, i);
			return;
		}
	}
	g_debug ("Device %s was not known, so not removed", path);
}

static void
adapter_set_powered_cb (GDBusProxy *proxy,
			GAsyncResult *res,
			gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) ret = NULL;

	ret = g_dbus_proxy_call_finish (proxy, res, &error);
	if (!ret) {
		g_debug ("Error setting property 'Powered' on interface org.bluez.Adapter1: %s (%s, %d)",
			 error->message, g_quark_to_string (error->domain), error->code);
	}
}

static void
adapter_set_powered (BluetoothClient *client,
		     GDBusProxy      *adapter,
		     gboolean         powered)
{
	GVariant *variant;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));

	variant = g_variant_new_boolean (powered);
	g_dbus_proxy_call (adapter,
			   "org.freedesktop.DBus.Properties.Set",
			   g_variant_new ("(ssv)", "org.bluez.Adapter1", "Powered", variant),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL, (GAsyncReadyCallback) adapter_set_powered_cb, client);
}

static void
add_devices_to_list_store (BluetoothClient *client)
{
	GList *object_list, *l;
	const char *default_adapter_path;

	g_debug ("Emptying list store as default adapter changed");
	g_list_store_remove_all (client->list_store);

	default_adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter));

	g_debug ("Coldplugging devices for new default adapter");
	object_list = g_dbus_object_manager_get_objects (client->manager);
	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;
		const char *adapter_path, *address, *alias, *name, *icon;
		g_auto(GStrv) uuids = NULL;
		gboolean paired, trusted, connected;
		int legacypairing;
		BluetoothType type = BLUETOOTH_TYPE_ANY;
		BluetoothDevice *device_obj;

		iface = g_dbus_object_get_interface (object, BLUEZ_DEVICE_INTERFACE);
		if (!iface)
			continue;

		adapter_path = device1_get_adapter (DEVICE1 (iface));
		if (g_strcmp0 (adapter_path, default_adapter_path) != 0)
			continue;

		address = device1_get_address (DEVICE1 (iface));
		alias = device1_get_alias (DEVICE1 (iface));
		name = device1_get_name (DEVICE1 (iface));
		paired = device1_get_paired (DEVICE1 (iface));
		trusted = device1_get_trusted (DEVICE1 (iface));
		connected = device1_get_connected (DEVICE1 (iface));
		uuids = device_list_uuids (device1_get_uuids (DEVICE1 (iface)));
		legacypairing = device1_get_legacy_pairing (DEVICE1 (iface));

		device_resolve_type_and_icon (DEVICE1 (iface), &type, &icon);

		g_debug ("Adding device '%s' on adapter '%s' to list store", address, adapter_path);

		device_obj = g_object_new (BLUETOOTH_TYPE_DEVICE,
					   "address", address,
					   "alias", alias,
					   "name", name,
					   "type", type,
					   "icon", icon,
					   "legacy-pairing", legacypairing,
					   "uuids", uuids,
					   "paired", paired,
					   "connected", connected,
					   "trusted", trusted,
					   "proxy", DEVICE1 (iface),
					   NULL);
		g_list_store_append (client->list_store, device_obj);
		g_signal_emit (G_OBJECT (client), signals[DEVICE_ADDED], 0, device_obj);
	}
	g_list_free_full (object_list, g_object_unref);
}

static void
default_adapter_changed (GDBusObjectManager   *manager,
			 GDBusProxy           *adapter,
			 BluetoothClient      *client)
{
	GtkTreeIter iter;
	gboolean powered;

	g_assert (!client->default_adapter);

	if (get_iter_from_proxy (client->store, &iter, adapter) == FALSE)
		return;

	g_debug ("Setting '%s' as the new default adapter", g_dbus_proxy_get_object_path (adapter));

	client->default_adapter = ADAPTER1 (g_object_ref (G_OBJECT (adapter)));

	gtk_tree_store_set (client->store, &iter,
			    BLUETOOTH_COLUMN_DEFAULT, TRUE, -1);

	add_devices_to_list_store (client);

	gtk_tree_model_get (GTK_TREE_MODEL(client->store), &iter,
			   BLUETOOTH_COLUMN_POWERED, &powered, -1);

	if (powered) {
		g_debug ("New default adapter is powered, so invalidating all the default-adapter* properties");
		g_object_notify (G_OBJECT (client), "default-adapter");
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
		g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
		g_object_notify (G_OBJECT (client), "default-adapter-name");
		return;
	}

	/*
	 * If the adapter is turn off (Powered = False in bluetooth) object
	 * notifications will be sent only when a Powered = True signal arrives
	 * from bluetoothd
	 */
	adapter_set_powered (client, adapter, TRUE);
}

static void
adapter_notify_cb (Adapter1       *adapter,
		   GParamSpec     *pspec,
		   BluetoothClient *client)
{
	const char *property = g_param_spec_get_name (pspec);
	GtkTreeIter iter;
	gboolean notify = TRUE;
	gboolean is_default;

	if (get_iter_from_proxy (client->store, &iter, G_DBUS_PROXY (adapter)) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL(client->store), &iter,
			    BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

	g_debug ("Property '%s' changed on %sadapter '%s'", property,
		 is_default ? "default " : "",
		 g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)));

	if (g_strcmp0 (property, "alias") == 0) {
		const gchar *alias = adapter1_get_alias (adapter);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_ALIAS, alias, -1);

		if (is_default) {
			g_object_notify (G_OBJECT (client), "default-adapter-powered");
			g_object_notify (G_OBJECT (client), "default-adapter-name");
		}
	} else if (g_strcmp0 (property, "discovering") == 0) {
		gboolean discovering = adapter1_get_discovering (adapter);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);

		if (is_default)
			g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
	} else if (g_strcmp0 (property, "powered") == 0) {
		gboolean powered = adapter1_get_powered (adapter);

		gtk_tree_store_set (client->store, &iter,
				    BLUETOOTH_COLUMN_POWERED, powered, -1);

		if (is_default && powered) {
			g_debug ("Default adapter is powered, so invalidating all the default-adapter* properties");
			g_object_notify (G_OBJECT (client), "default-adapter");
			g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
			g_object_notify (G_OBJECT (client), "default-adapter-name");
		}
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
	} else {
		notify = FALSE;
	}

	if (notify != FALSE) {
		GtkTreePath *path;

		/* Tell the world */
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (client->store), &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (client->store), path, &iter);
		gtk_tree_path_free (path);
	}
}

static void
adapter_added (GDBusObjectManager   *manager,
	       Adapter1             *adapter,
	       BluetoothClient      *client)
{
	GtkTreeIter iter;
	const gchar *address, *name, *alias;
	gboolean discovering, powered;

	g_signal_connect_object (G_OBJECT (adapter), "notify",
				 G_CALLBACK (adapter_notify_cb), client, 0);

	address = adapter1_get_address (adapter);
	name = adapter1_get_name (adapter);
	alias = adapter1_get_alias (adapter);
	discovering = adapter1_get_discovering (adapter);
	powered = adapter1_get_powered (adapter);

	g_debug ("Inserting adapter '%s'", address);

	gtk_tree_store_insert_with_values(client->store, &iter, NULL, -1,
					  BLUETOOTH_COLUMN_PROXY, adapter,
					  BLUETOOTH_COLUMN_ADDRESS, address,
					  BLUETOOTH_COLUMN_NAME, name,
					  BLUETOOTH_COLUMN_ALIAS, alias,
					  BLUETOOTH_COLUMN_DISCOVERING, discovering,
					  BLUETOOTH_COLUMN_POWERED, powered,
					  -1);

	if (!client->default_adapter) {
		default_adapter_changed (manager,
					 G_DBUS_PROXY (adapter),
					 client);
	}

	client->num_adapters++;
	g_object_notify (G_OBJECT (client), "num-adapters");
}

static void
adapter_removed (GDBusObjectManager   *manager,
		 const char           *path,
		 BluetoothClient      *client)
{
	GtkTreeIter iter, childiter;
	g_autoptr(GDBusProxy) new_default_adapter = NULL;
	GList *object_list, *l;
	gboolean was_default = FALSE;
	gboolean have_child;

	if (g_strcmp0 (path, g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter))) == 0)
		was_default = TRUE;

	g_debug ("Removing adapter '%s'", path);

	if (!was_default)
		goto out;

	if (get_iter_from_path (client->store, &iter, path) == FALSE)
		return;

	/* Ensure that all devices are removed. This can happen if bluetoothd
	 * crashes as the "object-removed" signal is emitted in an undefined
	 * order. */
	have_child = gtk_tree_model_iter_children (GTK_TREE_MODEL (client->store), &childiter, &iter);
	while (have_child) {
		GDBusProxy *object;

		gtk_tree_model_get (GTK_TREE_MODEL(client->store), &childiter,
				    BLUETOOTH_COLUMN_PROXY, &object, -1);

		g_signal_emit (G_OBJECT (client), signals[DEVICE_REMOVED], 0, g_dbus_proxy_get_object_path (object));
		g_object_unref (object);

		have_child = gtk_tree_store_remove (client->store, &childiter);
	}

	g_clear_object (&client->default_adapter);
	gtk_tree_store_remove (client->store, &iter);

	new_default_adapter = NULL;
	object_list = g_dbus_object_manager_get_objects (client->manager);
	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_ADAPTER_INTERFACE);
		if (iface) {
			new_default_adapter = G_DBUS_PROXY (g_object_ref (iface));
			break;
		}
	}
	g_list_free_full (object_list, g_object_unref);

	if (new_default_adapter) {
		default_adapter_changed (manager, new_default_adapter, client);
	} else {
		g_object_notify (G_OBJECT (client), "default-adapter");
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
		g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
	}

out:
	client->num_adapters--;
	g_object_notify (G_OBJECT (client), "num-adapters");
}

static GType
object_manager_get_proxy_type_func (GDBusObjectManagerClient *manager,
				    const gchar              *object_path,
				    const gchar              *interface_name,
				    gpointer                  user_data)
{
	if (interface_name == NULL)
		return G_TYPE_DBUS_OBJECT_PROXY;

	if (g_str_equal (interface_name, BLUEZ_DEVICE_INTERFACE))
		return TYPE_DEVICE1_PROXY;
	if (g_str_equal (interface_name, BLUEZ_ADAPTER_INTERFACE))
		return TYPE_ADAPTER1_PROXY;

	return G_TYPE_DBUS_PROXY;
}

static void
interface_added (GDBusObjectManager *manager,
		 GDBusObject        *object,
		 GDBusInterface     *interface,
		 gpointer            user_data)
{
	BluetoothClient *client = user_data;

	if (IS_ADAPTER1 (interface)) {
		adapter_added (manager,
			       ADAPTER1 (interface),
			       client);
	} else if (IS_DEVICE1 (interface)) {
		device_added (manager,
			      DEVICE1 (interface),
			      client,
			      FALSE);
	}
}

static void
interface_removed (GDBusObjectManager *manager,
		   GDBusObject        *object,
		   GDBusInterface     *interface,
		   gpointer            user_data)
{
	BluetoothClient *client = user_data;

	if (IS_ADAPTER1 (interface)) {
		adapter_removed (manager,
				 g_dbus_object_get_object_path (object),
				 client);
	} else if (IS_DEVICE1 (interface)) {
		device_removed (g_dbus_object_get_object_path (object),
				client);
	}
}

static void
object_added (GDBusObjectManager *manager,
	      GDBusObject        *object,
	      BluetoothClient    *client)
{
	GList *interfaces, *l;

	interfaces = g_dbus_object_get_interfaces (object);

	for (l = interfaces; l != NULL; l = l->next)
		interface_added (manager, object, G_DBUS_INTERFACE (l->data), client);

	g_list_free_full (interfaces, g_object_unref);
}

static void
object_removed (GDBusObjectManager *manager,
	        GDBusObject        *object,
	        BluetoothClient    *client)
{
	GList *interfaces, *l;

	interfaces = g_dbus_object_get_interfaces (object);

	for (l = interfaces; l != NULL; l = l->next)
		interface_removed (manager, object, G_DBUS_INTERFACE (l->data), client);

	g_list_free_full (interfaces, g_object_unref);
}

static void
object_manager_new_callback(GObject      *source_object,
			    GAsyncResult *res,
			    void         *user_data)
{
	BluetoothClient *client;
	GDBusObjectManager *manager;
	GList *object_list, *l;
	GError *error = NULL;

	manager = g_dbus_object_manager_client_new_for_bus_finish (res, &error);
	if (!manager) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Could not create bluez object manager: %s", error->message);
		g_error_free (error);
		return;
	}

	client = user_data;
	client->manager = manager;

	g_signal_connect_object (G_OBJECT (client->manager), "interface-added", (GCallback) interface_added, client, 0);
	g_signal_connect_object (G_OBJECT (client->manager), "interface-removed", (GCallback) interface_removed, client, 0);

	g_signal_connect_object (G_OBJECT (client->manager), "object-added", (GCallback) object_added, client, 0);
	g_signal_connect_object (G_OBJECT (client->manager), "object-removed", (GCallback) object_removed, client, 0);

	/* NOTE: Since 2013, in 68852faa5a957d14480e72e8e63bc8e1196d19cf, the default
	 * adapter has been dependent on the order objects are returned from
	 * g_dbus_object_manager_client_get_objects(), which has never not been
	 * tested to have the highest numbered device as the default one.
	 *
	 * It's 2021, and there are no reasons for that to be changed this late. */
	object_list = g_dbus_object_manager_get_objects (client->manager);

	/* We need to add the adapters first, otherwise the devices will
	 * be dropped to the floor, as they wouldn't have a parent in
	 * the treestore */
	g_debug ("Adding adapters from ObjectManager");
	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_ADAPTER_INTERFACE);
		if (!iface)
			continue;

		adapter_added (client->manager,
			       ADAPTER1 (iface),
			       client);
	}

	g_debug ("Adding devices from ObjectManager");
	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_DEVICE_INTERFACE);
		if (!iface)
			continue;

		device_added (client->manager,
			      DEVICE1 (iface),
			      client,
			      TRUE);
	}
	g_list_free_full (object_list, g_object_unref);
}

static void bluetooth_client_init(BluetoothClient *client)
{
	client->cancellable = g_cancellable_new ();
	client->store = gtk_tree_store_new(_BLUETOOTH_NUM_COLUMNS,
					 G_TYPE_OBJECT,     /* BLUETOOTH_COLUMN_PROXY */
					 G_TYPE_OBJECT,     /* BLUETOOTH_COLUMN_PROPERTIES */
					 G_TYPE_STRING,     /* BLUETOOTH_COLUMN_ADDRESS */
					 G_TYPE_STRING,     /* BLUETOOTH_COLUMN_ALIAS */
					 G_TYPE_STRING,     /* BLUETOOTH_COLUMN_NAME */
					 G_TYPE_UINT,       /* BLUETOOTH_COLUMN_TYPE */
					 G_TYPE_STRING,     /* BLUETOOTH_COLUMN_ICON */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_DEFAULT */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_PAIRED */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_TRUSTED */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_CONNECTED */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_DISCOVERING */
					 G_TYPE_INT,        /* BLUETOOTH_COLUMN_LEGACYPAIRING */
					 G_TYPE_BOOLEAN,    /* BLUETOOTH_COLUMN_POWERED */
					 G_TYPE_STRV);      /* BLUETOOTH_COLUMN_UUIDS */
	client->list_store = g_list_store_new (BLUETOOTH_TYPE_DEVICE);

	g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
						  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  BLUEZ_MANAGER_PATH,
						  object_manager_get_proxy_type_func,
						  NULL, NULL,
						  client->cancellable,
						  object_manager_new_callback, client);
}

GDBusProxy *
_bluetooth_client_get_default_adapter(BluetoothClient *client)
{
	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	if (client->default_adapter == NULL)
		return NULL;

	return G_DBUS_PROXY (g_object_ref (client->default_adapter));
}

static void
_bluetooth_client_set_default_adapter_discovering (BluetoothClient *client,
						   gboolean         discovering)
{
	g_autoptr(GDBusProxy) adapter = NULL;
	GVariantBuilder builder;

	adapter = _bluetooth_client_get_default_adapter (client);
	if (adapter == NULL)
		return;

	if (discovering) {
		g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add (&builder, "{sv}",
				       "Discoverable", g_variant_new_boolean (TRUE));
		adapter1_call_set_discovery_filter_sync (ADAPTER1 (adapter),
							 g_variant_builder_end (&builder), NULL, NULL);
	}

	client->discovery_started = discovering;
	if (discovering)
		adapter1_call_start_discovery (ADAPTER1 (adapter), NULL, NULL, NULL);
	else
		adapter1_call_stop_discovery (ADAPTER1 (adapter), NULL, NULL, NULL);
}

static void
bluetooth_client_get_property (GObject        *object,
			       guint           property_id,
			       GValue         *value,
			       GParamSpec     *pspec)
{
	BluetoothClient *client = BLUETOOTH_CLIENT (object);

	switch (property_id) {
	case PROP_NUM_ADAPTERS:
		g_value_set_uint (value, client->num_adapters);
		break;
	case PROP_DEFAULT_ADAPTER:
		g_value_set_string (value, client->default_adapter ?
				    g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter)) : NULL);
		break;
	case PROP_DEFAULT_ADAPTER_POWERED:
		g_value_set_boolean (value, client->default_adapter ?
				     adapter1_get_powered (client->default_adapter) : FALSE);
		break;
	case PROP_DEFAULT_ADAPTER_NAME:
		g_value_set_string (value, client->default_adapter ?
				    adapter1_get_alias (client->default_adapter) : NULL);
		break;
	case PROP_DEFAULT_ADAPTER_SETUP_MODE:
		g_value_set_boolean (value, client->default_adapter ?
				     adapter1_get_discovering (client->default_adapter) : FALSE);
		break;
	case PROP_DEFAULT_ADAPTER_ADDRESS:
		g_value_set_string (value, client->default_adapter ?
				    adapter1_get_address (client->default_adapter) : NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
bluetooth_client_set_property (GObject        *object,
			       guint           property_id,
			       const GValue   *value,
			       GParamSpec     *pspec)
{
	BluetoothClient *client = BLUETOOTH_CLIENT (object);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER_SETUP_MODE:
		_bluetooth_client_set_default_adapter_discovering (client, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void bluetooth_client_finalize(GObject *object)
{
	BluetoothClient *client = BLUETOOTH_CLIENT (object);

	if (client->cancellable != NULL) {
		g_cancellable_cancel (client->cancellable);
		g_clear_object (&client->cancellable);
	}
	g_clear_object (&client->manager);
	g_object_unref (client->store);
	g_object_unref (client->list_store);

	g_clear_object (&client->default_adapter);

	G_OBJECT_CLASS(bluetooth_client_parent_class)->finalize (object);
}

static void bluetooth_client_class_init(BluetoothClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = bluetooth_client_finalize;
	object_class->get_property = bluetooth_client_get_property;
	object_class->set_property = bluetooth_client_set_property;

	/**
	 * BluetoothClient::device-added:
	 * @client: a #BluetoothClient object which received the signal
	 * @device: a #BluetoothDevice object
	 *
	 * The #BluetoothClient::device-added signal is launched when a
	 * device gets added to the model.
	 **/
	signals[DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	/**
	 * BluetoothClient::device-removed:
	 * @client: a #BluetoothClient object which received the signal
	 * @device: the D-Bus object path for the now-removed device
	 *
	 * The #BluetoothClient::device-removed signal is launched when a
	 * device gets removed from the model.
	 **/
	signals[DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * BluetoothClient:num-adapters:
	 *
	 * The number of detected Bluetooth adapters.
	 */
	g_object_class_install_property (object_class, PROP_NUM_ADAPTERS,
					 g_param_spec_uint ("num-adapters", NULL,
							    "The number of detected Bluetooth adapters",
							    0, G_MAXUINT, 0, G_PARAM_READABLE));

	/**
	 * BluetoothClient:default-adapter:
	 *
	 * The D-Bus path of the default Bluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER,
					 g_param_spec_string ("default-adapter", NULL,
							      "The D-Bus path of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-powered:
	 *
	 * %TRUE if the default Bluetooth adapter is powered.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_POWERED,
					 g_param_spec_boolean ("default-adapter-powered", NULL,
							      "Whether the default adapter is powered",
							       FALSE, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-setup-mode:
	 *
	 * %TRUE if the default Bluetooth adapter is in setup mode (discoverable, and discovering).
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_SETUP_MODE,
					 g_param_spec_boolean ("default-adapter-setup-mode", NULL,
							      "Whether the default adapter is visible to others and scanning",
							       FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothClient:default-adapter-name:
	 *
	 * The name of the default Bluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_NAME,
					 g_param_spec_string ("default-adapter-name", NULL,
							      "The human readable name of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-address:
	 *
	 * The address of the default Bluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_ADDRESS,
					 g_param_spec_string ("default-adapter-address", NULL,
							      "The address of the default adapter",
							      NULL, G_PARAM_READABLE));
}

/**
 * bluetooth_client_new:
 *
 * Returns a reference to the #BluetoothClient singleton. Use g_object_unref() when done with the object.
 *
 * Return value: (transfer full): a #BluetoothClient object.
 **/
BluetoothClient *bluetooth_client_new(void)
{
	static BluetoothClient *bluetooth_client = NULL;

	if (bluetooth_client != NULL)
		return g_object_ref(bluetooth_client);

	bluetooth_client = BLUETOOTH_CLIENT (g_object_new (BLUETOOTH_TYPE_CLIENT, NULL));
	g_object_add_weak_pointer (G_OBJECT (bluetooth_client),
				   (gpointer) &bluetooth_client);

	return bluetooth_client;
}

/**
 * bluetooth_client_get_devices:
 * @client: a #BluetoothClient object
 *
 * Returns an unfiltered #GListStore representing the devices attached to the default
 * Bluetooth adapter.
 *
 * Return value: (transfer full): a #GListStore
 **/
GListStore *
bluetooth_client_get_devices (BluetoothClient *client)
{
	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	return G_LIST_STORE (g_object_ref (client->list_store));
}

typedef struct {
	BluetoothClientSetupFunc func;
	BluetoothClient *client;
} CreateDeviceData;

static void
device_pair_callback (GDBusProxy   *proxy,
		      GAsyncResult *res,
		      GTask        *task)
{
	GError *error = NULL;

	if (device1_call_pair_finish (DEVICE1(proxy), res, &error) == FALSE) {
		g_debug ("Pair() failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy),
			 error->message);
		g_task_return_error (task, error);
	} else {
		g_debug ("Pair() success for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_task_return_boolean (task, TRUE);
	}
	g_object_unref (task);
}

/**
 * bluetooth_client_setup_device_finish:
 * @client:
 * @res:
 * @path: (out):
 * @error:
 */
gboolean
bluetooth_client_setup_device_finish (BluetoothClient  *client,
				      GAsyncResult     *res,
				      char            **path,
				      GError          **error)
{
	GTask *task;
	char *object_path;
	gboolean ret;

	g_return_val_if_fail (path != NULL, FALSE);

	task = G_TASK (res);

	g_warn_if_fail (g_task_get_source_tag (task) == bluetooth_client_setup_device);

	ret = g_task_propagate_boolean (task, error);
	object_path = g_strdup (g_task_get_task_data (task));
	*path = object_path;
	g_debug ("bluetooth_client_setup_device_finish() %s (path: %s)",
		 ret ? "success" : "failure", object_path);
	return ret;
}

void
bluetooth_client_setup_device (BluetoothClient          *client,
			       const char               *path,
			       gboolean                  pair,
			       GCancellable             *cancellable,
			       GAsyncReadyCallback       callback,
			       gpointer                  user_data)
{
	GTask *task;
	g_autoptr(BluetoothDevice) device = NULL;
	g_autoptr(GDBusProxy) proxy = NULL;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (path != NULL);

	task = g_task_new (G_OBJECT (client),
			   cancellable,
			   callback,
			   user_data);
	g_task_set_source_tag (task, bluetooth_client_setup_device);
	g_task_set_task_data (task, g_strdup (path), (GDestroyNotify) g_free);

	device = get_device_for_path (client, path);
	if (!device) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
					 "Device with object path %s does not exist",
					 path);
		g_object_unref (task);
		return;
	}

	g_object_get (device,
		      "proxy", &proxy,
		      NULL);

	if (pair == TRUE) {
		device1_call_pair (DEVICE1(proxy),
				   cancellable,
				   (GAsyncReadyCallback) device_pair_callback,
				   task);
	} else {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
	}
}

/**
 * bluetooth_client_cancel_setup_device_finish:
 * @client:
 * @res:
 * @path: (out):
 * @error:
 */
gboolean
bluetooth_client_cancel_setup_device_finish (BluetoothClient  *client,
					     GAsyncResult     *res,
					     char            **path,
					     GError          **error)
{
	GTask *task;
	char *object_path;
	gboolean ret;

	g_return_val_if_fail (path != NULL, FALSE);

	task = G_TASK (res);

	g_warn_if_fail (g_task_get_source_tag (task) == bluetooth_client_cancel_setup_device);

	ret = g_task_propagate_boolean (task, error);
	object_path = g_strdup (g_task_get_task_data (task));
	*path = object_path;
	g_debug ("bluetooth_client_cancel_setup_device_finish() %s (path: %s)",
		 ret ? "success" : "failure", object_path);
	return ret;
}

static void
device_cancel_pairing_callback (GDBusProxy   *proxy,
				GAsyncResult *res,
				GTask        *task)
{
	GError *error = NULL;

	if (device1_call_cancel_pairing_finish (DEVICE1(proxy), res, &error) == FALSE) {
		g_debug ("CancelPairing() failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy),
			 error->message);
		g_task_return_error (task, error);
	} else {
		g_task_return_boolean (task, TRUE);
	}
	g_object_unref (task);
}

void
bluetooth_client_cancel_setup_device (BluetoothClient          *client,
				      const char               *path,
				      GCancellable             *cancellable,
				      GAsyncReadyCallback       callback,
				      gpointer                  user_data)
{
	GTask *task;
	g_autoptr(BluetoothDevice) device = NULL;
	g_autoptr(GDBusProxy) proxy = NULL;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (path != NULL);

	task = g_task_new (G_OBJECT (client),
			   cancellable,
			   callback,
			   user_data);
	g_task_set_source_tag (task, bluetooth_client_cancel_setup_device);
	g_task_set_task_data (task, g_strdup (path), (GDestroyNotify) g_free);

	device = get_device_for_path (client, path);
	if (device == NULL) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
					 "Device with object path %s does not exist",
					 path);
		g_object_unref (task);
		return;
	}

	g_object_get (G_OBJECT (device),
		      "proxy", &proxy,
		      NULL);

	device1_call_cancel_pairing (DEVICE1(proxy),
				     cancellable,
				     (GAsyncReadyCallback) device_cancel_pairing_callback,
				     task);
}

gboolean
bluetooth_client_set_trusted (BluetoothClient *client,
			      const char      *device_path,
			      gboolean         trusted)
{
	GObject *device;
	GtkTreeIter iter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device_path != NULL, FALSE);

	if (get_iter_from_path (client->store, &iter, device_path) == FALSE) {
		g_debug ("Couldn't find device '%s' in tree to mark it as trusted", device_path);
		return FALSE;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (client->store), &iter,
			    BLUETOOTH_COLUMN_PROXY, &device, -1);

	if (device == NULL)
		return FALSE;

	g_object_set (device, "trusted", trusted, NULL);
	g_object_unref (device);

	return TRUE;
}

static void
connect_callback (GDBusProxy   *proxy,
		  GAsyncResult *res,
		  GTask        *task)
{
	gboolean retval;
	GError *error = NULL;

	retval = device1_call_connect_finish (DEVICE1 (proxy), res, &error);
	if (retval == FALSE) {
		g_debug ("Connect failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy), error->message);
		g_task_return_error (task, error);
	} else {
		g_debug ("Connect succeeded for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_task_return_boolean (task, retval);
	}

	g_object_unref (task);
}

static void
disconnect_callback (GDBusProxy   *proxy,
		     GAsyncResult *res,
		     GTask        *task)
{
	gboolean retval;
	GError *error = NULL;

	retval = device1_call_disconnect_finish (DEVICE1 (proxy), res, &error);
	if (retval == FALSE) {
		g_debug ("Disconnect failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy),
			 error->message);
		g_task_return_error (task, error);
	} else {
		g_debug ("Disconnect succeeded for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_task_return_boolean (task, retval);
	}

	g_object_unref (task);
}

/**
 * bluetooth_client_connect_service:
 * @client: a #BluetoothClient
 * @path: the object path on which to operate
 * @connect: Whether try to connect or disconnect from services on a device
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the connection is complete
 * @user_data: the data to pass to callback function
 *
 * When the connection operation is finished, @callback will be called. You can
 * then call bluetooth_client_connect_service_finish() to get the result of the
 * operation.
 **/
void
bluetooth_client_connect_service (BluetoothClient     *client,
				  const char          *path,
				  gboolean             connect,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
	GtkTreeIter iter;
	GTask *task;
	g_autoptr(GDBusProxy) device = NULL;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (path != NULL);

	task = g_task_new (G_OBJECT (client),
			   cancellable,
			   callback,
			   user_data);
	g_task_set_source_tag (task, bluetooth_client_connect_service);

	if (get_iter_from_path (client->store, &iter, path) == FALSE) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
					 "Device with object path %s does not exist",
					 path);
		g_object_unref (task);
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL(client->store), &iter,
			    BLUETOOTH_COLUMN_PROXY, &device,
			    -1);

	if (connect) {
		device1_call_connect (DEVICE1(device),
				      cancellable,
				      (GAsyncReadyCallback) connect_callback,
				      task);
	} else {
		device1_call_disconnect (DEVICE1(device),
					 cancellable,
					 (GAsyncReadyCallback) disconnect_callback,
					 task);
	}
}

/**
 * bluetooth_client_connect_service_finish:
 * @client: a #BluetoothClient
 * @res: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes the connection operation. See bluetooth_client_connect_service().
 *
 * Returns: %TRUE if the connection operation succeeded, %FALSE otherwise.
 **/
gboolean
bluetooth_client_connect_service_finish (BluetoothClient *client,
					 GAsyncResult    *res,
					 GError         **error)
{
	GTask *task;

	task = G_TASK (res);

	g_warn_if_fail (g_task_get_source_tag (task) == bluetooth_client_connect_service);

	return g_task_propagate_boolean (task, error);
}
