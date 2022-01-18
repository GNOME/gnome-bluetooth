/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
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

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>
#include <math.h>

#include "bluetooth-client.h"
#include "bluetooth-device.h"
#include "bluetooth-client-private.h"
#include "bluetooth-client-glue.h"
#include "bluetooth-agent.h"
#include "bluetooth-utils.h"
#include "bluetooth-settings-widget.h"
#include "bluetooth-settings-resources.h"
#include "bluetooth-settings-row.h"
#include "bluetooth-settings-obexpush.h"
#include "bluetooth-pairing-dialog.h"
#include "pin.h"

struct _BluetoothSettingsWidget {
	GtkBox               parent;

	GtkBuilder          *builder;
	GtkWidget           *child_box;
	BluetoothClient     *client;
	gboolean             debug;
	GCancellable        *cancellable;

	/* Pairing */
	BluetoothAgent      *agent;
	GtkWindow           *pairing_dialog;
	GHashTable          *pairing_devices; /* key=object-path, value=boolean */

	/* Properties */
	GtkWindow           *properties_dialog;
	char                *selected_bdaddr;
	char                *selected_name;
	char                *selected_object_path;

	/* Device section */
	GtkWidget           *device_label;
	GtkWidget           *device_list;
	GtkAdjustment       *focus_adjustment;
	GtkSizeGroup        *row_sizegroup;
	GtkWidget           *device_stack;
	GtkWidget           *device_spinner;
	GHashTable          *connecting_devices; /* key=bdaddr, value=boolean */

	/* Hack to work-around:
	 * http://thread.gmane.org/gmane.linux.bluez.kernel/41471 */
	GHashTable          *devices_type; /* key=bdaddr, value=guint32 */

	/* Sharing section */
	GtkWidget           *visible_label;
	gboolean             has_console;
	GDBusProxy          *session_proxy;
};

G_DEFINE_TYPE(BluetoothSettingsWidget, bluetooth_settings_widget, GTK_TYPE_BOX)

enum {
	PANEL_CHANGED,
	ADAPTER_STATUS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->builder, s))

#define KEYBOARD_PREFS		"keyboard"
#define MOUSE_PREFS		"mouse"
#define SOUND_PREFS		"sound"

#define ICON_SIZE 128

/* We'll try to connect to the device repeatedly for that
 * amount of time before we bail out */
#define CONNECT_TIMEOUT 3.0

#define BLUEZ_SERVICE	"org.bluez"
#define ADAPTER_IFACE	"org.bluez.Adapter1"

#define AGENT_PATH "/org/gnome/bluetooth/settings"

#define GNOME_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"

#define FILLER_PAGE                  "filler-page"
#define DEVICES_PAGE                 "devices-page"

enum {
	CONNECTING_NOTEBOOK_PAGE_SWITCH = 0,
	CONNECTING_NOTEBOOK_PAGE_SPINNER = 1
};

static void
set_connecting_page (BluetoothSettingsWidget *self,
		     int                      page)
{
	if (page == CONNECTING_NOTEBOOK_PAGE_SPINNER)
		gtk_spinner_start (GTK_SPINNER (WID ("connecting_spinner")));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (WID ("connecting_notebook")), page);
	if (page == CONNECTING_NOTEBOOK_PAGE_SWITCH)
		gtk_spinner_stop (GTK_SPINNER (WID ("connecting_spinner")));
}

static void
remove_connecting (BluetoothSettingsWidget *self,
		   const char              *bdaddr)
{
	g_hash_table_remove (self->connecting_devices, bdaddr);
}

static void
add_connecting (BluetoothSettingsWidget *self,
		const char              *bdaddr)
{
	g_hash_table_insert (self->connecting_devices,
			     g_strdup (bdaddr),
			     GINT_TO_POINTER (1));
}

static gboolean
is_connecting (BluetoothSettingsWidget *self,
	       const char              *bdaddr)
{
	return GPOINTER_TO_INT (g_hash_table_lookup (self->connecting_devices,
						     bdaddr));
}

static gboolean
has_default_adapter (BluetoothSettingsWidget *self)
{
	g_autofree char *default_adapter = NULL;

	g_object_get (self->client, "default-adapter", &default_adapter, NULL);
	return (default_adapter != NULL);
}

typedef struct {
	char             *bdaddr;
	BluetoothSettingsWidget *self;
} ConnectData;

static void
connect_done (GObject      *source_object,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	BluetoothSettingsWidget *self;
	gboolean success;
	g_autoptr(GError) error = NULL;
	ConnectData *data = (ConnectData *) user_data;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object),
							   res, &error);
	if (!success && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto out;

	self = data->self;

	/* Check whether the same device is now selected, and update the UI */
	if (g_strcmp0 (self->selected_bdaddr, data->bdaddr) == 0) {
		GtkSwitch *button;

		button = GTK_SWITCH (WID ("switch_connection"));

		/* Ensure the switch position is in the correct place. */
		gtk_switch_set_active (button, gtk_switch_get_state (button));

		if (success == FALSE) {
			g_debug ("Connection failed to %s: %s", data->bdaddr, error->message);
		}
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
	}

	remove_connecting (self, data->bdaddr);

	//FIXME show an error if it failed?
	g_object_set (G_OBJECT (self->client),
		      "default-adapter-setup-mode", has_default_adapter (self),
		      NULL);

out:
	g_free (data->bdaddr);
	g_free (data);
}

static void
add_device_type (BluetoothSettingsWidget *self,
		 const char              *bdaddr,
		 BluetoothType            type)
{
	BluetoothType t;

	t = GPOINTER_TO_UINT (g_hash_table_lookup (self->devices_type, bdaddr));
	if (t == 0 || t == BLUETOOTH_TYPE_ANY) {
		g_hash_table_insert (self->devices_type, g_strdup (bdaddr), GUINT_TO_POINTER (type));
		g_debug ("Saving device type %s for %s", bluetooth_type_to_string (type), bdaddr);
	}
}

static void
setup_pairing_dialog (BluetoothSettingsWidget *self)
{
	GtkWidget *toplevel;

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
	self->pairing_dialog = GTK_WINDOW (bluetooth_pairing_dialog_new ());
	toplevel = GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (self)));
	gtk_window_set_transient_for (self->pairing_dialog, GTK_WINDOW (toplevel));
	gtk_window_set_modal (self->pairing_dialog, TRUE);
}

static gboolean
get_properties_for_device (BluetoothSettingsWidget  *self,
			   GDBusProxy               *device,
			   char                    **name,
			   char                    **ret_bdaddr,
			   BluetoothType            *type)
{
	g_autoptr(GVariant) value = NULL;
	g_autofree char *bdaddr = NULL;

	g_return_val_if_fail (name != NULL, FALSE);

	value = g_dbus_proxy_get_cached_property (device, "Name");
	if (value == NULL) {
		/* What?! */
		return FALSE;
	}
	*name = g_variant_dup_string (value, NULL);
	g_clear_pointer (&value, g_variant_unref);

	value = g_dbus_proxy_get_cached_property (device, "Address");
	bdaddr = g_variant_dup_string (value, NULL);
	g_clear_pointer (&value, g_variant_unref);

	if (ret_bdaddr)
		*ret_bdaddr = g_strdup (bdaddr);

	if (type) {
		value = g_dbus_proxy_get_cached_property (device, "Class");
		if (value != NULL) {
			*type = bluetooth_class_to_type (g_variant_get_uint32 (value));
		} else {
			*type = GPOINTER_TO_UINT (g_hash_table_lookup (self->devices_type, bdaddr));
			if (*type == 0)
				*type = BLUETOOTH_TYPE_ANY;
		}
	}

	return TRUE;
}

