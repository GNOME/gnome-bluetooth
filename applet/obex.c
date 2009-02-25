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

#include <dbus/dbus-glib.h>

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "notify.h"
#include "obex.h"

static gboolean receive_enabled = FALSE;
static gboolean sharing_enabled = FALSE;

static gboolean opp_startup = FALSE;
static gboolean ftp_startup = FALSE;

static DBusGProxy *opp_server = NULL;
static DBusGProxy *ftp_server = NULL;

static void close_opp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	if (dbus_g_proxy_end_call(proxy, call, NULL, G_TYPE_INVALID) == FALSE)
		return;

	receive_enabled = FALSE;

	g_object_unref(opp_server);
	opp_server = NULL;

	close_notification();
}

static void close_opp_server(void)
{
	if (opp_server == NULL)
		return;

	dbus_g_proxy_begin_call(opp_server, "Close",
				close_opp_notify, NULL, NULL, G_TYPE_INVALID);
}

static void close_ftp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	if (dbus_g_proxy_end_call(proxy, call, NULL, G_TYPE_INVALID) == FALSE)
		return;

	sharing_enabled = FALSE;

	g_object_unref(ftp_server);
	ftp_server = NULL;

	close_notification();
}

static void close_ftp_server(void)
{
	if (ftp_server == NULL)
		return;

	dbus_g_proxy_begin_call(ftp_server, "Close",
				close_ftp_notify, NULL, NULL, G_TYPE_INVALID);
}

static void start_opp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError *error = NULL;
	opp_startup = FALSE;

	if (dbus_g_proxy_end_call(proxy, call,
					&error, G_TYPE_INVALID) == FALSE) {
		if (error != NULL) {
			g_printerr("Bluetooth OBEX start failed: %s\n",
							error->message);
			g_error_free(error);
		}

		close_opp_server();
		return;
	}

	receive_enabled = TRUE;

	show_notification(_("File transfer"),
				_("Receiving of incoming files is enabled"),
							NULL, 4000, NULL);
}

static void create_opp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError *error = NULL;
	const gchar *dir, *path = NULL;

	if (dbus_g_proxy_end_call(proxy, call, &error,
					DBUS_TYPE_G_OBJECT_PATH, &path,
						G_TYPE_INVALID) == FALSE) {
		if (error != NULL) {
			g_printerr("Bluetooth OBEX server failed: %s\n",
							error->message);
			g_error_free(error);
		}

		opp_startup = FALSE;
		return;
	}

	dir = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
	if (dir == NULL) {
		g_printerr("G_USER_DIRECTORY_DOWNLOAD is not set\n");
		return;
	}

	opp_server = dbus_g_proxy_new_from_proxy(proxy,
					"org.openobex.Server", path);

	dbus_g_proxy_begin_call(opp_server, "Start",
				start_opp_notify, NULL, NULL,
				G_TYPE_STRING, dir,
				G_TYPE_BOOLEAN, TRUE,
				G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID);
}

static void create_opp_server(DBusGProxy *manager)
{
	opp_startup = TRUE;

	dbus_g_proxy_begin_call(manager, "CreateBluetoothServer",
				create_opp_notify, NULL, NULL,
				G_TYPE_STRING, "00:00:00:00:00:00",
				G_TYPE_STRING, "opp",
				G_TYPE_BOOLEAN, FALSE, G_TYPE_INVALID);
}

static void start_ftp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError * error = NULL;
	ftp_startup = FALSE;

	if (dbus_g_proxy_end_call(proxy, call,
					&error, G_TYPE_INVALID) == FALSE) {
		if (error != NULL) {
			g_printerr("Bluetooth FTP start failed: %s\n",
							error->message);
			g_error_free(error);
		}

		close_ftp_server();
		return;
	}

	sharing_enabled = TRUE;

	show_notification(_("File transfer"),
				_("Public file sharing has been activated"),
							NULL, 4000, NULL);
}

