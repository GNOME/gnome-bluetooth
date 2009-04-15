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

#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>

#include "bluetooth-killswitch.h"

enum {
	STATE_CHANGED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

#define BLUETOOTH_KILLSWITCH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitchPrivate))

typedef struct _BluetoothIndKillswitch BluetoothIndKillswitch;
struct _BluetoothIndKillswitch {
	char *udi;
	DBusPendingCall *call;
	KillswitchState state, target_state;
};

typedef struct _BluetoothKillswitchPrivate BluetoothKillswitchPrivate;
struct _BluetoothKillswitchPrivate {
	LibHalContext *halctx;
	DBusConnection *connection;
	guint num_remaining_answers;
	GList *killswitches; /* a GList of BluetoothIndKillswitch */
};

G_DEFINE_TYPE(BluetoothKillswitch, bluetooth_killswitch, G_TYPE_OBJECT)

static int
power_to_state (int power)
{
	switch (power) {
	case 1: /* RFKILL_STATE_UNBLOCKED */
		return KILLSWITCH_STATE_UNBLOCKED;
	case 0: /* RFKILL_STATE_SOFT_BLOCKED */
	case 2: /* RFKILL_STATE_HARD_BLOCKED */
		return KILLSWITCH_STATE_SOFT_BLOCKED;
	default:
		g_warning ("Unknown power state %d, please file a bug at "PACKAGE_BUGREPORT, power);
		return KILLSWITCH_STATE_UNKNOWN;
	}
}

static void
setpower_reply (DBusPendingCall *call, void *user_data)
{
	BluetoothKillswitch *killswitch = BLUETOOTH_KILLSWITCH (user_data);
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	DBusMessage *reply;
	gint32 retval;
	GList *l;

	priv->num_remaining_answers--;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &retval,
				  DBUS_TYPE_INVALID) == FALSE) {
		dbus_message_unref(reply);
		goto done;
	}

	/* Look for the killswitch */
	for (l = priv->killswitches; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		if (call != ind->call)
			continue;
		ind->call = NULL;
		if (retval == 0)
			ind->state = ind->target_state;
		break;
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref (call);

done:
	if (priv->num_remaining_answers == 0)
		g_signal_emit (G_OBJECT (killswitch),
			       signals[STATE_CHANGED],
			       0, bluetooth_killswitch_get_state (killswitch));
}

static void
getpower_reply (DBusPendingCall *call, void *user_data)
{
	BluetoothKillswitch *killswitch = BLUETOOTH_KILLSWITCH (user_data);
	BluetoothKillswitchPrivate *priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	DBusMessage *reply;
	GList *l;
	gint32 power;

	priv->num_remaining_answers--;

	reply = dbus_pending_call_steal_reply (call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &power,
				  DBUS_TYPE_INVALID) == FALSE) {
		dbus_message_unref(reply);
		goto done;
	}

	/* Look for the killswitch */
	for (l = priv->killswitches; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		if (call != ind->call)
			continue;
		ind->state = power_to_state (power);
		ind->call = NULL;
		break;
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref (call);

done:
	if (priv->num_remaining_answers == 0)
		g_signal_emit (G_OBJECT (killswitch),
			       signals[STATE_CHANGED],
			       0, bluetooth_killswitch_get_state (killswitch));
}

void
bluetooth_killswitch_update_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;
	GList *l;

	g_return_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch));

	priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);

	if (priv->num_remaining_answers > 0)
		return;

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

		priv->num_remaining_answers++;
		dbus_pending_call_set_notify (call, getpower_reply, killswitch, NULL);

		dbus_message_unref (message);
	}
}

void
bluetooth_killswitch_set_state (BluetoothKillswitch *killswitch, KillswitchState state)
{
	BluetoothKillswitchPrivate *priv;
	gboolean value;
	GList *l;

	g_return_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch));
	g_return_if_fail (state == KILLSWITCH_STATE_SOFT_BLOCKED || state == KILLSWITCH_STATE_UNBLOCKED);

	priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);

	if (priv->num_remaining_answers > 0)
		return;

	value = (state == KILLSWITCH_STATE_UNBLOCKED);

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
		ind->target_state = state;
		priv->num_remaining_answers++;

		dbus_pending_call_set_notify(call, setpower_reply, killswitch, NULL);

		dbus_message_unref(message);
	}
}

KillswitchState
bluetooth_killswitch_get_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;
	int state = KILLSWITCH_STATE_UNKNOWN;
	GList *l;

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), KILLSWITCH_STATE_UNKNOWN);

	priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	if (priv->connection == NULL)
		return KILLSWITCH_STATE_UNKNOWN;

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		if (ind->state == KILLSWITCH_STATE_SOFT_BLOCKED) {
			if (state == KILLSWITCH_STATE_UNKNOWN)
				state = KILLSWITCH_STATE_SOFT_BLOCKED;
			if (state != KILLSWITCH_STATE_SOFT_BLOCKED)
				state = KILLSWITCH_STATE_MIXED;
		} else {
			if (state == KILLSWITCH_STATE_UNKNOWN)
				state = KILLSWITCH_STATE_UNBLOCKED;
			if (state != KILLSWITCH_STATE_UNBLOCKED)
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

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), FALSE);

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
	priv->num_remaining_answers++;

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
		dbus_connection_unref (priv->connection);
		return;
	}

	if (libhal_ctx_set_dbus_connection(priv->halctx, priv->connection) == FALSE) {
		libhal_ctx_free(priv->halctx);
		dbus_connection_unref (priv->connection);
		priv->halctx = NULL;
		return;
	}

	if (libhal_ctx_init(priv->halctx, NULL) == FALSE) {
		g_printerr("Couldn't init HAL context\n");
		dbus_connection_unref (priv->connection);
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
free_ind_killswitch (BluetoothIndKillswitch *ind)
{
	g_free (ind->udi);
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

	g_list_foreach (priv->killswitches, (GFunc) free_ind_killswitch, NULL);
	g_list_free (priv->killswitches);
	priv->killswitches = NULL;

	G_OBJECT_CLASS(bluetooth_killswitch_parent_class)->finalize(object);
}

static void
bluetooth_killswitch_class_init(BluetoothKillswitchClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothKillswitchPrivate));
	object_class->finalize = bluetooth_killswitch_finalize;

	signals[STATE_CHANGED] =
		g_signal_new ("state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BluetoothKillswitchClass, state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

}

BluetoothKillswitch *
bluetooth_killswitch_new (void)
{
	return BLUETOOTH_KILLSWITCH(g_object_new (BLUETOOTH_TYPE_KILLSWITCH, NULL));
}