static char *
get_random_pincode (guint num_digits)
{
	if (num_digits == 0)
		num_digits = PIN_NUM_DIGITS;
	return g_strdup_printf ("%d", g_random_int_range (pow (10, num_digits - 1),
							  pow (10, num_digits)));
}

static char *
get_icade_pincode (char **pin_display_str)
{
	GString *pin, *pin_display;
	guint i;
	static char *arrows[] = {
		NULL,
		"⬆", /* up = 1    */
		"⬇", /* down = 2  */
		"⬅", /* left = 3  */
		"➡"  /* right = 4 */
	};

	pin = g_string_new (NULL);
	pin_display = g_string_new (NULL);

	for (i = 0; i < PIN_NUM_DIGITS; i++) {
		int r;
		g_autofree char *c = NULL;

		r = g_random_int_range (1, 5);

		c = g_strdup_printf ("%d", r);
		g_string_append (pin, c);

		g_string_append (pin_display, arrows[r]);
	}
	g_string_append (pin_display, "❍");

	*pin_display_str = g_string_free (pin_display, FALSE);
	return g_string_free (pin, FALSE);
}

static void
display_cb (GtkDialog *dialog,
	    int        response,
	    gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
}

static void
enter_pin_cb (GtkDialog *dialog,
	      int        response,
	      gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		const char *name;
		g_autofree char *pin = NULL;
		BluetoothPairingMode mode;

		mode = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (dialog), "mode"));
		name = g_object_get_data (G_OBJECT (dialog), "name");
		pin = bluetooth_pairing_dialog_get_pin (BLUETOOTH_PAIRING_DIALOG (dialog));
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", pin));

		if (bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog)) == BLUETOOTH_PAIRING_MODE_PIN_QUERY) {
			g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
			return;
		}
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
						   mode, pin, name);
		g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
				  G_CALLBACK (display_cb), user_data);
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation,
							    "org.bluez.Error.Canceled",
							    "User cancelled pairing");
		g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
		return;
	}

	g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", NULL);
	g_object_set_data (G_OBJECT (self->pairing_dialog), "mode", NULL);
	g_object_set_data (G_OBJECT (self->pairing_dialog), "name", NULL);
}

static void
confirm_remote_pin_cb (GtkDialog *dialog,
		       int        response,
		       gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		GDBusProxy *device;
		const char *pin;

		pin = g_object_get_data (G_OBJECT (invocation), "pin");
		device = g_object_get_data (G_OBJECT (invocation), "device");

		bluetooth_client_set_trusted (self->client, g_dbus_proxy_get_object_path (device), TRUE);

		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", pin));
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", "Pairing refused from settings panel");
	}

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
}

static void
pincode_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothType type;
	g_autofree char *name = NULL;
	g_autofree char *bdaddr = NULL;
	guint max_digits;
	gboolean confirm_pin = TRUE;
	g_autofree char *default_pin = NULL;
	g_autofree char *display_pin = NULL;
	BluetoothPairingMode mode;
	gboolean remote_initiated;

	g_debug ("pincode_callback (%s)", g_dbus_proxy_get_object_path (device));

	if (!get_properties_for_device (self, device, &name, &bdaddr, &type)) {
		g_autofree char *msg = NULL;

		msg = g_strdup_printf ("Missing information for %s", g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		return;
	}

	remote_initiated = !GPOINTER_TO_UINT (g_hash_table_lookup (self->pairing_devices,
								   g_dbus_proxy_get_object_path (device)));

	default_pin = get_pincode_for_device (type, bdaddr, name, &max_digits, &confirm_pin);
	if (g_strcmp0 (default_pin, "KEYBOARD") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD;
		g_free (default_pin);
		default_pin = get_random_pincode (max_digits);
		display_pin = g_strdup_printf ("%s⏎", default_pin);
	} else if (g_strcmp0 (default_pin, "ICADE") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE;
		confirm_pin = FALSE;
		g_free (default_pin);
		default_pin = get_icade_pincode (&display_pin);
	} else if (default_pin == NULL) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL;
		confirm_pin = TRUE;
		default_pin = get_random_pincode (0);
	} else if (g_strcmp0 (default_pin, "NULL") == 0) {
		g_assert_not_reached ();
	} else {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL;
		display_pin = g_strdup (default_pin);
	}

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	g_object_set_data_full (G_OBJECT (self->pairing_dialog), "name", g_strdup (name), g_free);
	g_object_set_data (G_OBJECT (self->pairing_dialog), "mode", GUINT_TO_POINTER (mode));

	if (confirm_pin) {
		g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", invocation);
		if (remote_initiated) {
			bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
							   BLUETOOTH_PAIRING_MODE_PIN_QUERY,
							   default_pin,
							   name);
		} else {
			bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
							   BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION,
							   default_pin,
							   name);
		}
		g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
				  G_CALLBACK (enter_pin_cb), user_data);
	} else if (!remote_initiated) {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
						   mode, display_pin, name);
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", default_pin));
		g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
				  G_CALLBACK (display_cb), user_data);
	} else {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
						   BLUETOOTH_PAIRING_MODE_YES_NO,
						   default_pin, name);

		g_object_set_data_full (G_OBJECT (invocation), "pin", g_steal_pointer (&default_pin), g_free);
		g_object_set_data_full (G_OBJECT (invocation), "device", g_object_ref (device), g_object_unref);
		g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", invocation);

		g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
				  G_CALLBACK (confirm_remote_pin_cb), user_data);
	}

	gtk_window_present (self->pairing_dialog);
}

static void
cancel_setup_cb (GObject      *source_object,
		 GAsyncResult *res,
		 gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	g_autofree char *path = NULL;
	gboolean ret;

	ret = bluetooth_client_cancel_setup_device_finish (BLUETOOTH_CLIENT (source_object),
							   res, &path, &error);
	if (!ret)
		g_debug ("Setup cancellation for '%s' failed: %s", path, error->message);
	else
		g_debug ("Setup cancellation for '%s' success", path);
}

static void
display_passkey_or_pincode_cb (GtkDialog *dialog,
			       int        response,
			       gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;

	if (response == GTK_RESPONSE_CANCEL ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		g_autofree char *path = NULL;

		path = g_object_steal_data (G_OBJECT (dialog), "path");
		bluetooth_client_cancel_setup_device (self->client,
						      path,
						      self->cancellable,
						      cancel_setup_cb,
						      user_data);
	} else {
		g_assert_not_reached ();
	}

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
}

