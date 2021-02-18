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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_DECLARE_FINAL_TYPE(BluetoothAgent, bluetooth_agent, BLUETOOTH, AGENT, GObject)
#define BLUETOOTH_TYPE_AGENT (bluetooth_agent_get_type())

BluetoothAgent *bluetooth_agent_new(const char *path);

gboolean bluetooth_agent_register(BluetoothAgent *agent);
gboolean bluetooth_agent_unregister(BluetoothAgent *agent);

typedef void (*BluetoothAgentPasskeyFunc) (GDBusMethodInvocation *invocation,
					   GDBusProxy            *device,
					   gpointer               data);
typedef void (*BluetoothAgentDisplayPasskeyFunc) (GDBusMethodInvocation *invocation,
					          GDBusProxy            *device,
					          guint                  passkey,
					          guint                  entered,
					          gpointer               data);
typedef void (*BluetoothAgentDisplayPinCodeFunc) (GDBusMethodInvocation *invocation,
						  GDBusProxy            *device,
						  const char            *pincode,
						  gpointer               data);
typedef void (*BluetoothAgentConfirmFunc) (GDBusMethodInvocation *invocation,
					   GDBusProxy            *device,
					   guint                  passkey,
					   gpointer               data);
typedef void (*BluetoothAgentAuthorizeFunc) (GDBusMethodInvocation *invocation,
					     GDBusProxy            *device,
					     gpointer               data);
typedef void (*BluetoothAgentAuthorizeServiceFunc) (GDBusMethodInvocation *invocation,
						    GDBusProxy            *device,
						    const char            *uuid,
						    gpointer               data);
typedef gboolean (*BluetoothAgentCancelFunc) (GDBusMethodInvocation *invocation,
					      gpointer               data);

void bluetooth_agent_set_pincode_func (BluetoothAgent            *agent,
				       BluetoothAgentPasskeyFunc  func,
				       gpointer                   data);
void bluetooth_agent_set_passkey_func (BluetoothAgent            *agent,
				       BluetoothAgentPasskeyFunc  func,
				       gpointer                   data);
void bluetooth_agent_set_display_passkey_func (BluetoothAgent                   *agent,
					       BluetoothAgentDisplayPasskeyFunc  func,
					       gpointer                          data);
void bluetooth_agent_set_display_pincode_func (BluetoothAgent                   *agent,
					       BluetoothAgentDisplayPinCodeFunc  func,
					       gpointer                          data);
void bluetooth_agent_set_confirm_func (BluetoothAgent            *agent,
				       BluetoothAgentConfirmFunc  func,
				       gpointer                   data);
void bluetooth_agent_set_authorize_func (BluetoothAgent              *agent,
					 BluetoothAgentAuthorizeFunc  func,
					 gpointer                     data);
void bluetooth_agent_set_authorize_service_func (BluetoothAgent                     *agent,
						 BluetoothAgentAuthorizeServiceFunc  func,
						 gpointer                            data);
void bluetooth_agent_set_cancel_func (BluetoothAgent           *agent,
				      BluetoothAgentCancelFunc  func,
				      gpointer                  data);

#define BLUETOOTH_AGENT_ERROR (bluetooth_agent_error_quark())
#define BLUETOOTH_AGENT_ERROR_TYPE (bluetooth_agent_error_get_type())

typedef enum {
	BLUETOOTH_AGENT_ERROR_REJECT
} BluetoothAgentError;

GType  bluetooth_agent_error_get_type (void);
GQuark bluetooth_agent_error_quark    (void);
