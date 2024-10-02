/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
 *  Copyright (C) 2013       Intel Corporation.
 *  Copyright (C) 2009-2021  Red Hat Inc.
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

#define _GNU_SOURCE
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <libupower-glib/upower.h>

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
	gboolean has_power_state;
	GDBusObjectManager *manager;
	GCancellable *cancellable;
	guint num_adapters;
	/* Prevent concurrent enabling or disabling discoveries */
	gboolean discovery_started;
	UpClient *up_client;
	gboolean bluez_devices_coldplugged;
	GList *removed_devices_queue;
	guint removed_devices_queue_id;
};

enum {
	PROP_0,
	PROP_NUM_ADAPTERS,
	PROP_DEFAULT_ADAPTER,
	PROP_DEFAULT_ADAPTER_POWERED,
	PROP_DEFAULT_ADAPTER_STATE,
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

G_DEFINE_TYPE(BluetoothClient, bluetooth_client, G_TYPE_OBJECT)

#define DEVICE_REMOVAL_TIMEOUT_MSECS 50

static void up_client_coldplug (BluetoothClient *client);

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

static BluetoothDevice *
get_device_for_bdaddr (BluetoothClient *client,
		       const char      *bdaddr)
{
	guint n_items, i;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
	for (i = 0; i < n_items; i++) {
		g_autoptr(BluetoothDevice) d = NULL;
		g_autofree char *s = NULL;

		d = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
		g_object_get (G_OBJECT (d), "address", &s, NULL);
		if (g_ascii_strncasecmp (bdaddr, s, BDADDR_STR_LEN) == 0) {
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
icon_override (const char    *name,
	       const char    *bdaddr,
	       BluetoothType *type)
{
	/* audio-card, you're ugly */
	switch (*type) {
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
	case BLUETOOTH_TYPE_MOUSE:
		if (name && strcasestr (name, "tablet")) {
			*type = BLUETOOTH_TYPE_TABLET;
			return "input-tablet";
		}
		/* fallthrough */
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
	const char *name;

	g_return_if_fail (type);
	g_return_if_fail (icon);

	name = device1_get_name (device);
	if (g_strcmp0 (name, "ION iCade Game Controller") == 0 ||
	    g_strcmp0 (name, "8Bitdo Zero GamePad") == 0) {
		*type = BLUETOOTH_TYPE_JOYPAD;
		*icon = "input-gaming";
		return;
	}

	if (*type == 0 || *type == BLUETOOTH_TYPE_ANY)
		*type = bluetooth_appearance_to_type (device1_get_appearance (device));
	if (*type == 0 || *type == BLUETOOTH_TYPE_ANY)
		*type = bluetooth_class_to_type (device1_get_class (device));

	*icon = icon_override (name, device1_get_address (device), type);

	if (!*icon) {
		*icon = device1_get_icon (device);
		if (*icon && *icon[0] == '\0')
			*icon = NULL;
	}

	if (!*icon)
		*icon = "bluetooth";
}

static void
device_notify_cb (Device1         *device1,
		  GParamSpec      *pspec,
		  BluetoothClient *client)
{
	const char *property = g_param_spec_get_name (pspec);
	g_autoptr(BluetoothDevice) device = NULL;
	const char *device_path;

	device_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (device1));
	device = get_device_for_path (client, device_path);
	if (!device) {
		g_debug ("Device %s was not known, so property '%s' not applied", device_path, property);
		return;
	}

	g_debug ("Property '%s' changed on device '%s'", property, device_path);

	if (g_strcmp0 (property, "name") == 0) {
		const gchar *name = device1_get_name (device1);

		g_object_set (G_OBJECT (device), "name", name, NULL);
	} else if (g_strcmp0 (property, "alias") == 0) {
		const gchar *alias = device1_get_alias (device1);

		g_object_set (G_OBJECT (device), "alias", alias, NULL);
	} else if (g_strcmp0 (property, "paired") == 0) {
		gboolean paired = device1_get_paired (device1);

		g_object_set (G_OBJECT (device), "paired", paired, NULL);
	} else if (g_strcmp0 (property, "trusted") == 0) {
		gboolean trusted = device1_get_trusted (device1);

		g_object_set (G_OBJECT (device), "trusted", trusted, NULL);
	} else if (g_strcmp0 (property, "connected") == 0) {
		gboolean connected = device1_get_connected (device1);

		g_object_set (G_OBJECT (device), "connected", connected, NULL);
	} else if (g_strcmp0 (property, "uuids") == 0) {
		g_auto(GStrv) uuids = NULL;

		uuids = device_list_uuids (device1_get_uuids (device1));

		g_object_set (G_OBJECT (device), "uuids", uuids, NULL);
	} else if (g_strcmp0 (property, "legacy-pairing") == 0) {
		gboolean legacypairing = device1_get_legacy_pairing (device1);

		g_object_set (G_OBJECT (device), "legacy-pairing", legacypairing, NULL);
	} else if (g_strcmp0 (property, "icon") == 0 ||
		   g_strcmp0 (property, "class") == 0 ||
		   g_strcmp0 (property, "appearance") == 0) {
		BluetoothType type = BLUETOOTH_TYPE_ANY;
		const char *icon = NULL;

		device_resolve_type_and_icon (device1, &type, &icon);

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
	      BluetoothClient      *client)
{
	g_autoptr (GDBusProxy) adapter = NULL;
	BluetoothDevice *device_obj;
	const char *default_adapter_path;
	const char *device_path;
	const char *adapter_path, *address, *alias, *name, *icon;
	g_auto(GStrv) uuids = NULL;
	gboolean paired, trusted, connected;
	int legacypairing;
	BluetoothType type = BLUETOOTH_TYPE_ANY;

	adapter_path = device1_get_adapter (device);
	default_adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter));
	if (g_strcmp0 (default_adapter_path, adapter_path) != 0)
		return;

	g_signal_connect_object (G_OBJECT (device), "notify",
				 G_CALLBACK (device_notify_cb), client, 0);

	device_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (device));
	device_obj = get_device_for_path (client, device_path);
	if (device_obj) {
		g_debug ("Updating proxy for device '%s'", device_path);
		g_object_set (G_OBJECT (device_obj),
			      "proxy", device,
			       NULL);
		return;
	}

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

static gboolean
unqueue_device_removal (BluetoothClient *client)
{
	GList *l;

	if (!client->removed_devices_queue)
		return G_SOURCE_REMOVE;

	for (l = client->removed_devices_queue; l != NULL; l = l->next) {
		char *path = l->data;
		guint i, n_items;
		gboolean found = FALSE;

		g_debug ("Removing '%s' from queue of removed devices", path);

		n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
		for (i = 0; i < n_items; i++) {
			g_autoptr(BluetoothDevice) device = NULL;

			device = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
			if (g_str_equal (path, bluetooth_device_get_object_path (device))) {
				g_list_store_remove (client->list_store, i);
				g_signal_emit (G_OBJECT (client), signals[DEVICE_REMOVED], 0, path);
				found = TRUE;
				break;
			}
		}
		if (!found)
			g_debug ("Device %s was not known, so not removed", path);
		g_free (path);
	}
	g_clear_pointer (&client->removed_devices_queue, g_list_free);

	client->removed_devices_queue_id = 0;
	return G_SOURCE_REMOVE;
}

static void
queue_device_removal (BluetoothClient *client,
		      const char      *path)
{
	client->removed_devices_queue = g_list_prepend (client->removed_devices_queue,
							g_strdup (path));
	g_clear_handle_id (&client->removed_devices_queue_id, g_source_remove);
	client->removed_devices_queue_id = g_timeout_add (DEVICE_REMOVAL_TIMEOUT_MSECS,
							  (GSourceFunc) unqueue_device_removal, client);
}

static void
device_removed (const char      *path,
		BluetoothClient *client)
{
	g_debug ("Device '%s' was removed", path);
	queue_device_removal (client, path);
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
		g_debug ("Error setting property 'Powered' on %s: %s (%s, %d)",
			 g_dbus_proxy_get_object_path (proxy),
			 error->message, g_quark_to_string (error->domain), error->code);
	}
}

static void
adapter_set_powered (BluetoothClient *client,
		     gboolean         powered)
{
	GVariant *variant;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));

