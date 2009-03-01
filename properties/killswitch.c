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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <hal/libhal.h>

#include "killswitch.h"

#define BLUETOOTH_KILLSWITCH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitchPrivate))

typedef struct _BluetoothIndKillswitch BluetoothIndKillswitch;
struct _BluetoothIndKillswitch {
	char *udi;
	DBusPendingCall *call;
	gboolean killed;
};

typedef struct _BluetoothKillswitchPrivate BluetoothKillswitchPrivate;
struct _BluetoothKillswitchPrivate {
	LibHalContext *halctx;
	DBusConnection *connection;
	GList *killswitches; /* a GList of BluetoothIndKillswitch */
};

G_DEFINE_TYPE(BluetoothKillswitch, bluetooth_killswitch, G_TYPE_OBJECT)

static void
setpower_reply (DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply;
	gint32 power;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &power,
				  DBUS_TYPE_INVALID) == FALSE) {
		dbus_message_unref(reply);
		return;
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref (call);
}

static void
getpower_reply (DBusPendingCall *call, void *user_data)
{
	BluetoothKillswitch *killswitch = BLUETOOTH_KILLSWITCH (user_data);
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	DBusMessage *reply;
	GList *l;
	gint32 power;

	reply = dbus_pending_call_steal_reply (call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &power,
				  DBUS_TYPE_INVALID) == FALSE) {
		dbus_message_unref(reply);
		return;
	}

	/* Look for the killswitch */
	for (l = priv->killswitches; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		g_message ("comparing %p", call);
		g_message ("with %p", ind->call);
		if (call != ind->call)
			continue;
		ind->killed = (power > 0);
		ind->call = NULL;
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref (call);

	//g_object_notify (G_OBJECT (killswitch), "killed");
}

void
bluetooth_killswitch_update_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	GList *l;

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		DBusPendingCall *call;
		DBusMessage *message;

		/* Already checking status */
		if (ind->call != NULL)
			continue;

		message = dbus_message_new_method_call ("org.freedesktop.Hal",
							ind->udi,
							"org.freedesktop.Hal.Device.KillSwitch",
							"GetPower");

		if (message == NULL)
			continue;

		if (dbus_connection_send_with_reply (priv->connection, message, &call, -1) == FALSE) {
			dbus_message_unref(message);
			continue;
		}

		dbus_pending_call_set_notify (call, getpower_reply, killswitch, NULL);

		dbus_message_unref (message);
	}
}

void
bluetooth_killswitch_set_state (BluetoothKillswitch *killswitch, KillswitchState state)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	gboolean value;
	GList *l;

	g_return_if_fail (state == KILLSWITCH_STATE_KILLED || state == KILLSWITCH_STATE_NOT_KILLED);

	value = (state == KILLSWITCH_STATE_NOT_KILLED);

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		DBusPendingCall *call;
		DBusMessage *message;

		message = dbus_message_new_method_call("org.freedesktop.Hal",
						       ind->udi,
						       "org.freedesktop.Hal.Device.KillSwitch",
						       "SetPower");
		if (message == NULL)
			return;

		dbus_message_append_args(message,
					 DBUS_TYPE_BOOLEAN, &value,
					 DBUS_TYPE_INVALID);

		if (dbus_connection_send_with_reply(priv->connection, message,
						    &call, -1) == FALSE) {
			dbus_message_unref(message);
			return;
		}

		ind->call = call;
		g_message ("set state, call %p", call);

		dbus_pending_call_set_notify(call, setpower_reply, killswitch, NULL);

		dbus_message_unref(message);
	}
}

KillswitchState
bluetooth_killswitch_get_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	int state = KILLSWITCH_STATE_UNKNOWN;
	GList *l;

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		if (ind->killed) {
			if (state == KILLSWITCH_STATE_UNKNOWN)
				state = KILLSWITCH_STATE_KILLED;
			if (state != KILLSWITCH_STATE_KILLED)
				state = KILLSWITCH_STATE_MIXED;
		} else {
			if (state == KILLSWITCH_STATE_UNKNOWN)
				state = KILLSWITCH_STATE_NOT_KILLED;
			if (state != KILLSWITCH_STATE_NOT_KILLED)
				state = KILLSWITCH_STATE_MIXED;
		}
	}

	if (state == KILLSWITCH_STATE_UNKNOWN)
		return KILLSWITCH_STATE_UNKNOWN;

	return state;
}

gboolean
bluetooth_killswitch_has_killswitches (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);

	return (priv->killswitches != NULL);
}

static void
add_killswitch (BluetoothKillswitch *killswitch, const char *udi)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	DBusPendingCall *call;
	DBusMessage *message;
	BluetoothIndKillswitch *ind;
	char *type;

	type = libhal_device_get_property_string (priv->halctx,
						  udi,
						  "killswitch.type",
						  NULL);
	if (type == NULL || g_str_equal(type, "bluetooth") == FALSE) {
		g_free (type);
		return;
	}
	g_free (type);

	message = dbus_message_new_method_call ("org.freedesktop.Hal",
						udi,
						"org.freedesktop.Hal.Device.KillSwitch",
						"GetPower");

	if (message == NULL)
		return;

	if (dbus_connection_send_with_reply (priv->connection, message, &call, -1) == FALSE) {
		dbus_message_unref(message);
		return;
	}

	ind = g_new0 (BluetoothIndKillswitch, 1);
	ind->udi = g_strdup (udi);
	ind->call = call;
	priv->killswitches = g_list_append (priv->killswitches, ind);

	g_message ("add killswitch %p", call);

	dbus_pending_call_set_notify (call, getpower_reply, killswitch, NULL);

	dbus_message_unref (message);
}

static void
bluetooth_killswitch_init (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	char **list;
	int num;

	priv->connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (priv->connection == NULL)
		return;

	priv->halctx = libhal_ctx_new();
	if (priv->halctx == NULL) {
		//FIXME clean up connection
		return;
	}

	if (libhal_ctx_set_dbus_connection(priv->halctx, priv->connection) == FALSE) {
		libhal_ctx_free(priv->halctx);
		//FIXME clean up connection
		priv->halctx = NULL;
		return;
	}

	if (libhal_ctx_init(priv->halctx, NULL) == FALSE) {
		//FIXME clean up connection
		g_printerr("Couldn't init HAL context\n");
		libhal_ctx_free(priv->halctx);
		priv->halctx = NULL;
		return;
	}

	list = libhal_find_device_by_capability(priv->halctx, "killswitch", &num, NULL);
	if (list) {
		char **tmp = list;

		while (*tmp) {
			add_killswitch (killswitch, *tmp);
			tmp++;
		}

		libhal_free_string_array (list);
	}
}

static void
bluetooth_killswitch_finalize (GObject *object)
{
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (object);

	if (priv->halctx != NULL) {
		libhal_ctx_shutdown(priv->halctx, NULL);
		libhal_ctx_free(priv->halctx);
		priv->halctx = NULL;
	}

	if (priv->connection != NULL) {
		dbus_connection_unref(priv->connection);
		priv->connection = NULL;
	}

	//FIXME
	//kill everything in killswitches

	G_OBJECT_CLASS(bluetooth_killswitch_parent_class)->finalize(object);
}

static void
bluetooth_killswitch_class_init(BluetoothKillswitchClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothKillswitchPrivate));
	object_class->finalize = bluetooth_killswitch_finalize;
}

BluetoothKillswitch *
bluetooth_killswitch_new (void)
{
	return BLUETOOTH_KILLSWITCH(g_object_new (BLUETOOTH_TYPE_KILLSWITCH, NULL));
}

