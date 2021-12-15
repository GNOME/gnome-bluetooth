/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#include <stdio.h>
#include <gio/gio.h>

#include "bluetooth-client-glue.h"
#include "bluetooth-agent.h"

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_AGENT_PATH		"/org/bluez/agent/gnome"
#define BLUEZ_MANAGER_PATH		"/"

static const gchar introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.Agent1'>"
"    <method name='Release'/>"
"    <method name='RequestPinCode'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='pincode' direction='out'/>"
"    </method>"
"    <method name='RequestPasskey'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='out'/>"
"    </method>"
"    <method name='DisplayPasskey'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='in'/>"
"      <arg type='q' name='entered' direction='in'/>"
"    </method>"
"    <method name='DisplayPinCode'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='pincode' direction='in'/>"
"    </method>"
"    <method name='RequestConfirmation'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='in'/>"
"    </method>"
"    <method name='RequestAuthorization'>"
"      <arg type='o' name='device' direction='in'/>"
"    </method>"
"    <method name='AuthorizeService'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='uuid' direction='in'/>"
"    </method>"
"    <method name='Cancel'/>"
"  </interface>"
"</node>";

struct _BluetoothAgent {
	GObject parent;

	GDBusConnection *conn;
	gchar *busname;
	gchar *path;
	AgentManager1 *agent_manager;
	GDBusNodeInfo *introspection_data;
	guint reg_id;
	guint watch_id;

	BluetoothAgentPasskeyFunc pincode_func;
	gpointer pincode_data;

	BluetoothAgentDisplayPasskeyFunc display_passkey_func;
	gpointer display_passkey_data;

	BluetoothAgentDisplayPinCodeFunc display_pincode_func;
	gpointer display_pincode_data;

	BluetoothAgentPasskeyFunc passkey_func;
	gpointer passkey_data;

	BluetoothAgentConfirmFunc confirm_func;
	gpointer confirm_data;

	BluetoothAgentAuthorizeFunc authorize_func;
	gpointer authorize_data;

	BluetoothAgentAuthorizeServiceFunc authorize_service_func;
	gpointer authorize_service_data;

	BluetoothAgentCancelFunc cancel_func;
	gpointer cancel_data;
};

G_DEFINE_TYPE(BluetoothAgent, bluetooth_agent, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PATH,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST];

static GDBusProxy *
get_device_from_path (BluetoothAgent *agent,
		      const char     *path)
{
	Device1 *device;

	device = device1_proxy_new_sync (agent->conn,
					 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
					 BLUEZ_SERVICE,
					 path,
					 NULL,
					 NULL);

	return G_DBUS_PROXY(device);
}

static gboolean bluetooth_agent_request_pincode(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->pincode_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->pincode_func(invocation, device, agent->pincode_data);

	return TRUE;
}

static gboolean bluetooth_agent_request_passkey(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->passkey_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->passkey_func(invocation, device, agent->passkey_data);

	return TRUE;
}

static gboolean bluetooth_agent_display_passkey(BluetoothAgent *agent,
			const char *path, guint passkey, guint16 entered,
						GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->display_passkey_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->display_passkey_func(invocation, device, passkey, entered,
			            agent->display_passkey_data);

	return TRUE;
}

static gboolean bluetooth_agent_display_pincode(BluetoothAgent *agent,
						const char *path, const char *pincode,
						GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->display_pincode_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->display_pincode_func(invocation, device, pincode,
				    agent->display_pincode_data);

	return TRUE;
}

static gboolean bluetooth_agent_request_confirmation(BluetoothAgent *agent,
					const char *path, guint passkey,
						GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->confirm_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->confirm_func(invocation, device, passkey, agent->confirm_data);

	return TRUE;
}

static gboolean bluetooth_agent_request_authorization(BluetoothAgent *agent,
					const char *path, GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->authorize_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->authorize_func(invocation, device, agent->authorize_data);

	return TRUE;
}

static gboolean bluetooth_agent_authorize_service(BluetoothAgent *agent,
					const char *path, const char *uuid,
						GDBusMethodInvocation *invocation)
{
	g_autoptr(GDBusProxy) device = NULL;

	if (agent->authorize_service_func == NULL)
		return FALSE;

	device = get_device_from_path (agent, path);
	if (device == NULL)
		return FALSE;

	agent->authorize_service_func(invocation, device, uuid,
					    agent->authorize_service_data);

	return TRUE;
}

static gboolean bluetooth_agent_cancel(BluetoothAgent *agent,
						GDBusMethodInvocation *invocation)
{
	if (agent->cancel_func == NULL)
		return FALSE;

	return agent->cancel_func(invocation, agent->cancel_data);
}