	if (!client->default_adapter) {
		g_debug ("No default adapter to power");
		return;
	}

	if (powered == adapter1_get_powered (client->default_adapter)) {
		g_debug ("Default adapter is already %spowered", powered ? "" : "un");
		return;
	}

	g_debug ("Powering %s default adapter %s",
		 powered ? "up" : "down",
		 g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter)));
	variant = g_variant_new_boolean (powered);
	g_dbus_proxy_call (G_DBUS_PROXY (client->default_adapter),
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
	gboolean coldplug_upower;

	coldplug_upower = !client->bluez_devices_coldplugged && client->up_client;

	g_debug ("Emptying list store as default adapter changed");
	g_list_store_remove_all (client->list_store);

	default_adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter));

	g_debug ("Coldplugging devices for new default adapter");
	client->bluez_devices_coldplugged = TRUE;
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

		g_signal_connect_object (G_OBJECT (DEVICE1 (iface)), "notify",
					 G_CALLBACK (device_notify_cb), client, 0);

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

	if (coldplug_upower)
		up_client_coldplug (client);
}

static void
adapter_notify_cb (Adapter1       *adapter,
		   GParamSpec     *pspec,
		   BluetoothClient *client)
{
	const char *property = g_param_spec_get_name (pspec);
	const char *adapter_path;
	const char *default_adapter_path;

	adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter));
	if (client->default_adapter == NULL) {
		g_debug ("Property '%s' changed on adapter '%s', but default adapter not set yet",
			 property, adapter_path);
		return;
	}

	default_adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter));
	if (g_strcmp0 (default_adapter_path, adapter_path) != 0) {
		g_debug ("Ignoring property '%s' change on non-default adapter %s",
			   property, adapter_path);
		return;
	}

	g_debug ("Property '%s' changed on default adapter '%s'", property, adapter_path);

	if (g_strcmp0 (property, "alias") == 0) {
		g_object_notify (G_OBJECT (client), "default-adapter-name");
	} else if (g_strcmp0 (property, "discovering") == 0) {
		g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
	} else if (g_strcmp0 (property, "powered") == 0) {
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
		if (!client->has_power_state)
			g_object_notify (G_OBJECT (client), "default-adapter-state");
	} else if (g_strcmp0 (property, "power-state") == 0) {
		g_object_notify (G_OBJECT (client), "default-adapter-state");
	}
}