static void
display_passkey_callback (GDBusMethodInvocation *invocation,
		          GDBusProxy            *device,
			  guint                  pin,
			  guint                  entered,
			  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	g_autofree char *pin_str = NULL;
	g_autofree char *name = NULL;

	g_debug ("display_passkey_callback (%s, %i, %i)", g_dbus_proxy_get_object_path (device), pin, entered);

	if (self->pairing_dialog == NULL ||
	    bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog)) != BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD)
		setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	pin_str = g_strdup_printf ("%06d", pin);
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD,
					   pin_str,
					   name);
	bluetooth_pairing_dialog_set_pin_entered (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
						  entered);
	g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
			  G_CALLBACK (display_passkey_or_pincode_cb), user_data);
	g_object_set_data_full (G_OBJECT (self->pairing_dialog),
				"path", g_strdup (g_dbus_proxy_get_object_path (device)),
				g_free);

	gtk_window_present (self->pairing_dialog);
}

static void
display_pincode_callback (GDBusMethodInvocation *invocation,
			  GDBusProxy            *device,
			  const char            *pincode,
			  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothType type;
	g_autofree char *display_pin = NULL;
	g_autofree char *name = NULL;
	g_autofree char *bdaddr = NULL;
	g_autofree char *db_pin = NULL;

	g_debug ("display_pincode_callback (%s, %s)", g_dbus_proxy_get_object_path (device), pincode);

	if (!get_properties_for_device (self, device, &name, &bdaddr, &type)) {
		g_autofree char *msg = NULL;

		msg = g_strdup_printf ("Missing information for %s", g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		return;
	}

	/* Verify PIN code validity */
	db_pin = get_pincode_for_device (type,
					 bdaddr,
					 name,
					 NULL,
					 NULL);
	if (g_strcmp0 (db_pin, "KEYBOARD") == 0) {
		/* Should work, follow through */
	} else if (g_strcmp0 (db_pin, "ICADE") == 0) {
		g_autofree char *msg = NULL;

		msg = g_strdup_printf ("Generated pincode for %s when it shouldn't have", name);
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		return;
	} else if (g_strcmp0 (db_pin, "0000") == 0) {
		g_debug ("Ignoring generated keyboard PIN '%s', should get 0000 soon", pincode);
		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	} else if (g_strcmp0 (db_pin, "NULL") == 0) {
		g_autofree char *msg = NULL;

		msg = g_strdup_printf ("Attempting pairing for %s that doesn't support pairing", name);
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		return;
	}

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	display_pin = g_strdup_printf ("%s⏎", pincode);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD,
					   display_pin,
					   name);
	g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
			  G_CALLBACK (display_passkey_or_pincode_cb), user_data);
	g_object_set_data_full (G_OBJECT (self->pairing_dialog),
				"path", g_strdup (g_dbus_proxy_get_object_path (device)),
				g_free);
	gtk_window_present (self->pairing_dialog);

	g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
passkey_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  gpointer               data)
{
	g_warning ("RequestPasskey(): not implemented");
	g_dbus_method_invocation_return_dbus_error (invocation,
						    "org.bluez.Error.Rejected",
						    "RequestPasskey not implemented");
}

static gboolean
cancel_callback (GDBusMethodInvocation *invocation,
		 gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GtkWidget *child;

	g_debug ("cancel_callback ()");

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);

	child = gtk_widget_get_first_child (self->device_list);
	while (child) {
		g_object_set (G_OBJECT (child), "pairing", FALSE, NULL);
		child = gtk_widget_get_next_sibling (child);
	}

	g_dbus_method_invocation_return_value (invocation, NULL);

	return TRUE;
}

static void
confirm_cb (GtkDialog *dialog,
	    int        response,
	    gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");
	if (response == GTK_RESPONSE_ACCEPT) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation,
							    "org.bluez.Error.Canceled",
							    "User cancelled pairing");
	}
	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
}

static void
confirm_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  guint                  pin,
		  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	g_autofree char *name = NULL;
	g_autofree char *pin_str = NULL;

	g_debug ("confirm_callback (%s, %i)", g_dbus_proxy_get_object_path (device), pin);

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	pin_str = g_strdup_printf ("%06d", pin);
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_MATCH,
					   pin_str, name);

	g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
			  G_CALLBACK (confirm_cb), user_data);
	g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", invocation);

	gtk_window_present (self->pairing_dialog);
}

static void
authorize_callback (GDBusMethodInvocation *invocation,
		    GDBusProxy            *device,
		    gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	g_autofree char *name = NULL;

	g_debug ("authorize_callback (%s)", g_dbus_proxy_get_object_path (device));

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_YES_NO,
					   NULL, name);

	g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
			  G_CALLBACK (confirm_cb), user_data);
	g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", invocation);

	gtk_window_present (self->pairing_dialog);
}

static void
authorize_service_cb (GtkDialog *dialog,
		      int        response,
		      gpointer   user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		GDBusProxy *device;

		device = g_object_get_data (G_OBJECT (invocation), "device");
		bluetooth_client_set_trusted (self->client, g_dbus_proxy_get_object_path (device), TRUE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_autofree char *msg = NULL;

		msg = g_strdup_printf ("Rejecting service auth (HID): not paired or trusted");
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
	}
	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
}

static void
authorize_service_callback (GDBusMethodInvocation *invocation,
			    GDBusProxy            *device,
			    const char            *uuid,
			    gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GVariant *value;
	gboolean paired, trusted;

	g_debug ("authorize_service_callback (%s, %s)", g_dbus_proxy_get_object_path (device), uuid);

	value = g_dbus_proxy_get_cached_property (device, "Paired");
	paired = g_variant_get_boolean (value);
	g_variant_unref (value);

	value = g_dbus_proxy_get_cached_property (device, "Trusted");
	trusted = g_variant_get_boolean (value);
	g_variant_unref (value);

	/* Device was paired, initiated from the remote device,
	 * so we didn't get the opportunity to set the trusted bit */
	if (paired && !trusted) {
		bluetooth_client_set_trusted (self->client,
					      g_dbus_proxy_get_object_path (device), TRUE);
		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	}

	if (g_strcmp0 (bluetooth_uuid_to_string (uuid), "HumanInterfaceDeviceService") != 0) {
		g_autofree char *msg = NULL;
		msg = g_strdup_printf ("Rejecting service auth (%s) for %s: not HID",
				       uuid, g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		return;
	}

	/* We shouldn't get asked, but shizzle happens */
	if (paired || trusted) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_autofree char *name = NULL;

		setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));
		get_properties_for_device (self, device, &name, NULL, NULL);
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (self->pairing_dialog),
						   BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH,
						   NULL, name);

		g_signal_connect (G_OBJECT (self->pairing_dialog), "response",
				  G_CALLBACK (authorize_service_cb), user_data);
		g_object_set_data_full (G_OBJECT (invocation), "device", g_object_ref (device), g_object_unref);
		g_object_set_data (G_OBJECT (self->pairing_dialog), "invocation", invocation);

		gtk_window_present (self->pairing_dialog);
	}
}

