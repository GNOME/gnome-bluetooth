/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
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
#include <gtk/gtk.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <hal/libhal.h>

#include "general.h"
#include "killswitch.h"

static LibHalContext *halctx = NULL;

static void setpower_reply(DBusPendingCall *call, void *user_data)
{
	GtkWidget *button = user_data;
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

	gtk_widget_set_sensitive(button, TRUE);
}

static void toggle_callback(GtkWidget *button, gpointer user_data)
{
	DBusConnection *conn;
	DBusPendingCall *call;
	DBusMessage *message;
	const char *udi;
	gboolean value;

	value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

	gtk_widget_set_sensitive(button, FALSE);

	udi = g_object_get_data(G_OBJECT(button), "UDI");

	message = dbus_message_new_method_call("org.freedesktop.Hal", udi,
				"org.freedesktop.Hal.Device.KillSwitch",
								"SetPower");
	if (message == NULL)
		return;

	dbus_message_append_args(message, DBUS_TYPE_BOOLEAN, &value,
							DBUS_TYPE_INVALID);

	conn = libhal_ctx_get_dbus_connection(halctx);

	if (dbus_connection_send_with_reply(conn, message,
						&call, -1) == FALSE) {
		dbus_message_unref(message);
		return;
	}

	dbus_pending_call_set_notify(call, setpower_reply, button, NULL);

	dbus_message_unref(message);
}

static void getpower_reply(DBusPendingCall *call, void *user_data)
{
	GtkWidget *button = user_data;
	DBusMessage *reply;
	gint32 power;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &power,
						DBUS_TYPE_INVALID) == FALSE) {
		dbus_message_unref(reply);
		return;
	}

	if (power > 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	dbus_message_unref(reply);

	gtk_widget_set_sensitive(button, TRUE);
}

static gboolean add_killswitch(GtkWidget *vbox, const char *udi)
{
	DBusConnection *conn;
	DBusPendingCall *call;
	DBusMessage *message;
	GtkWidget *button;
	char *type, *name;

	type = libhal_device_get_property_string(halctx, udi,
						"killswitch.type", NULL);
	if (type == NULL || g_str_equal(type, "bluetooth") == FALSE) {
		g_free (type);
		return FALSE;
	}
	g_free (type);

	name = libhal_device_get_property_string(halctx, udi,
							"info.product", NULL);

	button = gtk_check_button_new_with_label(name ? name : _("Bluetooth Switch"));
	g_free (name);
	g_object_set_data_full(G_OBJECT(button), "UDI", g_strdup(udi), (GDestroyNotify) g_free);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "toggled",
					G_CALLBACK(toggle_callback), NULL);

	message = dbus_message_new_method_call("org.freedesktop.Hal", udi,
				"org.freedesktop.Hal.Device.KillSwitch",
								"GetPower");
	if (message == NULL)
		return TRUE;

	conn = libhal_ctx_get_dbus_connection(halctx);

	if (dbus_connection_send_with_reply(conn, message,
						&call, -1) == FALSE) {
		dbus_message_unref(message);
		return TRUE;
	}

	dbus_pending_call_set_notify(call, getpower_reply, button, NULL);

	dbus_message_unref(message);

	return TRUE;
}

static gboolean detect_killswitch(GtkWidget *vbox)
{
	gboolean result = FALSE;
	char **list;
	int num;

	list = libhal_find_device_by_capability(halctx, "killswitch",
								&num, NULL);
	if (list) {
		char **tmp = list;

		while (*tmp) {
			if (add_killswitch(vbox, *tmp) == TRUE)
				result = TRUE;
			tmp++;
		}

		libhal_free_string_array(list);
	}

	return result;
}

GtkWidget *create_killswitch(void)
{
	GtkWidget *vbox;
	GtkWidget *label;

	vbox = gtk_vbox_new(FALSE, 6);

	if (detect_killswitch(vbox) == TRUE) {
		label = create_label(_("Power switches"));
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	}

	return vbox;
}

static DBusConnection *connection;

void setup_killswitch(void)
{
	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return;

	halctx = libhal_ctx_new();
	if (halctx == NULL)
		return;

	if (libhal_ctx_set_dbus_connection(halctx, connection) == FALSE) {
		libhal_ctx_free(halctx);
		halctx = NULL;
		return;
	}

	if (libhal_ctx_init(halctx, NULL) == FALSE) {
		g_printerr("Couldn't init HAL context\n");
		libhal_ctx_free(halctx);
		halctx = NULL;
		return;
	}
}

void cleanup_killswitch(void)
{
	if (halctx != NULL) {
		libhal_ctx_shutdown(halctx, NULL);
		libhal_ctx_free(halctx);
		halctx = NULL;
	}

	if (connection != NULL)
		dbus_connection_unref(connection);
}