typedef enum {
	OWNER_UPDATE,
	REPLACEMENT,
	NEW_DEFAULT,
	REMOVAL
} DefaultAdapterChangeType;

static void
notify_default_adapter_props (BluetoothClient *client)
{
	g_object_notify (G_OBJECT (client), "default-adapter");
	g_object_notify (G_OBJECT (client), "default-adapter-address");
	g_object_notify (G_OBJECT (client), "default-adapter-powered");
	g_object_notify (G_OBJECT (client), "default-adapter-state");
	g_object_notify (G_OBJECT (client), "default-adapter-setup-mode");
	g_object_notify (G_OBJECT (client), "default-adapter-name");
}

static void
_bluetooth_client_set_default_adapter_discovering (BluetoothClient *client,
						   gboolean         discovering);

static void
default_adapter_changed (GDBusObjectManager       *manager,
			 GDBusProxy               *adapter,
			 DefaultAdapterChangeType  change_type,
			 BluetoothClient          *client)
{
	if (change_type == REMOVAL) {
		g_clear_object (&client->default_adapter);
		g_debug ("Emptying list store as default adapter removed");
		g_list_store_remove_all (client->list_store);
		g_debug ("No default adapter so invalidating all the default-adapter* properties");
		notify_default_adapter_props (client);
		return;
	} else if (change_type == NEW_DEFAULT) {
		g_clear_object (&client->default_adapter);
		g_debug ("Setting '%s' as the new default adapter", g_dbus_proxy_get_object_path (adapter));
	} else if (change_type == OWNER_UPDATE) {
		g_clear_object (&client->default_adapter);
		g_debug ("Updating default adapter proxy '%s' for new owner",
			 g_dbus_proxy_get_object_path (adapter));
	} else if (change_type == REPLACEMENT) {
		g_debug ("Emptying list store as old default adapter removed");
		g_list_store_remove_all (client->list_store);
		g_debug ("Disabling discovery on old default adapter");
		_bluetooth_client_set_default_adapter_discovering (client, FALSE);
		g_clear_object (&client->default_adapter);
	}

	client->default_adapter = ADAPTER1 (g_object_ref (G_OBJECT (adapter)));
	g_signal_connect_object (G_OBJECT (adapter), "notify",
				 G_CALLBACK (adapter_notify_cb), client, 0);

	if (change_type == OWNER_UPDATE)
		return;

	add_devices_to_list_store (client);

	g_debug ("New default adapter so invalidating all the default-adapter* properties");
	notify_default_adapter_props (client);
}

