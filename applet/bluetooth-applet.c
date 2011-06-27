/* -*- tab-width: 8 -*-
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <gio/gio.h>

#include <bluetooth-applet.h>
#include <bluetooth-client.h>
#include <bluetooth-client-private.h>
#include <bluetooth-killswitch.h>
#include <bluetooth-agent.h>

#include <marshal.h>

static gpointer
bluetooth_simple_device_copy (gpointer boxed)
{
	BluetoothSimpleDevice* origin = (BluetoothSimpleDevice*) boxed;

	BluetoothSimpleDevice* result = g_new (BluetoothSimpleDevice, 1);
	result->bdaddr = g_strdup (origin->bdaddr);
	result->device_path = g_strdup (origin->device_path);
	result->alias = g_strdup (origin->alias);
	result->connected = origin->connected;
	result->can_connect = origin->can_connect;
	result->capabilities = origin->capabilities;
	result->type = origin->type;

	return (gpointer)result;
}

static void
bluetooth_simple_device_free (gpointer boxed)
{
	BluetoothSimpleDevice* obj = (BluetoothSimpleDevice*) boxed;

	g_free (obj->device_path);
	g_free (obj->bdaddr);
	g_free (obj->alias);
	g_free (obj);
}

G_DEFINE_BOXED_TYPE(BluetoothSimpleDevice, bluetooth_simple_device, bluetooth_simple_device_copy, bluetooth_simple_device_free)

struct _BluetoothApplet
{
	GObject parent_instance;

	BluetoothKillswitch* killswitch_manager;
	BluetoothClient* client;
	GtkTreeModel* device_model;
	gulong signal_row_added;
	gulong signal_row_changed;
	gulong signal_row_deleted;
	DBusGProxy* default_adapter;
	BluetoothAgent* agent;
	GHashTable* pending_requests;

	gint num_adapters_powered;
	gint num_adapters_present;
};

struct _BluetoothAppletClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE(BluetoothApplet, bluetooth_applet, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_KILLSWITCH_STATE,
	PROP_DISCOVERABLE,
	PROP_FULL_MENU,
	PROP_LAST
};
static GParamSpec *properties[PROP_LAST];

enum {
	SIGNAL_DEVICES_CHANGED,

	SIGNAL_PINCODE_REQUEST,
	SIGNAL_CONFIRM_REQUEST,
	SIGNAL_AUTHORIZE_REQUEST,
	SIGNAL_CANCEL_REQUEST,

	SIGNAL_LAST
};

guint signals[SIGNAL_LAST];

typedef struct {
  GSimpleAsyncResult *result;
  guint timestamp;
} MountClosure;

static void
mount_ready_cb (GObject *object,
		GAsyncResult *result,
		gpointer user_data)
{
	GError *error = NULL;
	GFile *file = G_FILE (object);
	char *uri = g_file_get_uri (file);
	MountClosure *closure = user_data;

	if (g_file_mount_enclosing_volume_finish (file, result, &error) == FALSE) {
		/* Ignore "already mounted" error */
		if (error->domain == G_IO_ERROR &&
		    error->code == G_IO_ERROR_ALREADY_MOUNTED) {
			g_error_free (error);
			error = NULL;
		}
	}

	if (!error) {
		gtk_show_uri (NULL, uri, closure->timestamp, &error);
	}

	if (error) {
		g_simple_async_result_set_from_error (closure->result, error);
		g_error_free (error);
	} else {
		g_simple_async_result_set_op_res_gboolean (closure->result, TRUE);
	}

	g_simple_async_result_complete (closure->result);

	g_free (uri);
	g_object_unref (closure->result);
	g_free (closure);
}

/**
 * bluetooth_applet_browse_address_finish:
 *
 * @applet: a #BluetoothApplet
 * @result: the #GAsyncResult from the callback
 * @error:
 *
 * Returns: TRUE if the operation was successful, FALSE if error is set
 */
gboolean
bluetooth_applet_browse_address_finish (BluetoothApplet *applet,
					GAsyncResult *result,
					GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (applet), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (applet), bluetooth_applet_browse_address), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;
	else
		return TRUE;
}