static void
turn_off_pairing (BluetoothSettingsWidget *self,
		  const char              *object_path)
{
	GtkWidget *child;

	for (child = gtk_widget_get_first_child (self->device_list);
	     child != NULL;
	     child = gtk_widget_get_next_sibling (child)) {
		g_autoptr(GDBusProxy) proxy = NULL;

		if (!GTK_IS_LIST_BOX_ROW (child))
			continue;

		g_object_get (G_OBJECT (child), "proxy", &proxy, NULL);
		if (g_strcmp0 (g_dbus_proxy_get_object_path (proxy), object_path) == 0) {
			g_object_set (G_OBJECT (child), "pairing", FALSE, NULL);
			break;
		}
	}
}

typedef struct {
	BluetoothSettingsWidget *self;
	char *device;
	GTimer *timer;
	guint timeout_id;
} SetupConnectData;

static void connect_callback (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data);

static gboolean
connect_timeout_cb (gpointer user_data)
{
	SetupConnectData *data = (SetupConnectData *) user_data;
	BluetoothSettingsWidget *self = data->self;

	bluetooth_client_connect_service (self->client, data->device, TRUE, self->cancellable, connect_callback, data);
	data->timeout_id = 0;

	return G_SOURCE_REMOVE;
}

static void
connect_callback (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      user_data)
{
	SetupConnectData *data = (SetupConnectData *) user_data;
	g_autoptr(GError) error = NULL;
	gboolean success;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object), res, &error);

	if (success == FALSE) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			goto bail;
		} else if (g_timer_elapsed (data->timer, NULL) < CONNECT_TIMEOUT) {
			g_assert (data->timeout_id == 0);
			data->timeout_id = g_timeout_add (500, connect_timeout_cb, data);
			return;
		}
		g_debug ("Failed to connect to device %s: %s", data->device, error->message);
	}

	turn_off_pairing (data->self, data->device);

	g_object_set (G_OBJECT (data->self->client),
		      "default-adapter-setup-mode", has_default_adapter (data->self),
		      NULL);

bail:
	if (data->timeout_id > 0)
		g_source_remove (data->timeout_id);

	g_free (data->device);
	g_timer_destroy (data->timer);
	g_free (data);
}

static void
create_callback (GObject      *source_object,
		 GAsyncResult *res,
		 gpointer      user_data)
{
	BluetoothSettingsWidget *self = user_data;
	SetupConnectData *data;
	g_autoptr(GError) error = NULL;
	gboolean ret;
	g_autofree char *path = NULL;

	ret = bluetooth_client_setup_device_finish (BLUETOOTH_CLIENT (source_object),
						    res, &path, &error);

	/* Create failed */
	if (ret == FALSE) {
		//char *text;
		g_autofree char *dbus_error = NULL;

		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			return;

		g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);

		turn_off_pairing (user_data, path);

		dbus_error = g_dbus_error_get_remote_error (error);
		if (g_strcmp0 (dbus_error, "org.bluez.Error.AuthenticationFailed") == 0) {
			//FIXME pairing failed because we entered the wrong code
		} else if (g_strcmp0 (dbus_error, "org.bluez.Error.AuthenticationCanceled") == 0) {
			//FIXME pairing was cancelled
		} else {
			//FIXME show an error?
			/* translators:
			 * The “%s” is the device name, for example:
			 * Setting up “Sony Bluetooth Headset” failed
			 */
			//text = g_strdup_printf(_("Setting up “%s” failed"), target_name);

			g_warning ("Setting up %s failed: %s", path, error->message);

			//gtk_label_set_markup(GTK_LABEL(label_summary), text);
			//g_free (text);
		}

		g_object_set (G_OBJECT (self->client),
			      "default-adapter-setup-mode", has_default_adapter (self),
			      NULL);
		return;
	}

	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);

	g_hash_table_remove (self->pairing_devices, path);

	bluetooth_client_set_trusted (BLUETOOTH_CLIENT (source_object), path, TRUE);

	data = g_new0 (SetupConnectData, 1);
	data->self = user_data;
	data->device = g_steal_pointer (&path);
	data->timer = g_timer_new ();

	bluetooth_client_connect_service (BLUETOOTH_CLIENT (source_object),
					  data->device, TRUE, self->cancellable, connect_callback, data);
	//gtk_assistant_set_current_page (window_assistant, PAGE_FINISHING);
}

static void start_pairing (BluetoothSettingsWidget *self,
			   GtkListBoxRow           *row);

static void
device_name_appeared (GObject    *gobject,
		      GParamSpec *pspec,
		      gpointer    user_data)
{
	g_autofree char *name = NULL;

	g_object_get (G_OBJECT (gobject),
		      "name", &name,
		      NULL);
	if (!name)
		return;

	g_debug ("Pairing device name is now '%s'", name);
	start_pairing (user_data, GTK_LIST_BOX_ROW (gobject));

	g_signal_handlers_disconnect_by_func (gobject, device_name_appeared, user_data);
}

static void
start_pairing (BluetoothSettingsWidget *self,
	       GtkListBoxRow           *row)
{
	g_autoptr(GDBusProxy) proxy = NULL;
	gboolean pair = TRUE;
	BluetoothType type;
	g_autofree char *bdaddr = NULL;
	g_autofree char *name = NULL;
	gboolean legacy_pairing;
	const char *pincode;

	g_object_set (G_OBJECT (row), "pairing", TRUE, NULL);
	g_object_get (G_OBJECT (row),
		      "proxy", &proxy,
		      "type", &type,
		      "address", &bdaddr,
		      "name", &name,
		      "legacy-pairing", &legacy_pairing,
		      NULL);

	if (name == NULL) {
		g_debug ("No name yet, will start pairing later");
		g_signal_connect (G_OBJECT (row), "notify::name",
				  G_CALLBACK (device_name_appeared), self);
		return;
	}

	g_debug ("Starting pairing for '%s'", name);

	/* Legacy pairing might not have been detected yet,
	 * so don't check for it */
	pincode = get_pincode_for_device (type,
					  bdaddr,
					  name,
					  NULL,
					  NULL);
	if (g_strcmp0 (pincode, "NULL") == 0)
		pair = FALSE;

	g_debug ("About to setup %s (legacy pairing: %d pair: %d)",
		 g_dbus_proxy_get_object_path (proxy),
		 legacy_pairing, pair);

	g_hash_table_insert (self->pairing_devices,
			     g_strdup (g_dbus_proxy_get_object_path (proxy)),
			     GINT_TO_POINTER (1));

	g_object_set (G_OBJECT (self->client), "default-adapter-setup-mode", FALSE, NULL);
	bluetooth_client_setup_device (self->client,
				       g_dbus_proxy_get_object_path (proxy),
				       pair,
				       self->cancellable,
				       (GAsyncReadyCallback) create_callback,
				       self);
}

static gboolean
switch_connected_state_set (GtkSwitch               *button,
			    gboolean                 state,
			    BluetoothSettingsWidget *self)
{
	ConnectData *data;

	if (gtk_switch_get_state (button) == state)
		return TRUE;

	if (is_connecting (self, self->selected_bdaddr))
		return TRUE;

	data = g_new0 (ConnectData, 1);
	data->bdaddr = g_strdup (self->selected_bdaddr);
	data->self = self;

	if (gtk_switch_get_active (button))
		g_object_set (G_OBJECT (self->client),
			      "default-adapter-setup-mode", FALSE,
			      NULL);
	bluetooth_client_connect_service (self->client,
					  self->selected_object_path,
					  gtk_switch_get_active (button),
					  self->cancellable,
					  connect_done,
					  data);

	add_connecting (self, data->bdaddr);
	set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);

	return TRUE;
}

