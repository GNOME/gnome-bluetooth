/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "bluetooth-agent.h"

static void
agent_confirm (GDBusMethodInvocation *invocation,
	       GDBusProxy *device,
	       guint passkey,
	       gpointer data)
{
	const char *path;

	path = g_dbus_proxy_get_object_path(device);

	g_print ("Confirming passkey %6.6d from %s\n", passkey, path);

	g_dbus_method_invocation_return_value (invocation, NULL);
}

static GMainLoop *mainloop = NULL;

static void
sig_term (int sig)
{
	g_main_loop_quit(mainloop);
}

int main (int argc, char **argv)
{
	struct sigaction sa;
	BluetoothAgent *agent;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);

	mainloop = g_main_loop_new(NULL, FALSE);

	agent = bluetooth_agent_new(NULL);

	bluetooth_agent_set_confirm_func(agent, agent_confirm, NULL);

	bluetooth_agent_register(agent);

	g_main_loop_run(mainloop);

	bluetooth_agent_unregister(agent);

	g_object_unref(agent);

	g_main_loop_unref(mainloop);

	return 0;
}
