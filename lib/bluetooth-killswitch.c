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

/**
 * SECTION:bluetooth-killswitch
 * @short_description: a Bluetooth killswitch object
 * @stability: Stable
 * @include: bluetooth-killswitch.h
 *
 * An object to manipulate Bluetooth killswitches.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <glib.h>

#include "rfkill.h"

#ifndef RFKILL_EVENT_SIZE_V1
#define RFKILL_EVENT_SIZE_V1	8
#endif

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
	guint index;
	BluetoothKillswitchState state;
};

struct _BluetoothKillswitchPrivate {
	int fd;
	GIOChannel *channel;
	guint watch_id;
	GList *killswitches; /* a GList of BluetoothIndKillswitch */
	BluetoothKillswitchPrivate *priv;
};

G_DEFINE_TYPE(BluetoothKillswitch, bluetooth_killswitch, G_TYPE_OBJECT)

static BluetoothKillswitchState
event_to_state (guint soft, guint hard)
{
	if (hard)
		return BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED;
	else if (soft)
		return BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED;
	else
		return BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
}

static const char *
state_to_string (BluetoothKillswitchState state)
{
	switch (state) {
	case BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER:
		return "no-adapter";
	case BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED:
		return "soft-blocked";
	case BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED:
		return "unblocked";
	case BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED:
		return "hard-blocked";
	default:
		g_assert_not_reached ();
	}
}

const char *
bluetooth_killswitch_state_to_string (BluetoothKillswitchState state)
{
	g_return_val_if_fail (state >= BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER, NULL);
	g_return_val_if_fail (state <= BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED, NULL);

	return state_to_string (state);
}

static void
update_killswitch (BluetoothKillswitch *killswitch,
		   guint index, guint soft, guint hard)
{
	BluetoothKillswitchPrivate *priv;
	gboolean changed;
	GList *l;

	priv = killswitch->priv;
	changed = FALSE;

	for (l = priv->killswitches; l != NULL; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		if (ind->index == index) {
			BluetoothKillswitchState state = event_to_state (soft, hard);
			if (state != ind->state) {
				ind->state = state;
				changed = TRUE;
			}
			break;
		}
	}

	if (changed != FALSE) {
		g_debug ("updating killswitch status %d to %s",
			 index, state_to_string (bluetooth_killswitch_get_state (killswitch)));
		g_signal_emit (G_OBJECT (killswitch),
			       signals[STATE_CHANGED],
			       0, bluetooth_killswitch_get_state (killswitch));
	}
}

void
bluetooth_killswitch_set_state (BluetoothKillswitch *killswitch,
				BluetoothKillswitchState state)
{
	BluetoothKillswitchPrivate *priv;
	struct rfkill_event event;
	ssize_t len;

	g_return_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch));
	g_return_if_fail (state != BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED);

	priv = killswitch->priv;

	memset (&event, 0, sizeof(event));
	event.op = RFKILL_OP_CHANGE_ALL;
	event.type = RFKILL_TYPE_BLUETOOTH;
	if (state == BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED)
		event.soft = 1;
	else if (state == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED)
		event.soft = 0;
	else
		g_assert_not_reached ();

	len = write (priv->fd, &event, sizeof(event));
	if (len < 0)
		g_warning ("Failed to change RFKILL state: %s",
			   g_strerror (errno));
}

BluetoothKillswitchState
bluetooth_killswitch_get_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;
	int state = BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
	GList *l;

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), state);

	priv = killswitch->priv;

	if (priv->killswitches == NULL)
		return BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER;

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		g_debug ("killswitch %d is %s",
			 ind->index, state_to_string (ind->state));

		if (ind->state == BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED) {
			state = BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED;
			break;
		}

		if (ind->state == BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED) {
			state = BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED;
			continue;
		}

		state = ind->state;
	}

	g_debug ("killswitches state %s", state_to_string (state));

	return state;
}

gboolean
bluetooth_killswitch_has_killswitches (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = killswitch->priv;

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), FALSE);

	return (priv->killswitches != NULL);
}

static void
remove_killswitch (BluetoothKillswitch *killswitch,
		   guint index)
{
	BluetoothKillswitchPrivate *priv;
	GList *l;

	priv = killswitch->priv;

	for (l = priv->killswitches; l != NULL; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		if (ind->index == index) {
			priv->killswitches = g_list_remove (priv->killswitches, ind);
			g_debug ("removing killswitch idx %d", index);
			g_free (ind);
			g_signal_emit (G_OBJECT (killswitch),
				       signals[STATE_CHANGED],
				       0, bluetooth_killswitch_get_state (killswitch));
			return;
		}
	}
}

static void
add_killswitch (BluetoothKillswitch *killswitch,
		guint index,
		BluetoothKillswitchState state)

{
	BluetoothKillswitchPrivate *priv;
	BluetoothIndKillswitch *ind;

	priv = killswitch->priv;

	g_debug ("adding killswitch idx %d state %s", index, state_to_string (state));
	ind = g_new0 (BluetoothIndKillswitch, 1);
	ind->index = index;
	ind->state = state;
	priv->killswitches = g_list_append (priv->killswitches, ind);
}