static gboolean
is_default_adapter (BluetoothClient *client,
		    Adapter1        *adapter)
{
	const char *default_adapter_path = NULL;
	const char *adapter_path;

	g_return_val_if_fail (client->default_adapter, FALSE);
	g_return_val_if_fail (adapter, FALSE);

	adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter));
	default_adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter));
	return (g_strcmp0 (adapter_path, default_adapter_path) == 0);
}

static gboolean
should_be_default_adapter (BluetoothClient *client,
			   Adapter1        *adapter)
{
	return g_strcmp0 (g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)),
			  g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter))) > 0;
}

static void
adapter_added (GDBusObjectManager   *manager,
	       Adapter1             *adapter,
	       BluetoothClient      *client)
{
	const char *adapter_path;
	const char *iface;
	const char *name;

	name = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (adapter));
	iface = g_dbus_proxy_get_interface_name (G_DBUS_PROXY (adapter));
	adapter_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter));
	if (!client->default_adapter) {
		g_debug ("Inserting adapter %s %s %s",
			 name, adapter_path, iface);
		default_adapter_changed (manager,
					 G_DBUS_PROXY (adapter),
					 NEW_DEFAULT,
					 client);
	} else if (is_default_adapter (client, adapter)) {
		g_debug ("Updating default adapter with new proxy %s %s %s",
			 name, adapter_path, iface);
		default_adapter_changed (manager,
					 G_DBUS_PROXY (adapter),
					 OWNER_UPDATE,
					 client);
		return;
	} else if (should_be_default_adapter (client, adapter)) {
		g_debug ("Replacing default adapter %s with %s %s %s",
			 g_dbus_proxy_get_name_owner (G_DBUS_PROXY (client->default_adapter)),
			 name, adapter_path, iface);
		default_adapter_changed (manager,
					 G_DBUS_PROXY (adapter),
					 REPLACEMENT,
					 client);
	} else {
		g_debug ("Ignoring added non-default adapter %s %s %s",
			 name, adapter_path, iface);
	}

	client->num_adapters++;
	g_object_notify (G_OBJECT (client), "num-adapters");
}

static void
adapter_removed (GDBusObjectManager   *manager,
		 const char           *path,
		 BluetoothClient      *client)
{
	g_autoptr(GDBusProxy) new_default_adapter = NULL;
	GList *object_list, *l;
	gboolean was_default = FALSE;
	DefaultAdapterChangeType change_type;

	if (g_strcmp0 (path, g_dbus_proxy_get_object_path (G_DBUS_PROXY (client->default_adapter))) == 0)
		was_default = TRUE;

	g_debug ("Removing adapter '%s'", path);

	if (!was_default)
		goto out;

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

	/* Removal? */
	change_type = new_default_adapter ? NEW_DEFAULT : REMOVAL;
	if (change_type == REMOVAL) {
		/* Clear out the removed_devices queue */
		g_clear_handle_id (&client->removed_devices_queue_id, g_source_remove);
		g_list_free_full (client->removed_devices_queue, g_free);
		client->removed_devices_queue = NULL;
	}

	default_adapter_changed (manager,
				 new_default_adapter,
				 change_type,
				 client);

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
			      client);
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

static int
reverse_sort_adapters (gconstpointer a,
                       gconstpointer b)
{
	GDBusProxy *adapter_a = (GDBusProxy *) a;
	GDBusProxy *adapter_b = (GDBusProxy *) b;

	return g_strcmp0 (g_dbus_proxy_get_object_path (adapter_b),
			  g_dbus_proxy_get_object_path (adapter_a));
}

