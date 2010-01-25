/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2009  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2010       Intel Corp
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

/**
 * SECTION:bluetooth-powerswitch
 * @short_description: a Bluetooth power switch object
 * @stability: Stable
 * @include: bluetooth-powerswitch.h
 *
 * An object to manipulate Bluetooth power switches.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "bluetooth-client.h"
#include "bluetooth-powerswitch.h"

enum {
	STATE_CHANGED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

#define BLUETOOTH_POWERSWITCH_GET_PRIVATE(obj)				\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), BLUETOOTH_TYPE_POWERSWITCH, BluetoothPowerswitchPrivate))

struct _BluetoothPowerswitchPrivate {
	BluetoothClient *client;
	GtkTreeModel *adapters;
};

G_DEFINE_TYPE (BluetoothPowerswitch, bluetooth_powerswitch, G_TYPE_OBJECT)

static gboolean
set_state_foreach (GtkTreeModel *model,
		   GtkTreePath	*path,
		   GtkTreeIter	*iter,
		   gpointer	 data)
{
	DBusGProxy *proxy = NULL;
	GValue value = { 0, };
	PowerswitchState state = GPOINTER_TO_INT (data);

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_PROXY, &proxy, -1);
	if (proxy == NULL)
		return FALSE;

	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, state);

	/* TODO: do this async to avoid blocking. Maybe _no_reply will do? */
	dbus_g_proxy_call (proxy, "SetProperty", NULL,
			   G_TYPE_STRING, "Powered",
			   G_TYPE_VALUE, &value,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);

	g_value_unset (&value);
	g_object_unref (proxy);

	return FALSE;
}

void
bluetooth_powerswitch_set_state (BluetoothPowerswitch *powerswitch,
				PowerswitchState state)
{
	BluetoothPowerswitchPrivate *priv = BLUETOOTH_POWERSWITCH_GET_PRIVATE (powerswitch);

	g_return_if_fail (state == POWERSWITCH_STATE_OFF ||
			  state == POWERSWITCH_STATE_ON);

	gtk_tree_model_foreach (priv->adapters, set_state_foreach, GINT_TO_POINTER (state));
}

static gboolean
get_state_foreach (GtkTreeModel *model,
		   GtkTreePath	*path,
		   GtkTreeIter	*iter,
		   gpointer	 data)
{
	gboolean powered = FALSE;
	gboolean *global_powered = data;

	gtk_tree_model_get (model, iter, BLUETOOTH_COLUMN_POWERED, &powered, -1);

	if (powered) {
		/* Found a powered adaptor, so we are "on" */
		*global_powered = TRUE;
		return TRUE;
	} else {
		/* Found an unpowered adaptor, set "off" but continue looking */
		*global_powered = FALSE;
		return FALSE;
	}
}

PowerswitchState
bluetooth_powerswitch_get_state (BluetoothPowerswitch *powerswitch)
{
	BluetoothPowerswitchPrivate *priv;
	gboolean powered = FALSE;

	g_return_val_if_fail (BLUETOOTH_IS_POWERSWITCH (powerswitch), POWERSWITCH_STATE_NO_ADAPTER);

	priv = BLUETOOTH_POWERSWITCH_GET_PRIVATE (powerswitch);

	if (bluetooth_powerswitch_has_powerswitches (powerswitch)) {
		gtk_tree_model_foreach (priv->adapters, get_state_foreach, &powered);
		return powered;
	} else {
		return POWERSWITCH_STATE_NO_ADAPTER;
	}
}

gboolean
bluetooth_powerswitch_has_powerswitches (BluetoothPowerswitch *powerswitch)
{
	BluetoothPowerswitchPrivate *priv = BLUETOOTH_POWERSWITCH_GET_PRIVATE (powerswitch);

	g_return_val_if_fail (BLUETOOTH_IS_POWERSWITCH (powerswitch), FALSE);

	return gtk_tree_model_iter_n_children (priv->adapters, NULL);
}

static void
powered_notify_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
	BluetoothPowerswitch *powerswitch = BLUETOOTH_POWERSWITCH (user_data);

	g_signal_emit (G_OBJECT (powerswitch),
		       signals[STATE_CHANGED],
		       0, bluetooth_powerswitch_get_state (powerswitch));
}

static void
bluetooth_powerswitch_init (BluetoothPowerswitch *powerswitch)
{
	BluetoothPowerswitchPrivate *priv = BLUETOOTH_POWERSWITCH_GET_PRIVATE (powerswitch);

	powerswitch->priv = priv;

	priv->client = bluetooth_client_new ();
	priv->adapters = bluetooth_client_get_adapter_model(priv->client);

	g_signal_connect (priv->client, "notify::default-adapter-powered",
			  G_CALLBACK (powered_notify_cb), powerswitch);

	g_signal_emit (G_OBJECT (powerswitch),
		       signals[STATE_CHANGED],
		       0, bluetooth_powerswitch_get_state (powerswitch));
}

static void
bluetooth_powerswitch_dispose (GObject *object)
{
	BluetoothPowerswitchPrivate *priv = BLUETOOTH_POWERSWITCH_GET_PRIVATE (object);

	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
	}

	G_OBJECT_CLASS(bluetooth_powerswitch_parent_class)->dispose(object);
}

static void
bluetooth_powerswitch_class_init(BluetoothPowerswitchClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothPowerswitchPrivate));
	object_class->dispose = bluetooth_powerswitch_dispose;

	signals[STATE_CHANGED] =
		g_signal_new ("state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BluetoothPowerswitchClass, state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

}

BluetoothPowerswitch *
bluetooth_powerswitch_new (void)
{
	return BLUETOOTH_POWERSWITCH(g_object_new (BLUETOOTH_TYPE_POWERSWITCH, NULL));
}