/**
 * bluetooth_applet_browse_address:
 *
 * Opens a Bluetooth device in Nautilus
 * @applet: a #BluetoothApplet
 * @address: the bluetooth device to browse
 * @callback: (scope async): the completion callback
 * @user_data:
 */
void bluetooth_applet_browse_address (BluetoothApplet *applet,
				      const char *address,
				      guint timestamp,
				      GAsyncReadyCallback callback,
				      gpointer user_data)
{
	GFile *file;
	char *uri;
	MountClosure *closure;

	g_return_if_fail (BLUETOOTH_IS_APPLET (applet));
	g_return_if_fail (address != NULL);

	uri = g_strdup_printf ("obex://[%s]/", address);
	file = g_file_new_for_uri (uri);

	closure = g_new (MountClosure, 1);
	closure->result = g_simple_async_result_new (G_OBJECT (applet), callback, user_data, bluetooth_applet_browse_address);
	closure->timestamp = timestamp;
	g_file_mount_enclosing_volume(file, G_MOUNT_MOUNT_NONE, NULL, NULL, mount_ready_cb, closure);

	g_free (uri);
	g_object_unref (file);
}

/**
 * bluetooth_applet_send_to_address:
 *
 * Send a file to a bluetooth device
 * @applet: a #BluetoothApplet
 * @address: the target
 * @alias: the string to display for the device
 */
void bluetooth_applet_send_to_address (BluetoothApplet *applet,
				       const char *address,
				       const char *alias)
{
	GPtrArray *a;
	GError *err = NULL;
	guint i;

	g_return_if_fail (BLUETOOTH_IS_APPLET (applet));

	a = g_ptr_array_new ();
	g_ptr_array_add (a, "bluetooth-sendto");
	if (address != NULL) {
		char *s;

		s = g_strdup_printf ("--device=%s", address);
		g_ptr_array_add (a, s);
	}
	if (address != NULL && alias != NULL) {
		char *s;

		s = g_strdup_printf ("--name=\"%s\"", alias);
		g_ptr_array_add (a, s);
	}
	g_ptr_array_add (a, NULL);

	if (g_spawn_async(NULL, (char **) a->pdata, NULL,
			  G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err) == FALSE) {
		g_printerr("Couldn't execute command: %s\n", err->message);
		g_error_free (err);
	}

	for (i = 1; a->pdata[i] != NULL; i++)
		g_free (a->pdata[i]);

	g_ptr_array_free (a, TRUE);
}

/**
 * bluetooth_applet_agent_reply_pincode:
 *
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @pincode: (allow-none): the PIN code entered by the user, as a string, or NULL if the dialog was dismissed
 */