static void
update_properties (BluetoothSettingsWidget *self,
		   BluetoothDevice         *device)
{
	GtkSwitch *button;
	BluetoothType type;
	gboolean connected, paired;
	g_auto(GStrv) uuids = NULL;
	char *bdaddr, *alias;
	g_autofree char *icon = NULL;
	guint i;

	if (self->debug)
		bluetooth_device_dump (device);

	g_object_get (device,
		      "address", &bdaddr,
		      "alias", &alias,
		      "icon", &icon,
		      "paired", &paired,
		      "connected", &connected,
		      "uuids", &uuids,
		      "type", &type,
		      NULL);

	g_free (self->selected_object_path);
	self->selected_object_path = g_strdup (bluetooth_device_get_object_path (device));

	/* Hide all the buttons now, and show them again if we need to */
	gtk_widget_hide (WID ("keyboard_button"));
	gtk_widget_hide (WID ("sound_button"));
	gtk_widget_hide (WID ("mouse_button"));
	gtk_widget_hide (WID ("send_button"));

	/* Name */
	gtk_window_set_title (GTK_WINDOW (self->properties_dialog), alias);
	g_free (self->selected_name);
	self->selected_name = alias;

	/* Icon */
	gtk_image_set_from_icon_name (GTK_IMAGE (WID ("image")), icon);

	/* Connection */
	button = GTK_SWITCH (WID ("switch_connection"));

	gtk_switch_set_state (button, connected);
	if (is_connecting (self, bdaddr)) {
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);
	} else {
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
	}

	/* Paired */
	gtk_label_set_text (GTK_LABEL (WID ("paired_label")),
			    paired ? _("Yes") : _("No"));

	/* UUIDs */
	gtk_widget_set_sensitive (GTK_WIDGET (button),
				  bluetooth_client_get_connectable ((const char **) uuids));
	for (i = 0; uuids && uuids[i] != NULL; i++) {
		if (g_str_equal (uuids[i], "OBEXObjectPush")) {
			gtk_widget_show (WID ("send_button"));
			break;
		}
	}

	/* Type */
	gtk_label_set_text (GTK_LABEL (WID ("type_label")), bluetooth_type_to_string (type));
	switch (type) {
	case BLUETOOTH_TYPE_KEYBOARD:
		gtk_widget_show (WID ("keyboard_button"));
		break;
	case BLUETOOTH_TYPE_MOUSE:
	case BLUETOOTH_TYPE_TABLET:
		gtk_widget_show (WID ("mouse_button"));
		break;
	case BLUETOOTH_TYPE_HEADSET:
	case BLUETOOTH_TYPE_HEADPHONES:
	case BLUETOOTH_TYPE_OTHER_AUDIO:
		gtk_widget_show (WID ("sound_button"));
	default:
		/* others? */
		;
	}

	/* Address */
	gtk_label_set_text (GTK_LABEL (WID ("address_label")), bdaddr);

	g_free (self->selected_bdaddr);
	self->selected_bdaddr = bdaddr;
}

static void
switch_panel (BluetoothSettingsWidget *self,
	      const char       *panel)
{
	g_signal_emit (G_OBJECT (self),
		       signals[PANEL_CHANGED],
		       0, panel);
}

static gboolean
keyboard_callback (GtkButton        *button,
		   BluetoothSettingsWidget *self)
{
	switch_panel (self, KEYBOARD_PREFS);
	return TRUE;
}

static gboolean
mouse_callback (GtkButton        *button,
		BluetoothSettingsWidget *self)
{
	switch_panel (self, MOUSE_PREFS);
	return TRUE;
}

static gboolean
sound_callback (GtkButton        *button,
		BluetoothSettingsWidget *self)
{
	switch_panel (self, SOUND_PREFS);
	return TRUE;
}

static void
send_callback (GtkButton               *button,
	       BluetoothSettingsWidget *self)
{
	g_autoptr(GError) error = NULL;

	if (!bluetooth_send_to_address (self->selected_bdaddr, self->selected_name, &error))
		g_warning ("Failed to call bluetooth-sendto: %s",
			   error ? error->message : "unknown error");
}

/* Visibility/Discoverable */
static void
update_visibility (BluetoothSettingsWidget *self)
{
	g_autofree char *name = NULL;

	g_object_get (G_OBJECT (self->client), "default-adapter-name", &name, NULL);
	if (name != NULL) {
		g_autofree char *label = NULL;
		g_autofree char *path = NULL;
		g_autofree char *uri = NULL;

		path = lookup_download_dir ();
		uri = g_filename_to_uri (path, NULL, NULL);

		/* translators: first %s is the name of the computer, for example:
		 * Visible as “Bastien Nocera’s Computer” followed by the
		 * location of the Downloads folder.*/
		label = g_strdup_printf (_("Visible as “%s” and available for Bluetooth file transfers. Transferred files are placed in the <a href=\"%s\">Downloads</a> folder."), name, uri);
		gtk_label_set_markup (GTK_LABEL (self->visible_label), label);
	}
	gtk_widget_set_visible (self->visible_label, name != NULL);
}

static void
name_changed (BluetoothClient  *client,
	      GParamSpec       *spec,
	      BluetoothSettingsWidget *self)
{
	update_visibility (self);
}

static void
confirm_dialog_response_cb (GtkDialog *dialog,
			    gint       response,
			    gpointer   user_data)
{
	GMainLoop *mainloop = g_object_get_data (G_OBJECT (dialog), "mainloop");
	int *out_response = user_data;

	*out_response = response;

	g_main_loop_quit (mainloop);
}