static GList *
filter_adapter_list (GList *object_list)
{
	GList *l, *out = NULL;

	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_ADAPTER_INTERFACE);
		if (iface)
			out = g_list_prepend (out, iface);
	}
	return out;
}

static void
object_manager_new_callback(GObject      *source_object,
			    GAsyncResult *res,
			    void         *user_data)
{
	BluetoothClient *client;
	GDBusObjectManager *manager;
	g_autolist(GDBusObject) object_list = NULL;
	g_autolist(GDBusProxy) adapter_list = NULL;
	g_autoptr(GError) error = NULL;
	GList *l;

	manager = g_dbus_object_manager_client_new_for_bus_finish (res, &error);
	if (!manager) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Could not create bluez object manager: %s", error->message);
		return;
	}

	client = user_data;
	client->manager = manager;

	g_signal_connect_object (G_OBJECT (client->manager), "interface-added", (GCallback) interface_added, client, 0);
	g_signal_connect_object (G_OBJECT (client->manager), "interface-removed", (GCallback) interface_removed, client, 0);

	g_signal_connect_object (G_OBJECT (client->manager), "object-added", (GCallback) object_added, client, 0);
	g_signal_connect_object (G_OBJECT (client->manager), "object-removed", (GCallback) object_removed, client, 0);

	/* We need to add the adapters first, otherwise the devices will
	 * be dropped to the floor, as they wouldn't have a default adapter */
	object_list = g_dbus_object_manager_get_objects (client->manager);
	adapter_list = filter_adapter_list (object_list);
	adapter_list = g_list_sort (adapter_list, reverse_sort_adapters);

	g_debug ("Adding adapters from ObjectManager");
	for (l = adapter_list; l != NULL; l = l->next)
		adapter_added (client->manager, l->data, client);
}

static void
device_set_up_device (BluetoothDevice *device,
		      UpDevice        *up_device)
{
	g_object_set_data_full (G_OBJECT (device), "up-device", up_device ? g_object_ref (up_device) : NULL, g_object_unref);
}

static UpDevice *
device_get_up_device (BluetoothDevice *device)
{
	return g_object_get_data (G_OBJECT (device), "up-device");
}

static void
up_device_notify_cb (GObject    *gobject,
		     GParamSpec *pspec,
		     gpointer    user_data)
{
	UpDevice *up_device = UP_DEVICE (gobject);
	BluetoothDevice *device = user_data;
	UpDeviceLevel battery_level;
	double percentage;
	BluetoothBatteryType battery_type;

	g_object_get (up_device,
		      "battery-level", &battery_level,
		      "percentage", &percentage,
		      NULL);

	if (battery_level == UP_DEVICE_LEVEL_NONE)
		battery_type = BLUETOOTH_BATTERY_TYPE_PERCENTAGE;
	else
		battery_type = BLUETOOTH_BATTERY_TYPE_COARSE;
	g_debug ("Updating battery information for %s", bluetooth_device_get_object_path (device));
	g_object_set (device,
		      "battery-type", battery_type,
		      "battery-level", battery_level,
		      "battery-percentage", percentage,
		      NULL);
}

static BluetoothDevice *
get_bluetooth_device_for_up_device (BluetoothClient *client,
				    const char      *path)
{
	guint n_items, i;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
	for (i = 0; i < n_items; i++) {
		g_autoptr(BluetoothDevice) d = NULL;
		UpDevice *up_device;

		d = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
		up_device = device_get_up_device (d);
		if (!up_device)
			continue;
		if (g_str_equal (path, up_device_get_object_path (up_device)))
			return g_steal_pointer (&d);
	}
	return NULL;
}

static void
up_device_removed_cb (UpClient   *up_client,
		      const char *object_path,
		      gpointer    user_data)
{
	BluetoothClient *client = user_data;
	BluetoothDevice *device;

	device = get_bluetooth_device_for_up_device (client, object_path);
	if (device == NULL)
		return;
	g_debug ("Removing UpDevice %s for BluetoothDevice %s",
		 object_path, bluetooth_device_get_object_path (device));
	device_set_up_device (device, NULL);
	g_object_set (device,
		      "battery-type", BLUETOOTH_BATTERY_TYPE_NONE,
		      "battery-level", UP_DEVICE_LEVEL_UNKNOWN,
		      "battery-percentage", 0.0f,
		      NULL);
}