static void
register_agent (BluetoothAgent *agent)
{
	g_autoptr(GError) error = NULL;
	gboolean ret;

	ret = agent_manager1_call_register_agent_sync (agent->agent_manager,
						       agent->path,
						       "DisplayYesNo",
						       NULL, &error);
	if (ret == FALSE) {
		g_printerr ("Agent registration failed: %s\n", error->message);
		return;
	}

	ret = agent_manager1_call_request_default_agent_sync (agent->agent_manager,
							      agent->path,
							      NULL, &error);
	if (ret == FALSE)
		g_printerr ("Agent registration as default failed: %s\n", error->message);
}

static void
name_appeared_cb (GDBusConnection *connection,
		  const gchar     *name,
		  const gchar     *name_owner,
		  BluetoothAgent  *agent)
{
	g_free (agent->busname);
	agent->busname = g_strdup (name_owner);

	agent->agent_manager = agent_manager1_proxy_new_sync (agent->conn,
							      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
							      BLUEZ_SERVICE,
							      "/org/bluez",
							      NULL,
							      NULL);

	if (agent->reg_id > 0)
		register_agent (agent);
}

static void
name_vanished_cb (GDBusConnection *connection,
		  const gchar     *name,
		  BluetoothAgent  *agent)
{
	g_clear_pointer (&agent->busname, g_free);
	g_clear_object (&agent->agent_manager);
}

static void
bluetooth_agent_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	BluetoothAgent *agent = BLUETOOTH_AGENT (object);

	switch (prop_id) {
	case PROP_PATH:
		g_value_set_string (value, agent->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
bluetooth_agent_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	BluetoothAgent *agent = BLUETOOTH_AGENT (object);

	switch (prop_id) {
	case PROP_PATH:
		agent->path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void bluetooth_agent_init(BluetoothAgent *agent)
{
	agent->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (agent->introspection_data);
	agent->conn = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
	agent->watch_id = g_bus_watch_name_on_connection (agent->conn,
							  BLUEZ_SERVICE,
							  G_BUS_NAME_WATCHER_FLAGS_NONE,
							  (GBusNameAppearedCallback) name_appeared_cb,
							  (GBusNameVanishedCallback) name_vanished_cb,
							  agent,
							  NULL);
}

static void bluetooth_agent_finalize(GObject *object)
{
	BluetoothAgent *agent = BLUETOOTH_AGENT (object);

	bluetooth_agent_unregister (agent);

	g_clear_pointer (&agent->path, g_free);
	g_bus_unwatch_name (agent->watch_id);
	g_free (agent->busname);
	g_dbus_node_info_unref (agent->introspection_data);
	g_object_unref (agent->conn);

	G_OBJECT_CLASS(bluetooth_agent_parent_class)->finalize(object);
}

static void bluetooth_agent_class_init(BluetoothAgentClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->finalize = bluetooth_agent_finalize;
	object_class->set_property = bluetooth_agent_set_property;
	object_class->get_property = bluetooth_agent_get_property;

	props[PROP_PATH] =
		g_param_spec_string ("path", "Path",
				     "Object path for the agent",
				     BLUEZ_AGENT_PATH,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class,
					   PROP_LAST,
					   props);

}

BluetoothAgent *
bluetooth_agent_new (const char *path)
{
	if (path != NULL)
		return BLUETOOTH_AGENT (g_object_new (BLUETOOTH_TYPE_AGENT,
						      "path", path,
						      NULL));
	else
		return BLUETOOTH_AGENT (g_object_new (BLUETOOTH_TYPE_AGENT,
						      NULL));
}

static void
handle_method_call (GDBusConnection       *connection,
		    const gchar           *sender,
		    const gchar           *object_path,
		    const gchar           *interface_name,
		    const gchar           *method_name,
		    GVariant              *parameters,
		    GDBusMethodInvocation *invocation,
		    gpointer               user_data)
{
	BluetoothAgent *agent = (BluetoothAgent *) user_data;

	if (g_str_equal (sender, agent->busname) == FALSE) {
		GError *error;
		error = g_error_new (BLUETOOTH_AGENT_ERROR, BLUETOOTH_AGENT_ERROR_REJECT,
				     "Permission Denied");
		g_dbus_method_invocation_take_error(invocation, error);
		return;
	}

	if (g_strcmp0 (method_name, "Release") == 0) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "RequestPinCode") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_pincode (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "RequestPasskey") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_passkey (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "DisplayPasskey") == 0) {
		const char *path;
		guint32 passkey;
		guint16 entered;

		g_variant_get (parameters, "(&ouq)", &path, &passkey, &entered);
		bluetooth_agent_display_passkey (agent, path, passkey, entered, invocation);
	} else if (g_strcmp0 (method_name, "DisplayPinCode") == 0) {
		const char *path;
		const char *pincode;

		g_variant_get (parameters, "(&o&s)", &path, &pincode);
		bluetooth_agent_display_pincode (agent, path, pincode, invocation);
	} else if (g_strcmp0 (method_name, "RequestConfirmation") == 0) {
		const char *path;
		guint32 passkey;

		g_variant_get (parameters, "(&ou)", &path, &passkey);
		bluetooth_agent_request_confirmation (agent, path, passkey, invocation);
	} else if (g_strcmp0 (method_name, "RequestAuthorization") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_authorization (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "AuthorizeService") == 0) {
		const char *path;
		const char *uuid;

		g_variant_get (parameters, "(&o&s)", &path, &uuid);
		bluetooth_agent_authorize_service (agent, path, uuid, invocation);
	} else if (g_strcmp0 (method_name, "Cancel") == 0) {
		bluetooth_agent_cancel (agent, invocation);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
	NULL, /* GetProperty */
	NULL, /* SetProperty */
};

gboolean bluetooth_agent_register(BluetoothAgent *agent)
{
	g_autoptr(GError) error = NULL;

	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);
	g_return_val_if_fail (agent->path != NULL, FALSE);
	g_return_val_if_fail (g_variant_is_object_path(agent->path), FALSE);

	agent->reg_id = g_dbus_connection_register_object (agent->conn,
							   agent->path,
							   agent->introspection_data->interfaces[0],
							   &interface_vtable,
							   agent,
							   NULL,
							   &error);
	if (agent->reg_id == 0) {
		g_warning ("Failed to register object: %s", error->message);
		return FALSE;
	}

	if (agent->agent_manager != NULL)
		register_agent (agent);

	return TRUE;
}

static void
agent_unregister_cb (GObject      *object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
	g_autoptr(GError) error = NULL;

	if (!agent_manager1_call_unregister_agent_finish (AGENT_MANAGER1 (object), res, &error))
		g_debug ("Failed to unregister agent: %s", error->message);
	else
		g_debug ("Unregistered agent successfully");
}

gboolean bluetooth_agent_unregister(BluetoothAgent *agent)
{
	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);

	if (agent->agent_manager == NULL) {
		g_debug ("AgentManager not registered yet");
		return FALSE;
	}

	agent_manager1_call_unregister_agent (agent->agent_manager,
					      agent->path,
					      NULL, agent_unregister_cb, NULL);

	g_clear_object (&agent->agent_manager);
	g_clear_pointer (&agent->busname, g_free);

	if (agent->reg_id > 0) {
		g_dbus_connection_unregister_object (agent->conn, agent->reg_id);
		agent->reg_id = 0;
	}

	return TRUE;
}