static gboolean
show_confirm_dialog (BluetoothSettingsWidget *self,
		     const char *name)
{
	g_autoptr(GMainLoop) mainloop = NULL;
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new (GTK_WINDOW (self->properties_dialog), GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Remove “%s” from the list of devices?"), name);
	g_object_set (G_OBJECT (dialog), "secondary-text",
		      _("If you remove the device, you will have to set it up again before next use."),
		      NULL);

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Remove"), GTK_RESPONSE_ACCEPT);

	mainloop = g_main_loop_new (NULL, FALSE);
	g_object_set_data (G_OBJECT (dialog), "mainloop", mainloop);
	g_signal_connect (dialog, "response", G_CALLBACK (confirm_dialog_response_cb), &response);

	g_main_loop_run (mainloop);

	gtk_window_destroy (GTK_WINDOW (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;

	return FALSE;
}

static gboolean
remove_selected_device (BluetoothSettingsWidget *self)
{
	GDBusProxy *adapter_proxy;
	g_autoptr(GError) error = NULL;
	GVariant *ret;

	g_debug ("About to call RemoveDevice for %s", self->selected_object_path);

	adapter_proxy = _bluetooth_client_get_default_adapter (self->client);

	if (adapter_proxy == NULL) {
		g_warning ("Failed to get a GDBusProxy for the default adapter");
		return FALSE;
	}

	//FIXME use adapter1_call_remove_device_sync()
	ret = g_dbus_proxy_call_sync (G_DBUS_PROXY (adapter_proxy),
				      "RemoveDevice",
				      g_variant_new ("(o)", self->selected_object_path),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret == NULL) {
		g_warning ("Failed to remove device '%s': %s",
			   self->selected_object_path, error->message);
	} else {
		g_variant_unref (ret);
	}

	g_object_unref (adapter_proxy);

	return (ret != NULL);
}

static void
delete_clicked (GtkButton               *button,
		BluetoothSettingsWidget *self)
{
	if (show_confirm_dialog (self, self->selected_name) != FALSE) {
		remove_selected_device (self);
		gtk_widget_hide (GTK_WIDGET (self->properties_dialog));
	}
}

static void
default_adapter_changed (BluetoothClient         *client,
			 GParamSpec              *spec,
			 BluetoothSettingsWidget *self)
{
	g_autofree char *default_adapter = NULL;

	g_object_get (self->client, "default-adapter", &default_adapter, NULL);

	g_debug ("Default adapter changed to: %s", default_adapter ? default_adapter : "(none)");

	g_object_set (G_OBJECT (client), "default-adapter-setup-mode", default_adapter != NULL, NULL);

	g_signal_emit (G_OBJECT (self), signals[ADAPTER_STATUS_CHANGED], 0);
}

static gint
device_sort_func (gconstpointer a, gconstpointer b, gpointer data)
{
	GObject *row_a = (GObject*)a;
	GObject *row_b = (GObject*)b;
	gboolean setup_a, setup_b;
	gboolean paired_a, paired_b;
	gboolean trusted_a, trusted_b;
	gboolean connected_a, connected_b;
	gint64 time_a, time_b;

	g_object_get (row_a,
		      "paired", &paired_a,
		      "trusted", &trusted_a,
		      "connected", &connected_a,
		      "time-created", &time_a,
		      NULL);
	g_object_get (row_b,
		      "paired", &paired_b,
		      "trusted", &trusted_b,
		      "connected", &connected_b,
		      "time-created", &time_b,
		      NULL);

	/* First, paired or trusted devices (setup devices) */
	setup_a = paired_a || trusted_a;
	setup_b = paired_b || trusted_b;
	if (setup_a != setup_b) {
		if (setup_a)
			return -1;
		else
			return 1;
	}

	/* Then connected ones */
	if (connected_a != connected_b) {
		if (connected_a)
			return -1;
		else
			return 1;
	}

	/* And all being equal, oldest ones first */
	return (time_a > time_b) ? 1 : -1;
}

static void
update_header_func (GtkListBoxRow  *row,
		    GtkListBoxRow  *before,
		    gpointer    user_data)
{
	GtkWidget *current;

	if (before == NULL)
		return;

	current = gtk_list_box_row_get_header (row);
	if (current == NULL) {
		current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_list_box_row_set_header (row, current);
	}
}

static gboolean
keynav_failed (GtkWidget *list, GtkDirectionType direction, BluetoothSettingsWidget *self)
{
	GtkWidget *child;

	if (direction == GTK_DIR_DOWN) {
		child = gtk_widget_get_first_child (self->device_list);
	} else {
		child = gtk_widget_get_last_child (self->device_list);
	}

	gtk_widget_child_focus (child, direction);

	return TRUE;
}

static void
activate_row (BluetoothSettingsWidget *self,
              GtkListBoxRow    *row)
{
	GtkWindow *w;
	GtkWidget *toplevel;
	g_autoptr(BluetoothDevice) device = NULL;
	gboolean paired, trusted, is_setup;

	g_object_get (G_OBJECT (row),
		      "paired", &paired,
		      "trusted", &trusted,
		      "device", &device,
		      NULL);
	is_setup = paired || trusted;

	if (is_setup) {
		update_properties (self, device);

		w = self->properties_dialog;
		toplevel = GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (self)));
		gtk_window_set_transient_for (w, GTK_WINDOW (toplevel));
		gtk_window_set_modal (w, TRUE);
		gtk_window_present (w);
	} else {
		start_pairing (self, row);
	}
}

static void
add_device_section (BluetoothSettingsWidget *self)
{
	GtkWidget *vbox;
	GtkWidget *box, *hbox, *spinner;
	GtkWidget *frame, *label;
	gchar *s;

	vbox = WID ("vbox_bluetooth");

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_vexpand (box, TRUE);
	gtk_widget_set_margin_top (box, 6);
	gtk_box_append (GTK_BOX (vbox), box);
	self->child_box = box;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append (GTK_BOX (box), hbox);

	s = g_markup_printf_escaped ("<b>%s</b>", _("Devices"));
	self->device_label = gtk_label_new (s);
	g_free (s);
	gtk_label_set_use_markup (GTK_LABEL (self->device_label), TRUE);
	g_object_set (G_OBJECT (self->device_label),
		      "xalign", 0.0,
		      "yalign", 0.5,
		      NULL);
	gtk_widget_set_margin_end (self->device_label, 6);
	gtk_widget_set_margin_bottom (self->device_label, 12);
	gtk_box_append (GTK_BOX (hbox), self->device_label);

	/* Discoverable spinner */
	self->device_spinner = spinner = gtk_spinner_new ();
	g_object_bind_property (G_OBJECT (self->client), "default-adapter-setup-mode",
				G_OBJECT (self->device_spinner), "spinning",
				G_BINDING_SYNC_CREATE);
	gtk_widget_set_margin_bottom (spinner, 12);
	gtk_box_append (GTK_BOX (hbox), spinner);

	/* Discoverable label placeholder, the real name is set in update_visibility().
	 * If you ever see this string during normal use, please file a bug. */
	self->visible_label = WID ("explanation-label");
	gtk_label_set_use_markup (GTK_LABEL (self->visible_label), TRUE);
	update_visibility (self);

	self->device_list = gtk_list_box_new ();
	g_signal_connect (self->device_list, "keynav-failed", G_CALLBACK (keynav_failed), self);
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->device_list), GTK_SELECTION_NONE);
	gtk_list_box_set_header_func (GTK_LIST_BOX (self->device_list),
				      update_header_func,
				      NULL, NULL);
	gtk_list_box_set_sort_func (GTK_LIST_BOX (self->device_list),
				    (GtkListBoxSortFunc)device_sort_func, NULL, NULL);
	g_signal_connect_swapped (self->device_list, "row-activated",
				  G_CALLBACK (activate_row), self);
	gtk_accessible_update_relation (GTK_ACCESSIBLE (self->device_list),
					GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->device_label, NULL,
					-1);

	self->device_stack = gtk_stack_new ();
	gtk_stack_set_hhomogeneous (GTK_STACK (self->device_stack), FALSE);
	gtk_stack_set_vhomogeneous (GTK_STACK (self->device_stack), FALSE);

	label = gtk_label_new (_("Searching for devices…"));
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
	gtk_stack_add_named (GTK_STACK (self->device_stack), label, FILLER_PAGE);

	frame = gtk_frame_new (NULL);
	gtk_widget_set_vexpand (frame, TRUE);
	gtk_frame_set_child (GTK_FRAME (frame), self->device_list);
	gtk_stack_add_named (GTK_STACK (self->device_stack), frame, DEVICES_PAGE);
	gtk_box_append (GTK_BOX (box), self->device_stack);
}