static void
up_device_added_cb (UpClient *up_client,
		    UpDevice *up_device,
		    gpointer  user_data)
{
	BluetoothClient *client = user_data;
	g_autofree char *serial = NULL;
	g_autoptr(BluetoothDevice) device = NULL;
	UpDeviceLevel battery_level;
	double percentage;
	BluetoothBatteryType battery_type;

	g_debug ("Considering UPower device %s", up_device_get_object_path (up_device));

	g_object_get (up_device,
		      "serial", &serial,
		      "battery-level", &battery_level,
		      "percentage", &percentage,
		      NULL);

	if (!serial || !bluetooth_verify_address(serial))
		return;
	device = get_device_for_bdaddr (client, serial);
	if (!device) {
		g_debug ("Could not find bluez device for upower device with serial %s", serial);
		return;
	}
	g_signal_connect (G_OBJECT (up_device), "notify::battery-level",
			  G_CALLBACK (up_device_notify_cb), device);
	g_signal_connect (G_OBJECT (up_device), "notify::percentage",
			  G_CALLBACK (up_device_notify_cb), device);
	device_set_up_device (device, up_device);
	if (battery_level == UP_DEVICE_LEVEL_NONE)
		battery_type = BLUETOOTH_BATTERY_TYPE_PERCENTAGE;
	else
		battery_type = BLUETOOTH_BATTERY_TYPE_COARSE;
	g_debug ("Applying battery information for %s", serial);
	g_object_set (device,
		      "battery-type", battery_type,
		      "battery-level", battery_level,
		      "battery-percentage", percentage,
		      NULL);
}

static void
up_client_get_devices_cb (GObject      *source_object,
			  GAsyncResult *res,
			  gpointer      user_data)
{
	GPtrArray *devices;
	g_autoptr(GError) error = NULL;
	BluetoothClient *client;
	guint i;

	devices = up_client_get_devices_finish (UP_CLIENT (source_object), res, &error);
	if (!devices) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_debug ("Could not get UPower devices: %s", error->message);
		return;
	}
	g_debug ("Got initial list of %d UPower devices", devices->len);
	client = user_data;
	for (i = 0; i < devices->len; i++) {
		UpDevice *device = g_ptr_array_index (devices, i);
		up_device_added_cb (client->up_client, device, client);
	}
	g_ptr_array_unref (devices);
}

static void
up_client_coldplug (BluetoothClient *client)
{
	g_return_if_fail (client->up_client != NULL);
	up_client_get_devices_async (client->up_client, client->cancellable,
				     up_client_get_devices_cb, client);
}

static void
up_client_new_cb (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	BluetoothClient *client;
	UpClient *up_client;

	up_client = up_client_new_finish (res, &error);
	if (!up_client) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_debug ("Could not create UpClient: %s", error->message);
		return;
	}

	g_debug ("Successfully created UpClient");
	client = user_data;
	client->up_client = up_client;
	g_signal_connect_object (G_OBJECT (up_client), "device-added",
				 G_CALLBACK (up_device_added_cb), client, 0);
	g_signal_connect_object (G_OBJECT (up_client), "device-removed",
				 G_CALLBACK (up_device_removed_cb), client, 0);
	if (client->bluez_devices_coldplugged)
		up_client_coldplug (client);
}

static void bluetooth_client_init(BluetoothClient *client)
{
	client->cancellable = g_cancellable_new ();
	client->list_store = g_list_store_new (BLUETOOTH_TYPE_DEVICE);
	client->has_power_state = TRUE;

	g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
						  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  BLUEZ_MANAGER_PATH,
						  object_manager_get_proxy_type_func,
						  NULL, NULL,
						  client->cancellable,
						  object_manager_new_callback, client);
	up_client_new_async (client->cancellable,
			     up_client_new_cb,
			     client);
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
stop_discovery_cb (Adapter1     *adapter,
		   GAsyncResult *res,
		   gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	gboolean ret;

	ret = adapter1_call_stop_discovery_finish (adapter, res, &error);
	if (!ret) {
		g_debug ("Error calling StopDiscovery() on %s org.bluez.Adapter1: %s (%s, %d)",
			 g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)),
			 error->message, g_quark_to_string (error->domain), error->code);
	} else {
		g_debug ("Ran StopDiscovery() successfully on %s org.bluez.Adapter1",
			 g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)));
	}
}