void
bluetooth_applet_agent_reply_pincode (BluetoothApplet *self,
				      const char      *request_key,
				      const char      *pincode)
{
	DBusGMethodInvocation* context;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	context = g_hash_table_lookup (self->pending_requests, request_key);

	if (pincode != NULL) {
		dbus_g_method_return (context, pincode);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Pairing request rejected");
		dbus_g_method_return_error (context, error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_passkey:
 *
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @passkey: the numeric PIN code entered by the user, or -1 if the dialog was dismissed
 */
void
bluetooth_applet_agent_reply_passkey (BluetoothApplet *self,
				      const char      *request_key,
				      int              passkey)
{
	DBusGMethodInvocation* context;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	context = g_hash_table_lookup (self->pending_requests, request_key);

	if (passkey != -1) {
		dbus_g_method_return (context, passkey);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Pairing request rejected");
		dbus_g_method_return_error (context, error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_confirm:
 *
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @confirm: TRUE if operation was confirmed, FALSE otherwise
 */
void
bluetooth_applet_agent_reply_confirm (BluetoothApplet *self,
				      const char      *request_key,
				      gboolean         confirm)
{
	DBusGMethodInvocation* context;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	context = g_hash_table_lookup (self->pending_requests, request_key);

	if (confirm) {
		dbus_g_method_return (context);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Confirmation request rejected");
		dbus_g_method_return_error (context, error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_auth:
 *
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @auth: TRUE if operation was authorized, FALSE otherwise
 * @trusted: TRUE if the operation should be authorized automatically in the future
 */
void
bluetooth_applet_agent_reply_auth (BluetoothApplet *self,
				   const char      *request_key,
				   gboolean         auth,
				   gboolean         trusted)
{
	DBusGMethodInvocation* context;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	context = g_hash_table_lookup (self->pending_requests, request_key);

	if (auth) {
		if (trusted)
			bluetooth_client_set_trusted (self->client, request_key, TRUE);

		dbus_g_method_return (context);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Confirmation request rejected");
		dbus_g_method_return_error (context, error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

#ifndef DBUS_TYPE_G_DICTIONARY
#define DBUS_TYPE_G_DICTIONARY \
	(dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#endif

static char *
device_get_name (DBusGProxy *proxy, char **long_name)
{
	GHashTable *hash;
	GValue *value;
	char *alias, *address;

	g_return_val_if_fail (long_name != NULL, NULL);

	if (dbus_g_proxy_call (proxy, "GetProperties",  NULL,
			       G_TYPE_INVALID,
			       DBUS_TYPE_G_DICTIONARY, &hash,
			       G_TYPE_INVALID) == FALSE) {
		return NULL;
	}

	value = g_hash_table_lookup (hash, "Address");
	if (value == NULL) {
		g_hash_table_destroy (hash);
		return NULL;
	}
	address = g_value_dup_string (value);

	value = g_hash_table_lookup (hash, "Name");
	alias = value ? g_value_dup_string (value) : address;

	g_hash_table_destroy (hash);

	if (value)
		*long_name = g_strdup_printf ("'%s' (%s)", alias, address);
	else
		*long_name = g_strdup_printf ("'%s'", address);

	if (alias != address)
		g_free (address);
	return alias;
}

static gboolean
pincode_request (DBusGMethodInvocation *context,
		 DBusGProxy            *device,
		 gpointer               user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = dbus_g_proxy_get_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), context);

	g_signal_emit (self, signals[SIGNAL_PINCODE_REQUEST], 0, path, name, long_name, FALSE);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
passkey_request (DBusGMethodInvocation *context,
		 DBusGProxy            *device,
		 gpointer               user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = dbus_g_proxy_get_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), context);

	g_signal_emit (self, signals[SIGNAL_PINCODE_REQUEST], 0, path, name, long_name, TRUE);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
confirm_request (DBusGMethodInvocation *context,
		 DBusGProxy *device,
		 guint pin,
		 gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = dbus_g_proxy_get_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), context);

	g_signal_emit (self, signals[SIGNAL_CONFIRM_REQUEST], 0, path, name, long_name, pin);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
authorize_request (DBusGMethodInvocation *context,
		   DBusGProxy *device,
		   const char *uuid,
		   gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = dbus_g_proxy_get_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), context);

	g_signal_emit (self, signals[SIGNAL_AUTHORIZE_REQUEST], 0, path, name, long_name, uuid);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static void
cancel_request_single (gpointer key, gpointer value, gpointer user_data)
{
	DBusGMethodInvocation* request_context = value;
	GError* result;

	if (value) {
		result = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT, "Agent callback cancelled");
		dbus_g_method_return_error (request_context, result);
	}
}

static gboolean
cancel_request(DBusGMethodInvocation *context,
               gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);

	g_hash_table_foreach (self->pending_requests, cancel_request_single, NULL);
	g_hash_table_remove_all (self->pending_requests);

	g_signal_emit (self, signals[SIGNAL_CANCEL_REQUEST], 0);

	return TRUE;
}

static void
device_added_or_changed (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
	BluetoothApplet *self = BLUETOOTH_APPLET (data);

	g_signal_emit (self, signals[SIGNAL_DEVICES_CHANGED], 0);
}

static void
device_removed (GtkTreeModel *model,
	        GtkTreePath *path,
	        gpointer user_data)
{
	device_added_or_changed (model, path, NULL, user_data);
}

static void
default_adapter_changed (GObject    *client,
			 GParamSpec *spec,
			 gpointer    data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (data);

	if (self->default_adapter)
		g_object_unref (self->default_adapter);
	self->default_adapter = bluetooth_client_get_default_adapter (self->client);

	if (self->device_model) {
		g_signal_handler_disconnect (self->device_model, self->signal_row_added);
		g_signal_handler_disconnect (self->device_model, self->signal_row_changed);
		g_signal_handler_disconnect (self->device_model, self->signal_row_deleted);
		g_object_unref (self->device_model);
	}
	if (self->default_adapter)
		self->device_model = bluetooth_client_get_device_model (self->client, self->default_adapter);
	else
		self->device_model = NULL;

	if (self->device_model) {
		self->signal_row_added = g_signal_connect (self->device_model, "row-inserted",
							   G_CALLBACK(device_added_or_changed), self);
		self->signal_row_deleted = g_signal_connect (self->device_model, "row-deleted",
							     G_CALLBACK(device_removed), self);
		self->signal_row_changed = g_signal_connect (self->device_model, "row-changed",
							     G_CALLBACK (device_added_or_changed), self);
	}

	if (self->agent)
		g_object_unref (self->agent);

	if (self->default_adapter) {
		self->agent = bluetooth_agent_new ();
		g_object_add_weak_pointer (G_OBJECT (self->agent), (void**) &(self->agent));

		bluetooth_agent_set_pincode_func (self->agent, pincode_request, self);
		bluetooth_agent_set_passkey_func (self->agent, passkey_request, self);
		bluetooth_agent_set_authorize_func (self->agent, authorize_request, self);
		bluetooth_agent_set_confirm_func (self->agent, confirm_request, self);
		bluetooth_agent_set_cancel_func (self->agent, cancel_request, self);

		bluetooth_agent_register (self->agent, self->default_adapter);
	}

	g_signal_emit (self, signals[SIGNAL_DEVICES_CHANGED], 0);
}

static void
default_adapter_powered_changed (GObject    *client,
				 GParamSpec *spec,
				 gpointer    data)
{
        BluetoothApplet *self = BLUETOOTH_APPLET (data);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FULL_MENU]);
}

static void
default_adapter_discoverable_changed (GObject    *client,
				 GParamSpec *spec,
				 gpointer    data)
{
        BluetoothApplet *self = BLUETOOTH_APPLET (data);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISCOVERABLE]);
}

static gboolean
set_powered_foreach (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
	DBusGProxy *proxy = NULL;
	GValue value = { 0, };

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy, -1);
	if (proxy == NULL)
		return FALSE;

	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, TRUE);

	dbus_g_proxy_call_no_reply (proxy, "SetProperty",
				    G_TYPE_STRING, "Powered",
				    G_TYPE_VALUE, &value,
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	g_value_unset (&value);
	g_object_unref (proxy);

	return FALSE;
}

static void
set_adapter_powered (BluetoothApplet* self)
{
	GtkTreeModel *adapters;

	adapters = bluetooth_client_get_adapter_model (self->client);
	gtk_tree_model_foreach (adapters, set_powered_foreach, NULL);
	g_object_unref (adapters);
}

static gboolean
device_has_uuid (const char **uuids, const char *uuid)
{
	guint i;

	if (uuids == NULL)
		return FALSE;

	for (i = 0; uuids[i] != NULL; i++) {
		if (g_str_equal (uuid, uuids[i]) != FALSE)
			return TRUE;
	}
	return FALSE;
}

static void
killswitch_state_change (BluetoothKillswitch *kill_switch, KillswitchState state, gpointer user_data)
{
  BluetoothApplet *self = BLUETOOTH_APPLET (user_data);

  g_object_notify (G_OBJECT (self), "killswitch-state");
}

typedef struct {
	BluetoothApplet* self;
	BluetoothAppletConnectFunc func;
	gpointer user_data;
} ConnectionClosure;

static void
connection_callback (BluetoothClient* client, gboolean success, gpointer data)
{
	ConnectionClosure *closure = (ConnectionClosure*) data;

	(*(closure->func)) (closure->self, success, closure->user_data);

	g_free (closure);
}

/**
 * bluetooth_applet_connect_device:
 *
 * @applet: a #BluetoothApplet
 * @device: the device to connect
 * @func: (scope async): a completion callback
 * @data: user data
 */
gboolean
bluetooth_applet_connect_device (BluetoothApplet* applet,
                                 const char* device,
                                 BluetoothAppletConnectFunc func,
                                 gpointer data)
{
	ConnectionClosure *closure;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (applet), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	closure = g_new (ConnectionClosure, 1);
	closure->self = applet;
	closure->func = func;
	closure->user_data = data;

	return bluetooth_client_connect_service (applet->client, device, connection_callback, closure);
}

/**
 * bluetooth_applet_disconnect_device:
 *
 * @applet: a #BluetoothApplet
 * @device: the device to disconnect
 * @func: (scope async): a completion callback
 * @data: user data
 */
gboolean
bluetooth_applet_disconnect_device (BluetoothApplet* applet,
                                 const char* device,
                                 BluetoothAppletConnectFunc func,
                                 gpointer data)
{
	ConnectionClosure *closure;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (applet), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	closure = g_new (ConnectionClosure, 1);
	closure->self = applet;
	closure->func = func;
	closure->user_data = data;

	return bluetooth_client_disconnect_service (applet->client, device, connection_callback, closure);
}

/**
 * bluetooth_applet_get_discoverable:
 *
 * @self: a #BluetoothApplet
 *
 * Returns: TRUE if the default adapter is discoverable, false otherwise
 */
gboolean
bluetooth_applet_get_discoverable (BluetoothApplet* self)
{
	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

       	return bluetooth_client_get_discoverable (self->client);
}

/**
 * bluetooth_applet_set_discoverable:
 *
 * @self: a #BluetoothApplet
 * @disc:
 */
void
bluetooth_applet_set_discoverable (BluetoothApplet* self, gboolean disc)
{
	g_return_if_fail (BLUETOOTH_IS_APPLET (self));

	bluetooth_client_set_discoverable (self->client, disc, 0);
}

/**
 * bluetooth_applet_get_killswitch_state:
 *
 * @self: a #BluetoothApplet
 *
 * Returns: the state of the killswitch, if one is present, or BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER otherwise
 */
BluetoothKillswitchState
bluetooth_applet_get_killswitch_state (BluetoothApplet* self)
{

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER);

	if (bluetooth_killswitch_has_killswitches (self->killswitch_manager))
		return bluetooth_killswitch_get_state (self->killswitch_manager);
	else
		return BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER;
}

/**
 * bluetooth_applet_set_killswitch_state:
 *
 * @self: a #BluetoothApplet
 * @state: the new state
 *
 * Returns: TRUE if the operation could be performed, FALSE otherwise
 */
gboolean
bluetooth_applet_set_killswitch_state (BluetoothApplet* self, BluetoothKillswitchState state)
{

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

	if (bluetooth_killswitch_has_killswitches (self->killswitch_manager)) {
		bluetooth_killswitch_set_state (self->killswitch_manager, state);
		return TRUE;
	}
	return FALSE;
}

/**
 * bluetooth_applet_get_show_full_menu:
 *
 * @self: a #BluetoothApplet
 *
 * Returns: TRUE if the full menu is to be shown, FALSE otherwise
 * (full menu includes device submenus and global actions)
 */
gboolean
bluetooth_applet_get_show_full_menu (BluetoothApplet* self)
{
	gboolean has_adapter, has_powered_adapter;
	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

	has_adapter = self->default_adapter != NULL;
	g_object_get (G_OBJECT (self->client), "default-adapter-powered", &has_powered_adapter, NULL);

	if (!has_adapter)
		return FALSE;

	return has_powered_adapter &&
		bluetooth_applet_get_killswitch_state(self) == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
}

static BluetoothSimpleDevice *
bluetooth_applet_create_device_from_iter (GtkTreeModel *model,
					  GtkTreeIter  *iter,
					  gboolean      check_proxy)
{
	BluetoothSimpleDevice *dev;
	GHashTable *services;
	DBusGProxy *proxy;
	char **uuids;

	dev = g_new0 (BluetoothSimpleDevice, 1);

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_ADDRESS, &dev->bdaddr,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    BLUETOOTH_COLUMN_SERVICES, &services,
			    BLUETOOTH_COLUMN_ALIAS, &dev->alias,
			    BLUETOOTH_COLUMN_UUIDS, &uuids,
			    BLUETOOTH_COLUMN_TYPE, &dev->type,
			    -1);

	if (dev->bdaddr == NULL || dev->alias == NULL ||
	    (check_proxy != FALSE && proxy == NULL)) {
		if (proxy != NULL)
			g_object_unref (proxy);
		g_strfreev (uuids);
		if (services != NULL)
			g_hash_table_unref (services);
		bluetooth_simple_device_free (dev);

		return NULL;
	}

	if (proxy != NULL) {
		dev->device_path = g_strdup (dbus_g_proxy_get_path (proxy));
		g_object_unref (proxy);
	}

	/* If one service is connected, then we're connected */
	dev->connected = FALSE;
	dev->can_connect = FALSE;
	if (services != NULL) {
		GList *list, *l;

		dev->can_connect = TRUE;
		list = g_hash_table_get_values (services);
		for (l = list; l != NULL; l = l->next) {
			BluetoothStatus val = GPOINTER_TO_INT (l->data);
			if (val == BLUETOOTH_STATUS_CONNECTED ||
			    val == BLUETOOTH_STATUS_PLAYING) {
				dev->connected = TRUE;
				break;
			}
		}
		g_list_free (list);
	}

	dev->capabilities = 0;
	dev->capabilities |= device_has_uuid ((const char **) uuids, "OBEXObjectPush") ? BLUETOOTH_CAPABILITIES_OBEX_PUSH : 0;
	dev->capabilities |= device_has_uuid ((const char **) uuids, "OBEXFileTransfer") ? BLUETOOTH_CAPABILITIES_OBEX_FILE_TRANSFER : 0;

	if (services != NULL)
		g_hash_table_unref (services);
	g_strfreev (uuids);

	return dev;
}