static void create_ftp_notify(DBusGProxy *proxy,
				DBusGProxyCall *call, void *user_data)
{
	GError *error = NULL;
	const gchar *dir, *path = NULL;

	if (dbus_g_proxy_end_call(proxy, call, &error,
					DBUS_TYPE_G_OBJECT_PATH, &path,
						G_TYPE_INVALID) == FALSE) {
		if (error != NULL) {
			g_printerr("Bluetooth FTP server failed: %s\n",
							error->message);
			g_error_free(error);
		}

		ftp_startup = FALSE;
		return;
	}

	dir = g_get_user_special_dir(G_USER_DIRECTORY_PUBLIC_SHARE);
	if (dir == NULL) {
		g_printerr("G_USER_DIRECTORY_PUBLIC_SHARE is not set\n");
		return;
	}

	ftp_server = dbus_g_proxy_new_from_proxy(proxy,
					"org.openobex.Server", path);

	dbus_g_proxy_begin_call(ftp_server, "Start",
				start_ftp_notify, NULL, NULL,
				G_TYPE_STRING, dir,
				G_TYPE_BOOLEAN, FALSE,
				G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID);
}

static void create_ftp_server(DBusGProxy *manager)
{
	ftp_startup = TRUE;

	dbus_g_proxy_begin_call(manager, "CreateBluetoothServer",
				create_ftp_notify, NULL, NULL,
				G_TYPE_STRING, "00:00:00:00:00:00",
				G_TYPE_STRING, "ftp",
				G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID);
}

static DBusGConnection *connection = NULL;
static DBusGProxy *manager = NULL;

static void name_owner_changed(DBusGProxy *object, const char *name,
			const char *prev, const char *new, gpointer user_data)
{
	if (g_str_equal(name, "org.openobex") == TRUE && *new == '\0') {
		if (opp_server != NULL) {
			g_object_unref(opp_server);
			opp_server = NULL;
		}

		if (ftp_server != NULL) {
			g_object_unref(ftp_server);
			ftp_server = NULL;
		}
	}

	if (g_str_equal(name, "org.openobex") == TRUE && *prev == '\0') {
		if (opp_startup == FALSE && receive_enabled == TRUE)
			create_opp_server(manager);

		if (ftp_startup == FALSE && sharing_enabled == TRUE)
			create_ftp_server(manager);
	}
}

int setup_obex(void)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (error != NULL) {
		g_printerr("Connecting to session bus failed: %s\n",
							error->message);
		g_error_free(error);
		return -1;
	}

	proxy = dbus_g_proxy_new_for_name(connection, DBUS_SERVICE_DBUS,
					DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal(proxy, "NameOwnerChanged",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
				G_CALLBACK(name_owner_changed), NULL, NULL);

	manager = dbus_g_proxy_new_for_name(connection, "org.openobex",
				"/org/openobex", "org.openobex.Manager");

	if (opp_startup == FALSE && receive_enabled == TRUE)
		create_opp_server(manager);

	if (ftp_startup == FALSE && sharing_enabled == TRUE)
		create_ftp_server(manager);

	return 0;
}

void cleanup_obex(void)
{
	if (receive_enabled == TRUE)
		close_opp_server();

	if (sharing_enabled == TRUE)
		close_ftp_server();

	if (manager != NULL)
		g_object_unref(manager);

	if (connection != NULL)
		dbus_g_connection_unref(connection);
}

void set_receive_enabled(gboolean value)
{
	if (receive_enabled == value)
		return;

	if (manager == NULL) {
		receive_enabled = value;
		return;
	}

	if (value == TRUE)
		create_opp_server(manager);
	else
		close_opp_server();
}

void set_sharing_enabled(gboolean value)
{
	if (sharing_enabled == value)
		return;

	if (manager == NULL) {
		sharing_enabled = value;
		return;
	}

	if (value == TRUE)
		create_ftp_server(manager);
	else
		close_ftp_server();
}
