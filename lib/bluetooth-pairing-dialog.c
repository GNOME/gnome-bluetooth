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

#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>

#include "bluetooth-pairing-dialog.h"
#include "bluetooth-enums.h"
#include "gnome-bluetooth-enum-types.h"
#include "bluetooth-settings-resources.h"

#define BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE(obj) \
	(bluetooth_pairing_dialog_get_instance_private (obj))

typedef struct _BluetoothPairingDialogPrivate BluetoothPairingDialogPrivate;

struct _BluetoothPairingDialogPrivate {
	GtkWidget            *help_label;
	GtkWidget            *label_pin;
	GtkWidget            *entry_pin;
	GtkWidget            *pin_notebook;
	GtkWidget            *done;
	GtkWidget            *spinner;
	GtkWidget            *cancel;

	BluetoothPairingMode  mode;
	char                 *pin;
};

G_DEFINE_TYPE_WITH_PRIVATE(BluetoothPairingDialog, bluetooth_pairing_dialog, ADW_TYPE_DIALOG)

enum {
	CONFIRMATION_PAGE,
	DISPLAY_PAGE,
	MESSAGE_PAGE
};

enum {
  RESPONSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

void
bluetooth_pairing_dialog_set_mode (BluetoothPairingDialog *self,
				   BluetoothPairingMode    mode,
				   const char             *pin,
				   const char             *device_name)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	g_autofree char *title = NULL;
	g_autofree char *help = NULL;

	priv->mode = mode;

	g_clear_pointer (&priv->pin, g_free);
	priv->pin = g_strdup (pin);
	gtk_editable_set_text (GTK_EDITABLE (priv->entry_pin), pin ? pin : "");
	gtk_entry_set_visibility (GTK_ENTRY (priv->entry_pin), TRUE);
	gtk_label_set_text (GTK_LABEL (priv->label_pin), pin);

	switch (mode) {
	case BLUETOOTH_PAIRING_MODE_PIN_QUERY:
		gtk_widget_set_visible (priv->done, TRUE);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), CONFIRMATION_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Please confirm the PIN that was entered on “%s”."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION:
		gtk_widget_set_visible (priv->done, TRUE);
		gtk_button_set_label (GTK_BUTTON (priv->done), _("C_onfirm"));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), CONFIRMATION_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Confirm the Bluetooth PIN for “%s”. This can usually be found in the device’s manual."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL:
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD:
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE:
		gtk_widget_set_visible (priv->done, FALSE);
		title = g_strdup_printf (_("Pairing “%s”"), device_name);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), DISPLAY_PAGE);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_MATCH:
		gtk_button_set_label (GTK_BUTTON (priv->done), _("C_onfirm"));
		gtk_widget_set_visible (priv->done, TRUE);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), DISPLAY_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Please confirm that the following PIN matches the one displayed on “%s”."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_YES_NO:
		gtk_widget_set_visible (priv->done, TRUE);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), MESSAGE_PAGE);
		title = g_strdup (_("Bluetooth Pairing Request"));
		help = g_strdup_printf (_("“%s” wants to pair with this device. Do you want to allow pairing?"), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH:
		gtk_widget_set_visible (priv->done, TRUE);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), MESSAGE_PAGE);
		title = g_strdup (_("Confirm Bluetooth Connection"));
		help = g_strdup_printf (_("“%s” wants to connect with this device. Do you want to allow it?"), device_name);
		break;
	default:
		g_assert_not_reached ();
	}

	switch (mode) {
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL:
		help = g_strdup_printf (_("Please enter the following PIN on “%s”."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD:
		help = g_strdup_printf (_("Please enter the following PIN on “%s”. Then press “Return” on the keyboard."), device_name);
		/* populate the invisible character */
		gtk_entry_set_visibility (GTK_ENTRY (priv->entry_pin), FALSE);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE:
		help = g_strdup (_("Please move the joystick of your iCade in the following directions. Then press any of the white buttons."));
		break;
	default:
		g_assert (help);
	}

	if (mode == BLUETOOTH_PAIRING_MODE_YES_NO ||
	    mode == BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH) {
		gtk_button_set_label (GTK_BUTTON (priv->done), _("_Allow"));
		gtk_widget_remove_css_class (priv->done, "suggested-action");

		gtk_button_set_label (GTK_BUTTON (priv->cancel), _("_Dismiss"));
		gtk_widget_add_css_class (priv->cancel, "destructive-action");

		gtk_widget_set_visible (priv->pin_notebook, FALSE);
	} else {
		gtk_button_set_label (GTK_BUTTON (priv->done), _("C_onfirm"));
		gtk_widget_add_css_class (priv->done, "suggested-action");

		gtk_button_set_label (GTK_BUTTON (priv->cancel), _("_Cancel"));
		gtk_widget_remove_css_class (priv->cancel, "destructive-action");

		gtk_widget_set_visible (priv->pin_notebook, TRUE);
	}

	adw_dialog_set_title (ADW_DIALOG (self), title);
	gtk_label_set_text (GTK_LABEL (priv->help_label), help);
}

BluetoothPairingMode
bluetooth_pairing_dialog_get_mode (BluetoothPairingDialog *self)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);

	return priv->mode;
}