/**
 * bluetooth_applet_get_devices:
 *
 * @self: a #BluetoothApplet
 *
 * Returns: (element-type GnomeBluetoothApplet.SimpleDevice) (transfer full): Returns the devices which should be shown to the user
 */
GList*
bluetooth_applet_get_devices (BluetoothApplet* self)
{
	GList* result = NULL;
	GtkTreeIter iter;
	gboolean cont;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), NULL);

	/* No adapter */
	if (self->default_adapter == NULL)
		return NULL;

	cont = gtk_tree_model_get_iter_first (self->device_model, &iter);
	while (cont) {
		BluetoothSimpleDevice *dev;

		dev = bluetooth_applet_create_device_from_iter (self->device_model, &iter, TRUE);

		if (dev != NULL)
			result = g_list_prepend (result, dev);

		cont = gtk_tree_model_iter_next (self->device_model, &iter);
	}

	result = g_list_reverse (result);

	return result;
}

static void
bluetooth_applet_get_property (GObject    *self,
			       guint       property_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_FULL_MENU:
		g_value_set_boolean (value, bluetooth_applet_get_show_full_menu (BLUETOOTH_APPLET (self)));
		return;
	case PROP_KILLSWITCH_STATE:
		g_value_set_int (value, bluetooth_applet_get_killswitch_state (BLUETOOTH_APPLET (self)));
		return;
	case PROP_DISCOVERABLE:
		g_value_set_boolean (value, bluetooth_applet_get_discoverable (BLUETOOTH_APPLET (self)));
		return;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
	}
}