static void
start_discovery_cb (Adapter1     *adapter,
		    GAsyncResult *res,
		    gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	gboolean ret;

	ret = adapter1_call_start_discovery_finish (adapter, res, &error);
	if (!ret) {
		g_debug ("Error calling StartDiscovery() on %s org.bluez.Adapter1: %s (%s, %d)",
			 g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)),
			 error->message, g_quark_to_string (error->domain), error->code);
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			BluetoothClient *client = user_data;
			client->discovery_started = FALSE;
		}
	}
}

static void
set_discovery_filter_cb (Adapter1     *adapter,
			 GAsyncResult *res,
			 gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	gboolean ret;

	ret = adapter1_call_set_discovery_filter_finish (adapter, res, &error);
	if (!ret) {
		g_debug ("Error calling SetDiscoveryFilter() on interface org.bluez.Adapter1: %s (%s, %d)",
			 error->message, g_quark_to_string (error->domain), error->code);
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			BluetoothClient *client = user_data;
			client->discovery_started = FALSE;
		}
	} else {
		BluetoothClient *client = user_data;

		g_debug ("Starting discovery on %s", g_dbus_proxy_get_object_path (G_DBUS_PROXY (adapter)));
		adapter1_call_start_discovery (ADAPTER1 (adapter),
					       client->cancellable,
					       (GAsyncReadyCallback) start_discovery_cb,
					       client);
	}
}

static void
_bluetooth_client_set_default_adapter_discovering (BluetoothClient *client,
						   gboolean         discovering)
{
	g_autoptr(GDBusProxy) adapter = NULL;
	GVariantBuilder builder;

	adapter = _bluetooth_client_get_default_adapter (client);
	if (adapter == NULL) {
		g_debug ("%s discovery requested, but no default adapter",
			 discovering ? "Starting" : "Stopping");
		client->discovery_started = FALSE;
		return;
	}

	if (!adapter1_get_powered (client->default_adapter) && discovering) {
		g_debug("Starting discovery requested, but default adapter is unpowered");
		client->discovery_started = FALSE;
		return;
	}

	if (client->discovery_started == discovering)
		return;

	client->discovery_started = discovering;

	if (discovering) {
		g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add (&builder, "{sv}",
				       "Discoverable", g_variant_new_boolean (TRUE));
		adapter1_call_set_discovery_filter (ADAPTER1 (adapter),
						    g_variant_builder_end (&builder),
						    client->cancellable,
						    (GAsyncReadyCallback) set_discovery_filter_cb,
						    client);
	} else {
		/* Don't cancel a discovery stop when the BluetoothClient
		 * is finalised, so don't pass a cancellable */
		g_debug ("Stopping discovery on %s", g_dbus_proxy_get_object_path (adapter));
		adapter1_call_stop_discovery (ADAPTER1 (adapter),
					      NULL,
					      (GAsyncReadyCallback) stop_discovery_cb,
					      client);
	}
}