char *
bluetooth_pairing_dialog_get_pin (BluetoothPairingDialog *self)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);

	g_assert (priv->mode == BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION ||
		  priv->mode == BLUETOOTH_PAIRING_MODE_PIN_QUERY);
	g_assert (gtk_widget_is_sensitive (GTK_WIDGET (priv->done)));

	return g_strdup (gtk_editable_get_text (GTK_EDITABLE (priv->entry_pin)));
}

void
bluetooth_pairing_dialog_set_pin_entered (BluetoothPairingDialog *self,
					  guint                   entered)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	g_autofree char *done = NULL;

	g_assert (priv->mode == BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD);
	g_assert (priv->pin);

	if (entered > 0) {
		gunichar invisible;
		GString *str;
		guint i;

		invisible = gtk_entry_get_invisible_char (GTK_ENTRY (priv->entry_pin));
		if (invisible == 0)
			invisible = '*';

		str = g_string_new (NULL);
		for (i = 0; i < entered; i++)
			g_string_append_unichar (str, invisible);
		if (entered < strlen (priv->pin))
			g_string_append (str, priv->pin + entered);

		done = g_string_free (str, FALSE);
	} else {
		done = g_strdup (priv->pin);
	}

	gtk_label_set_text (GTK_LABEL (priv->label_pin), done);
}

static void
response_cb (GtkWidget *button,
	     gpointer   user_data)
{
	BluetoothPairingDialog *self = user_data;
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	int response;

	if (button == priv->done)
		response = GTK_RESPONSE_ACCEPT;
	else if (button == priv->cancel)
		response = GTK_RESPONSE_CANCEL;
	else
		g_assert_not_reached ();

	adw_dialog_set_can_close (ADW_DIALOG (self), TRUE);
	g_signal_emit (self, signals[RESPONSE], 0, response);
}

static void
close_attempt_cb (AdwDialog *dialog)
{
	g_signal_emit (dialog, signals[RESPONSE], 0, GTK_RESPONSE_DELETE_EVENT);
}

static void
text_changed_cb (GObject    *gobject,
		 GParamSpec *pspec,
		 gpointer    user_data)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (user_data);
	const char *text;

	if (priv->mode != BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION &&
	    priv->mode != BLUETOOTH_PAIRING_MODE_PIN_QUERY)
		return;

	text = gtk_editable_get_text (GTK_EDITABLE (priv->entry_pin));
	if (!text || strlen (text) < 4)
		gtk_widget_set_sensitive (GTK_WIDGET (priv->done), FALSE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (priv->done), TRUE);
}

static void
bluetooth_pairing_dialog_init (BluetoothPairingDialog *self)
{
	g_autoptr(GtkCssProvider) provider = NULL;

	gtk_widget_init_template (GTK_WIDGET (self));

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (provider, "/org/gnome/bluetooth/bluetooth-settings.css");
	gtk_style_context_add_provider_for_display (gdk_display_get_default (),
						    GTK_STYLE_PROVIDER (provider),
						    GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static void
bluetooth_pairing_dialog_constructed (GObject *object)
{
	BluetoothPairingDialog *self = BLUETOOTH_PAIRING_DIALOG (object);
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);

	G_OBJECT_CLASS(bluetooth_pairing_dialog_parent_class)->constructed (object);

	g_signal_connect (G_OBJECT (priv->entry_pin), "notify::text",
			  G_CALLBACK (text_changed_cb), self);

	gtk_widget_add_css_class (priv->done, "suggested-action");
}

static void
bluetooth_pairing_dialog_finalize (GObject *object)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (BLUETOOTH_PAIRING_DIALOG (object));

	g_free (priv->pin);

	G_OBJECT_CLASS(bluetooth_pairing_dialog_parent_class)->finalize(object);
}

static void
bluetooth_pairing_dialog_class_init (BluetoothPairingDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	object_class->constructed = bluetooth_pairing_dialog_constructed;
	object_class->finalize = bluetooth_pairing_dialog_finalize;

	signals[RESPONSE] = g_signal_new ("response",
					  G_OBJECT_CLASS_TYPE (klass),
					  G_SIGNAL_RUN_LAST,
					  0,
					  NULL, NULL,
					  NULL,
					  G_TYPE_NONE, 1,
					  G_TYPE_INT);

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/bluetooth/bluetooth-pairing-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, help_label);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, pin_notebook);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, entry_pin);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, label_pin);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, done);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, spinner);
	gtk_widget_class_bind_template_child_private (widget_class, BluetoothPairingDialog, cancel);
	gtk_widget_class_bind_template_callback (widget_class, response_cb);
	gtk_widget_class_bind_template_callback (widget_class, close_attempt_cb);
}

/**
 * bluetooth_pairing_dialog_new:
 *
 * Returns a new #BluetoothPairingDialog widget.
 *
 * Return value: A #BluetoothPairingDialog widget
 **/
GtkWidget *
bluetooth_pairing_dialog_new (void)
{
	return g_object_new (BLUETOOTH_TYPE_PAIRING_DIALOG, NULL);
}