static void
bluetooth_applet_set_property (GObject      *gobj,
			       guint         property_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	BluetoothApplet *self = BLUETOOTH_APPLET (gobj);

	switch (property_id) {
	case PROP_KILLSWITCH_STATE:
		bluetooth_applet_set_killswitch_state (self, g_value_get_int (value));
		return;
	case PROP_DISCOVERABLE:
		bluetooth_applet_set_discoverable (self, g_value_get_boolean (value));
		return;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
	}
}

static void
bluetooth_applet_init (BluetoothApplet *self)
{
	self->client = bluetooth_client_new ();
	self->device_model = NULL;

	self->default_adapter = NULL;
	self->agent = NULL;

	self->killswitch_manager = bluetooth_killswitch_new ();
	g_signal_connect (self->killswitch_manager, "state-changed", G_CALLBACK(killswitch_state_change), self);

	self->pending_requests = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	dbus_g_error_domain_register (AGENT_ERROR, "org.bluez.Error", AGENT_ERROR_TYPE);

	/* Make sure all the unblocked adapters are powered,
	 * so as to avoid seeing unpowered, but unblocked
	 * devices */
	set_adapter_powered (self);
	default_adapter_changed (NULL, NULL, self);

	g_signal_connect (self->client, "notify::default-adapter", G_CALLBACK (default_adapter_changed), self);
	g_signal_connect (self->client, "notify::default-adapter-powered", G_CALLBACK (default_adapter_powered_changed), self);
	g_signal_connect (self->client, "notify::default-adapter-discoverable", G_CALLBACK (default_adapter_discoverable_changed), self);
}