static void
device_changed_cb (GObject    *object,
		   GParamSpec *pspec,
		   gpointer    user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothDevice *device = BLUETOOTH_DEVICE (object);
	GtkWidget *child;
	const char *object_path;

	object_path = bluetooth_device_get_object_path (device);

	for (child = gtk_widget_get_first_child (self->device_list);
	     child != NULL;
	     child = gtk_widget_get_next_sibling (child)) {
		const char *path;

		if (!GTK_IS_LIST_BOX_ROW (child))
			continue;

		path = g_object_get_data (G_OBJECT (child), "object-path");
		if (g_str_equal (object_path, path)) {
			g_autofree char *address = NULL;
			BluetoothType type;

			g_object_get (G_OBJECT (device),
				      "address", &address,
				      "type", &type,
				      NULL);

			add_device_type (self, address, type);

			/* Update the properties if necessary */
			if (g_strcmp0 (self->selected_object_path, object_path) == 0)
				update_properties (user_data, device);
			break;
		}
	}
}

static void
device_added_cb (BluetoothClient *client,
		 BluetoothDevice *device,
		 gpointer         user_data)
{
	BluetoothSettingsWidget *self = user_data;
	g_autofree char *alias = NULL;
	g_autofree char *address = NULL;
	BluetoothType type;
	GtkWidget *row;

	row = bluetooth_settings_row_new_from_device (device);

	g_object_get (G_OBJECT (row),
		      "alias", &alias,
		      "address", &address,
		      "type", &type,
		      NULL);
	add_device_type (self, address, type);
	g_debug ("Adding device %s (%s)", alias, bluetooth_device_get_object_path (device));

	g_object_set_data_full (G_OBJECT (row), "object-path", g_strdup (bluetooth_device_get_object_path (device)), g_free);

	gtk_list_box_append (GTK_LIST_BOX (self->device_list), row);
	gtk_size_group_add_widget (self->row_sizegroup, row);

	gtk_stack_set_transition_type (GTK_STACK (self->device_stack),
				       GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
	gtk_widget_set_hexpand (self->child_box, FALSE);
	gtk_widget_set_vexpand (self->child_box, FALSE);
	gtk_stack_set_visible_child_name (GTK_STACK (self->device_stack), DEVICES_PAGE);

	g_signal_connect_object (G_OBJECT (device), "notify",
				 G_CALLBACK (device_changed_cb), self, 0);
}

static void
device_removed_cb (BluetoothClient *client,
		   const char      *object_path,
		   gpointer         user_data)
{
	BluetoothSettingsWidget *self = user_data;
	GtkWidget *child;
	gboolean found = FALSE;

	for (child = gtk_widget_get_first_child (self->device_list);
	     child != NULL;
	     child = gtk_widget_get_next_sibling (child)) {
		const char *path;

		if (!GTK_IS_LIST_BOX_ROW (child))
			continue;

		path = g_object_get_data (G_OBJECT (child), "object-path");
		if (g_str_equal (path, object_path)) {
			g_autofree char *name = NULL;

			g_object_get (G_OBJECT (child), "name", &name, NULL);
			g_debug ("Removing device '%s'", name ? name : object_path);

			gtk_list_box_remove (GTK_LIST_BOX (self->device_list), GTK_WIDGET (child));
			found = TRUE;
			break;
		}
	}

	if (found) {
		if (gtk_widget_get_first_child (self->device_list) == NULL) {
			gtk_stack_set_transition_type (GTK_STACK (self->device_stack),
						       GTK_STACK_TRANSITION_TYPE_NONE);
			gtk_widget_set_hexpand (self->child_box, FALSE);
			gtk_widget_set_vexpand (self->child_box, FALSE);
			gtk_stack_set_visible_child_name (GTK_STACK (self->device_stack), FILLER_PAGE);
		}
	} else {
		g_debug ("Didn't find a row to remove for tree path %s", object_path);
	}
}

static void
devices_coldplug (BluetoothSettingsWidget *self)
{
	g_autoptr(GListModel) model = NULL;
	guint n_devices, i;

	model = G_LIST_MODEL (bluetooth_client_get_devices (self->client));
	n_devices = g_list_model_get_n_items (model);
	for (i = 0; i < n_devices; i++) {
		g_autoptr(BluetoothDevice) device = NULL;

		device = g_list_model_get_item (model, i);
		device_added_cb (self->client, device, self);
	}
}

static void
setup_properties_dialog (BluetoothSettingsWidget *self)
{
	GtkStyleContext *context;

	self->properties_dialog = g_object_new (GTK_TYPE_DIALOG, "use-header-bar", TRUE, NULL);
	gtk_window_set_hide_on_close (self->properties_dialog, TRUE);
	gtk_widget_set_size_request (GTK_WIDGET (self->properties_dialog), 380, -1);
	gtk_window_set_resizable (self->properties_dialog, FALSE);
	gtk_window_set_child (self->properties_dialog, WID ("properties_vbox"));

	g_signal_connect (G_OBJECT (WID ("delete_button")), "clicked",
			  G_CALLBACK (delete_clicked), self);
	g_signal_connect (G_OBJECT (WID ("mouse_button")), "clicked",
			  G_CALLBACK (mouse_callback), self);
	g_signal_connect (G_OBJECT (WID ("keyboard_button")), "clicked",
			  G_CALLBACK (keyboard_callback), self);
	g_signal_connect (G_OBJECT (WID ("sound_button")), "clicked",
			  G_CALLBACK (sound_callback), self);
	g_signal_connect (G_OBJECT (WID ("send_button")), "clicked",
			  G_CALLBACK (send_callback), self);
	g_signal_connect (G_OBJECT (WID ("switch_connection")), "state-set",
			  G_CALLBACK (switch_connected_state_set), self);

	/* Styling */
	gtk_image_set_pixel_size (GTK_IMAGE (WID ("image")), ICON_SIZE);

	context = gtk_widget_get_style_context (WID ("delete_button"));
	gtk_style_context_add_class (context, "destructive-action");
}

static void
setup_pairing_agent (BluetoothSettingsWidget *self)
{
	self->agent = bluetooth_agent_new (AGENT_PATH);
	if (bluetooth_agent_register (self->agent) == FALSE) {
		g_clear_object (&self->agent);
		return;
	}

	bluetooth_agent_set_pincode_func (self->agent, pincode_callback, self);
	bluetooth_agent_set_passkey_func (self->agent, passkey_callback, self);
	bluetooth_agent_set_display_passkey_func (self->agent, display_passkey_callback, self);
	bluetooth_agent_set_display_pincode_func (self->agent, display_pincode_callback, self);
	bluetooth_agent_set_cancel_func (self->agent, cancel_callback, self);
	bluetooth_agent_set_confirm_func (self->agent, confirm_callback, self);
	bluetooth_agent_set_authorize_func (self->agent, authorize_callback, self);
	bluetooth_agent_set_authorize_service_func (self->agent, authorize_service_callback, self);
}

static void
session_properties_changed_cb (GDBusProxy               *session,
			       GVariant                 *changed,
			       char                    **invalidated,
			       BluetoothSettingsWidget  *self)
{
	GVariant *v;

	v = g_variant_lookup_value (changed, "SessionIsActive", G_VARIANT_TYPE_BOOLEAN);
	if (v) {
		self->has_console = g_variant_get_boolean (v);
		g_debug ("Received session is active change: now %s", self->has_console ? "active" : "inactive");
		g_variant_unref (v);

		if (self->has_console)
			obex_agent_up ();
		else
			obex_agent_down ();
	}
}

static gboolean
is_session_active (BluetoothSettingsWidget *self)
{
	GVariant *variant;
	gboolean is_session_active = FALSE;

	variant = g_dbus_proxy_get_cached_property (self->session_proxy,
						    "SessionIsActive");
	if (variant) {
		is_session_active = g_variant_get_boolean (variant);
		g_variant_unref (variant);
	}

	return is_session_active;
}

static void
setup_obex (BluetoothSettingsWidget *self)
{
	g_autoptr(GError) error = NULL;

	self->session_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							     G_DBUS_PROXY_FLAGS_NONE,
							     NULL,
							     GNOME_SESSION_DBUS_NAME,
							     GNOME_SESSION_DBUS_OBJECT,
							     GNOME_SESSION_DBUS_INTERFACE,
							     NULL,
							     &error);

	if (self->session_proxy == NULL) {
		g_warning ("Failed to get session proxy: %s", error->message);
		return;
	}
	g_signal_connect (self->session_proxy, "g-properties-changed",
			  G_CALLBACK (session_properties_changed_cb), self);
	self->has_console = is_session_active (self);

	if (self->has_console)
		obex_agent_up ();
}