static const char *
type_to_string (unsigned int type)
{
	switch (type) {
	case RFKILL_TYPE_ALL:
		return "ALL";
	case RFKILL_TYPE_WLAN:
		return "WLAN";
	case RFKILL_TYPE_BLUETOOTH:
		return "BLUETOOTH";
	case RFKILL_TYPE_UWB:
		return "UWB";
	case RFKILL_TYPE_WIMAX:
		return "WIMAX";
	case RFKILL_TYPE_WWAN:
		return "WWAN";
	default:
		g_assert_not_reached ();
	}
}

static const char *
op_to_string (unsigned int op)
{
	switch (op) {
	case RFKILL_OP_ADD:
		return "ADD";
	case RFKILL_OP_DEL:
		return "DEL";
	case RFKILL_OP_CHANGE:
		return "CHANGE";
	case RFKILL_OP_CHANGE_ALL:
		return "CHANGE_ALL";
	default:
		g_assert_not_reached ();
	}
}

static void
print_event (struct rfkill_event *event)
{
	g_debug ("RFKILL event: idx %u type %u (%s) op %u (%s) soft %u hard %u",
		 event->idx,
		 event->type, type_to_string (event->type),
		 event->op, op_to_string (event->op),
		 event->soft, event->hard);
}

static gboolean
event_cb (GIOChannel *source,
	  GIOCondition condition,
	  BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchState state;
	gboolean changed;

	changed = FALSE;

	/* Gather the previous killswitch so we can
	 * see what really changed */
	state = bluetooth_killswitch_get_state (killswitch);

	if (condition & G_IO_IN) {
		GIOStatus status;
		struct rfkill_event event;
		gsize read;

		status = g_io_channel_read_chars (source,
						  (char *) &event,
						  sizeof(event),
						  &read,
						  NULL);

		while (status == G_IO_STATUS_NORMAL && read == sizeof(event)) {
			if (event.type != RFKILL_TYPE_BLUETOOTH &&
			    event.type != RFKILL_TYPE_ALL)
				goto carry_on;

			print_event (&event);

			if (event.op == RFKILL_OP_CHANGE) {
				update_killswitch (killswitch, event.idx, event.soft, event.hard);
				/* No changed here because update_killswitch
				 * handles sending the state-changed signal */
			} else if (event.op == RFKILL_OP_DEL) {
				remove_killswitch (killswitch, event.idx);
				changed = TRUE;
			} else if (event.op == RFKILL_OP_ADD) {
				BluetoothKillswitchState state;
				state = event_to_state (event.soft, event.hard);
				add_killswitch (killswitch, event.idx, state);
				changed = TRUE;
			}

carry_on:
			status = g_io_channel_read_chars (source,
							  (char *) &event,
							  sizeof(event),
							  &read,
							  NULL);
		}
	} else {
		g_debug ("something else happened");
		return FALSE;
	}

	if (changed) {
		BluetoothKillswitchState new_state;

		new_state = bluetooth_killswitch_get_state (killswitch);
		if (new_state != state) {
			g_signal_emit (G_OBJECT (killswitch),
				       signals[STATE_CHANGED],
				       0, bluetooth_killswitch_get_state (killswitch));
		}
	}

	return TRUE;
}

static void
bluetooth_killswitch_init (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;
	struct rfkill_event event;
	int fd;

	priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	killswitch->priv = priv;

	fd = open("/dev/rfkill", O_RDWR);
	if (fd < 0) {
		if (errno == EACCES)
			g_warning ("Could not open RFKILL control device, please verify your installation");
		return;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		g_debug ("Can't set RFKILL control device to non-blocking");
		close(fd);
		return;
	}

	while (1) {
		BluetoothKillswitchState state;
		ssize_t len;

		len = read(fd, &event, sizeof(event));
		if (len < 0) {
			if (errno == EAGAIN)
				break;
			g_debug ("Reading of RFKILL events failed");
			break;
		}

		if (len != RFKILL_EVENT_SIZE_V1) {
			g_warning ("Wrong size of RFKILL event\n");
			continue;
		}

		if (event.op != RFKILL_OP_ADD)
			continue;
		state = event_to_state (event.soft, event.hard);

		g_debug ("Read killswitch of type '%s' (idx=%d): %s",
			 type_to_string (event.type),
			 event.idx, bluetooth_killswitch_state_to_string (state));

		if (event.type != RFKILL_TYPE_BLUETOOTH)
			continue;

		add_killswitch (killswitch, event.idx, state);
	}

	/* Setup monitoring */
	priv->fd = fd;
	priv->channel = g_io_channel_unix_new (priv->fd);
	priv->watch_id = g_io_add_watch (priv->channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR,
				(GIOFunc) event_cb,
				killswitch);

	g_signal_emit (G_OBJECT (killswitch),
		       signals[STATE_CHANGED],
		       0, bluetooth_killswitch_get_state (killswitch));
}

static void
bluetooth_killswitch_finalize (GObject *object)
{
	BluetoothKillswitch *killswitch;
	BluetoothKillswitchPrivate *priv;

	killswitch = BLUETOOTH_KILLSWITCH (object);
	priv = killswitch->priv;

	/* cleanup monitoring */
	if (priv->watch_id > 0) {
		g_source_remove (priv->watch_id);
		priv->watch_id = 0;
		g_io_channel_shutdown (priv->channel, FALSE, NULL);
		g_io_channel_unref (priv->channel);
	}
	close(priv->fd);

	g_list_foreach (priv->killswitches, (GFunc) g_free, NULL);
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