static void
bluetooth_applet_dispose (GObject* self)
{

	BluetoothApplet* applet = BLUETOOTH_APPLET (self);

	if (applet->client) {
		g_object_unref (applet->client);
		applet->client = NULL;
	}

	if (applet->killswitch_manager) {
		g_object_unref (applet->killswitch_manager);
		applet->killswitch_manager = NULL;
	}

	if (applet->device_model) {
		g_object_unref (applet->device_model);
		applet->device_model = NULL;
	}

	if (applet->agent) {
		g_object_unref (applet->agent);
		applet->agent = NULL;
	}
}

static void
bluetooth_applet_class_init (BluetoothAppletClass *klass)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = bluetooth_applet_dispose;
	gobject_class->get_property = bluetooth_applet_get_property;
	gobject_class->set_property = bluetooth_applet_set_property;

	/* should be enum, but KillswitchState is not registered */
	properties[PROP_KILLSWITCH_STATE] = g_param_spec_int ("killswitch-state",
							      "Killswitch state",
							      "State of Bluetooth hardware switches",
							      KILLSWITCH_STATE_NO_ADAPTER, KILLSWITCH_STATE_HARD_BLOCKED, KILLSWITCH_STATE_NO_ADAPTER,
							      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_KILLSWITCH_STATE, properties[PROP_KILLSWITCH_STATE]);

	properties[PROP_DISCOVERABLE] = g_param_spec_boolean ("discoverable",
							      "Adapter visibility",
							      "Whether the adapter is visible or not",
							      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_DISCOVERABLE, properties[PROP_DISCOVERABLE]);

	properties[PROP_FULL_MENU] = g_param_spec_boolean ("show-full-menu",
							   "Show the full applet menu",
							   "Show actions related to the adapter and other miscellanous in the main menu",
							   TRUE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_FULL_MENU, properties[PROP_FULL_MENU]);

	signals[SIGNAL_DEVICES_CHANGED] = g_signal_new ("devices-changed", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
							G_TYPE_NONE, 0);

	signals[SIGNAL_PINCODE_REQUEST] = g_signal_new ("pincode-request", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST, 0, NULL, NULL, marshal_VOID__STRING_STRING_STRING_BOOLEAN,
							G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

	signals[SIGNAL_CONFIRM_REQUEST] = g_signal_new ("confirm-request", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST, 0, NULL, NULL, marshal_VOID__STRING_STRING_STRING_UINT,
							G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	signals[SIGNAL_AUTHORIZE_REQUEST] = g_signal_new ("auth-request", G_TYPE_FROM_CLASS (gobject_class),
							  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, marshal_VOID__STRING_STRING_STRING_STRING,
							  G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	signals[SIGNAL_CANCEL_REQUEST] = g_signal_new ("cancel-request", G_TYPE_FROM_CLASS (gobject_class),
						       G_SIGNAL_RUN_FIRST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
						       G_TYPE_NONE, 0);
}