static void
bluetooth_settings_widget_init (BluetoothSettingsWidget *self)
{
	GtkWidget *widget;
	g_autoptr(GError) error = NULL;

	/* This ensures the BluetoothHdyColumn type is known by GtkBuilder when
	 * loading the UI template.
	 */
	g_type_ensure (ADW_TYPE_CLAMP);

	self->cancellable = g_cancellable_new ();
	self->debug = g_getenv ("BLUETOOTH_DEBUG") != NULL;

	g_resources_register (bluetooth_settings_get_resource ());
	self->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (self->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (self->builder,
                                       "/org/gnome/bluetooth/settings.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Could not load ui: %s", error->message);
		return;
	}

	widget = WID ("scrolledwindow1");

	self->connecting_devices = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								(GDestroyNotify) g_free,
								NULL);
	self->pairing_devices = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       NULL);
	self->devices_type = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify) g_free,
						    NULL);

	setup_pairing_agent (self);
	self->client = bluetooth_client_new ();
	g_signal_connect (self->client, "device-added",
			  G_CALLBACK (device_added_cb), self);
	g_signal_connect (self->client, "device-removed",
			  G_CALLBACK (device_removed_cb), self);
	devices_coldplug (self);

	g_signal_connect (G_OBJECT (self->client), "notify::default-adapter",
			  G_CALLBACK (default_adapter_changed), self);
	g_signal_connect (G_OBJECT (self->client), "notify::default-adapter-name",
			  G_CALLBACK (name_changed), self);
	g_signal_connect (G_OBJECT (self->client), "notify::default-adapter-powered",
			  G_CALLBACK (default_adapter_changed), self);
	default_adapter_changed (self->client, NULL, self);

	self->row_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	add_device_section (self);

	gtk_widget_set_hexpand (widget, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_append (GTK_BOX (self), widget);

	setup_properties_dialog (self);
	setup_obex (self);
}

static void
bluetooth_settings_widget_finalize (GObject *object)
{
	BluetoothSettingsWidget *self = BLUETOOTH_SETTINGS_WIDGET(object);

	g_clear_object (&self->agent);
	g_clear_pointer (&self->properties_dialog, gtk_window_destroy);
	g_clear_pointer (&self->pairing_dialog, gtk_window_destroy);
	g_clear_object (&self->session_proxy);

	obex_agent_down ();

	if (self->client)
		g_object_set (G_OBJECT (self->client), "default-adapter-setup-mode", FALSE, NULL);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);

	g_clear_object (&self->client);
	g_clear_object (&self->builder);

	g_clear_pointer (&self->devices_type, g_hash_table_destroy);
	g_clear_pointer (&self->connecting_devices, g_hash_table_destroy);
	g_clear_pointer (&self->pairing_devices, g_hash_table_destroy);
	g_clear_pointer (&self->selected_name, g_free);
	g_clear_pointer (&self->selected_object_path, g_free);

	G_OBJECT_CLASS(bluetooth_settings_widget_parent_class)->finalize(object);
}

static void
bluetooth_settings_widget_class_init (BluetoothSettingsWidgetClass *klass)
{
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);

	G_OBJECT_CLASS (klass)->finalize = bluetooth_settings_widget_finalize;

	/**
	 * BluetoothSettingsWidget::panel-changed:
	 * @chooser: a #BluetoothSettingsWidget widget which received the signal
	 * @panel: the new panel that the Settings application should now open
	 *
	 * The #BluetoothChooser::panel-changed signal is launched when a
	 * link to another settings panel is clicked.
	 **/
	signals[PANEL_CHANGED] =
		g_signal_new ("panel-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * BluetoothSettingsWidget::adapter-status-changed:
	 * @chooser: a #BluetoothSettingsWidget widget which received the signal
	 *
	 * The #BluetoothChooser::adapter-status-changed signal is launched when the status
	 * of the adapter changes (powered, available, etc.).
	 **/
	signals[ADAPTER_STATUS_CHANGED] =
		g_signal_new ("adapter-status-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0, G_TYPE_NONE);
}

/**
 * bluetooth_settings_widget_new:
 *
 * Returns a new #BluetoothSettingsWidget widget.
 *
 * Return value: A #BluetoothSettingsWidget widget
 **/
GtkWidget *
bluetooth_settings_widget_new (void)
{
	return g_object_new (BLUETOOTH_TYPE_SETTINGS_WIDGET, NULL);
}

/**
 * bluetooth_settings_widget_get_default_adapter_powered:
 * @widget: a #BluetoothSettingsWidget widget.
 *
 * Return value: Whether the default Bluetooth adapter is powered.
 **/
gboolean
bluetooth_settings_widget_get_default_adapter_powered (BluetoothSettingsWidget *self)
{
	gboolean ret;

	g_return_val_if_fail (BLUETOOTH_IS_SETTINGS_WIDGET (self), FALSE);

	g_object_get (G_OBJECT (self->client),
		      "default-adapter-powered", &ret,
		      NULL);

	return ret;
}

/**
 * bluetooth_settings_widget_set_default_adapter_powered:
 * @widget: a #BluetoothSettingsWidget widget.
 * @powered: whether the adapter should be powered
 *
 * Power up or down the default adapter.
 **/
void
bluetooth_settings_widget_set_default_adapter_powered (BluetoothSettingsWidget *self,
                                                       gboolean                 powered)
{
	g_return_if_fail (BLUETOOTH_IS_SETTINGS_WIDGET (self));

	g_object_set (G_OBJECT (self->client),
		      "default-adapter-powered", powered,
		      NULL);
}