static BluetoothAdapterState
adapter_get_state (BluetoothClient *client)
{
	const char *str;

	if (!client->default_adapter)
		return BLUETOOTH_ADAPTER_STATE_ABSENT;

	str = adapter1_get_power_state (client->default_adapter);
	if (str) {
		if (g_str_equal (str, "on"))
			return BLUETOOTH_ADAPTER_STATE_ON;
		if (g_str_equal (str, "off") ||
		    g_str_equal (str, "off-blocked"))
			return BLUETOOTH_ADAPTER_STATE_OFF;
		if (g_str_equal (str, "off-enabling"))
			return BLUETOOTH_ADAPTER_STATE_TURNING_ON;
		if (g_str_equal (str, "on-disabling"))
			return BLUETOOTH_ADAPTER_STATE_TURNING_OFF;
		g_warning_once ("Unexpected adapter PowerState value '%s'", str);
	} else {
		client->has_power_state = FALSE;
	}

	/* Fallback if property is missing, or value is unexpected */
	return adapter1_get_powered (client->default_adapter) ?
		BLUETOOTH_ADAPTER_STATE_ON : BLUETOOTH_ADAPTER_STATE_OFF;

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
	case PROP_DEFAULT_ADAPTER_STATE:
		g_value_set_enum (value, adapter_get_state (client));
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
	case PROP_DEFAULT_ADAPTER_POWERED:
		adapter_set_powered (client, g_value_get_boolean (value));
		break;
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
	g_clear_handle_id (&client->removed_devices_queue_id, g_source_remove);
	g_list_free_full (client->removed_devices_queue, g_free);
	client->removed_devices_queue = NULL;
	g_clear_object (&client->manager);
	g_object_unref (client->list_store);

	g_clear_object (&client->default_adapter);
	g_clear_object (&client->up_client);

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
	 *
	 * Note that #BluetoothClient::device-removed will not be called
	 * for each individual device as the model is cleared when the
	 * #BluetoothClient:default-adapter property changes.
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
							       FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothClient:default-adapter-state:
	 *
	 * The #BluetoothAdapterState of the default Bluetooth adapter. More precise than
	 * #BluetoothClient:default-adapter-powered.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_STATE,
					 g_param_spec_enum ("default-adapter-state", NULL,
							    "State of the default adapter",
							    BLUETOOTH_TYPE_ADAPTER_STATE,
							    BLUETOOTH_ADAPTER_STATE_ABSENT,
							    G_PARAM_READABLE));
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
	g_autoptr(BluetoothDevice) device = NULL;
	g_autoptr(GDBusProxy) proxy = NULL;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device_path != NULL, FALSE);

	device = get_device_for_path (client, device_path);
	if (device == NULL) {
		g_debug ("Couldn't find device '%s' in tree to mark it as trusted", device_path);
		return FALSE;
	}

	g_object_get (G_OBJECT (device),
		      "proxy", &proxy,
		      NULL);

	g_object_set (proxy, "trusted", trusted, NULL);

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
 * This will start the process of connecting to one of the known-connectable
 * services on the device. This means that it could connect to all the audio
 * services on a headset, but just to the input service on a keyboard.
 *
 * Broadly speaking, this will only have an effect on devices with audio and HID
 * services, and do nothing if the device doesn't have the "connectable"
 * property set.
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
	GTask *task;
	g_autoptr(BluetoothDevice) device = NULL;
	g_autoptr(GDBusProxy) proxy = NULL;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (path != NULL);

	task = g_task_new (G_OBJECT (client),
			   cancellable,
			   callback,
			   user_data);
	g_task_set_source_tag (task, bluetooth_client_connect_service);

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

	if (connect) {
		device1_call_connect (DEVICE1(proxy),
				      cancellable,
				      (GAsyncReadyCallback) connect_callback,
				      task);
	} else {
		device1_call_disconnect (DEVICE1(proxy),
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

/**
 * bluetooth_client_has_connected_input_devices:
 * @client: a #BluetoothClient
 *
 * Returns whether there are connected devices with the input capability.
 * This can be used by an OS user interface to warn the user before disabling
 * Bluetooth so that the user isn't stranded without any input devices.
 *
 * Returns: %TRUE if there are connected input devices.
 **/
gboolean
bluetooth_client_has_connected_input_devices (BluetoothClient *client)
{
	guint i, n_items;
	guint n_connected = 0;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (client->list_store));
	for (i = 0; i < n_items; i++) {
		g_autoptr(BluetoothDevice) device = NULL;
		g_auto(GStrv) uuids = NULL;
		gboolean connected = FALSE;

		device = g_list_model_get_item (G_LIST_MODEL (client->list_store), i);
		g_object_get (device,
			      "connected", &connected,
			      "uuids", &uuids, NULL);
		if (!connected)
			continue;
		if (!uuids)
			continue;
		if (g_strv_contains ((const gchar * const *) uuids, "Human Interface Device") ||
		    g_strv_contains ((const gchar * const *) uuids, "HumanInterfaceDeviceService"))
			n_connected++;
	}
	g_debug ("Found %i input devices connected", n_connected);

	return n_connected > 0;
}