void bluetooth_agent_set_pincode_func(BluetoothAgent *agent,
				BluetoothAgentPasskeyFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->pincode_func = func;
	agent->pincode_data = data;
}

void bluetooth_agent_set_passkey_func(BluetoothAgent *agent,
				BluetoothAgentPasskeyFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->passkey_func = func;
	agent->passkey_data = data;
}

void bluetooth_agent_set_display_passkey_func(BluetoothAgent *agent,
				BluetoothAgentDisplayPasskeyFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->display_passkey_func = func;
	agent->display_passkey_data = data;
}

void bluetooth_agent_set_display_pincode_func(BluetoothAgent *agent,
				BluetoothAgentDisplayPinCodeFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->display_pincode_func = func;
	agent->display_pincode_data = data;
}

void bluetooth_agent_set_confirm_func(BluetoothAgent *agent,
				BluetoothAgentConfirmFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->confirm_func = func;
	agent->confirm_data = data;
}

void bluetooth_agent_set_authorize_func(BluetoothAgent *agent,
				BluetoothAgentAuthorizeFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->authorize_func = func;
	agent->authorize_data = data;
}

void bluetooth_agent_set_authorize_service_func(BluetoothAgent *agent,
				BluetoothAgentAuthorizeServiceFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->authorize_service_func = func;
	agent->authorize_service_data = data;
}

void bluetooth_agent_set_cancel_func(BluetoothAgent *agent,
				BluetoothAgentCancelFunc func, gpointer data)
{
	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	agent->cancel_func = func;
	agent->cancel_data = data;
}

GQuark bluetooth_agent_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType bluetooth_agent_error_get_type(void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY(BLUETOOTH_AGENT_ERROR_REJECT, "Rejected"),
			{ 0, 0, 0 }
		};

		etype = g_enum_register_static("BluetoothAgentError", values);
	}

	return etype;
}

