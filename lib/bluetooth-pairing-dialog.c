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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>

#include "bluetooth-pairing-dialog.h"
#include "bluetooth-enums.h"
#include "gnome-bluetooth-enum-types.h"
#include "bluetooth-settings-resources.h"

#define BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), BLUETOOTH_TYPE_PAIRING_DIALOG, BluetoothPairingDialogPrivate))

typedef struct _BluetoothPairingDialogPrivate BluetoothPairingDialogPrivate;

struct _BluetoothPairingDialogPrivate {
	GtkBuilder           *builder;
	GtkWidget            *header;
	GtkWidget            *title;
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

G_DEFINE_TYPE(BluetoothPairingDialog, bluetooth_pairing_dialog, GTK_TYPE_DIALOG)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (priv->builder, s))

enum {
	CONFIRMATION_PAGE,
	DISPLAY_PAGE,
	MESSAGE_PAGE
};

void
bluetooth_pairing_dialog_set_mode (BluetoothPairingDialog *self,
				   BluetoothPairingMode    mode,
				   const char             *pin,
				   const char             *device_name)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	char *title;
	char *help;
	GtkStyleContext *context;

	priv->mode = mode;

	g_clear_pointer (&priv->pin, g_free);
	priv->pin = g_strdup (pin);
	gtk_entry_set_text (GTK_ENTRY (priv->entry_pin), pin ? pin : "");
	gtk_label_set_text (GTK_LABEL (priv->label_pin), pin);

	switch (mode) {
	case BLUETOOTH_PAIRING_MODE_PIN_QUERY:
		gtk_widget_show (priv->done);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), CONFIRMATION_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Please confirm the PIN that was entered on '%s'."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION:
		gtk_widget_show (priv->done);
		gtk_button_set_label (GTK_BUTTON (priv->done), _("Confirm"));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), CONFIRMATION_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Confirm the Bluetooth PIN for '%s'. This can usually be found in the device's manual."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL:
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD:
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE:
		gtk_widget_hide (priv->done);
		title = g_strdup_printf (_("Pairing '%s'"), device_name);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), DISPLAY_PAGE);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_MATCH:
		gtk_button_set_label (GTK_BUTTON (priv->done), _("Confirm"));
		gtk_widget_show (priv->done);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), DISPLAY_PAGE);
		title = g_strdup(_("Confirm Bluetooth PIN"));
		help = g_strdup_printf (_("Please confirm that the following PIN matches the one displayed on '%s'."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_YES_NO:
		gtk_widget_show (priv->done);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->pin_notebook), MESSAGE_PAGE);
		title = g_strdup (_("Bluetooth Pairing Request"));
		help = g_strdup_printf (_("'%s' wants to pair with this device. Do you want to allow pairing?"), device_name);
		break;
	default:
		g_assert_not_reached ();
	}

	switch (mode) {
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL:
		help = g_strdup_printf (_("Please enter the following PIN on '%s'."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD:
		help = g_strdup_printf (_("Please enter the following PIN on '%s'. Then press “Return” on the keyboard."), device_name);
		break;
	case BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE:
		help = g_strdup (_("Please move the joystick of your iCade in the following directions. Then press any of the white buttons."));
		break;
	default:
		g_assert (help);
	}

	if (mode == BLUETOOTH_PAIRING_MODE_YES_NO) {
		gtk_button_set_label (GTK_BUTTON (priv->done), _("Allow"));
		context = gtk_widget_get_style_context (priv->done);
		gtk_style_context_remove_class (context, "suggested-action");

		gtk_button_set_label (GTK_BUTTON (priv->cancel), _("Dismiss"));
		context = gtk_widget_get_style_context (priv->cancel);
		gtk_style_context_add_class (context, "destructive-action");

		gtk_widget_hide (priv->pin_notebook);
	} else {
		gtk_button_set_label (GTK_BUTTON (priv->done), _("Confirm"));
		context = gtk_widget_get_style_context (priv->done);
		gtk_style_context_add_class (context, "suggested-action");

		gtk_button_set_label (GTK_BUTTON (priv->cancel), _("Cancel"));
		context = gtk_widget_get_style_context (priv->cancel);
		gtk_style_context_remove_class (context, "destructive-action");

		gtk_widget_show (priv->pin_notebook);
	}

	gtk_label_set_text (GTK_LABEL (priv->title), title);
	gtk_label_set_text (GTK_LABEL (priv->help_label), help);
	g_free (title);
	g_free (help);
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

	return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry_pin)));
}

void
bluetooth_pairing_dialog_set_pin_entered (BluetoothPairingDialog *self,
					  guint                   entered)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	char *done;

	g_assert (priv->mode == BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD);
	g_assert (priv->pin);

	if (entered > 0) {
		gunichar invisible;
		GString *str;
		guint i;

		invisible = gtk_entry_get_invisible_char (GTK_ENTRY (priv->entry_pin));

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
	g_free (done);
}

static void
response_cb (GtkWidget *button,
	     gpointer   user_data)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (user_data);
	int response;

	if (button == priv->done)
		response = GTK_RESPONSE_ACCEPT;
	else if (button == priv->cancel)
		response = GTK_RESPONSE_CANCEL;
	else
		g_assert_not_reached ();

	gtk_dialog_response (GTK_DIALOG (user_data), response);
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

	text = gtk_entry_get_text (GTK_ENTRY (priv->entry_pin));
	if (!text || strlen (text) < 4)
		gtk_widget_set_sensitive (GTK_WIDGET (priv->done), FALSE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (priv->done), TRUE);
}

static void
bluetooth_pairing_dialog_init (BluetoothPairingDialog *self)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	GError *error = NULL;

	g_resources_register (bluetooth_settings_get_resource ());
	priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (priv->builder,
                                       "/org/gnome/bluetooth/settings.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Could not load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	gtk_widget_set_size_request (GTK_WIDGET (self), 380, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
}

static void
bluetooth_pairing_dialog_constructed (GObject *object)
{
	BluetoothPairingDialog *self = BLUETOOTH_PAIRING_DIALOG (object);
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (self);
	GtkWidget *container, *buttonbox;
	GtkStyleContext *context;

	container = gtk_dialog_get_content_area (GTK_DIALOG (self));
	buttonbox = gtk_dialog_get_action_area (GTK_DIALOG (self));

	/* Header */
	priv->header = gtk_header_bar_new ();
	priv->title = gtk_label_new ("");
	gtk_header_bar_set_custom_title (GTK_HEADER_BAR (priv->header), priv->title);

	/* OK button */
	priv->done = gtk_button_new_with_label (_("Accept"));
	gtk_widget_set_no_show_all (priv->done, TRUE);
	gtk_widget_set_can_default (GTK_WIDGET (priv->done), TRUE);
	g_signal_connect (G_OBJECT (priv->done), "clicked",
			  G_CALLBACK (response_cb), self);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header), priv->done);

	/* Spinner */
	priv->spinner = gtk_spinner_new ();
	gtk_widget_set_margin_end (priv->spinner, 12);
	gtk_widget_set_no_show_all (priv->spinner, TRUE);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header), priv->spinner);
	g_object_bind_property (priv->spinner, "visible",
				priv->spinner, "active", 0);
	g_object_bind_property (priv->spinner, "visible",
				priv->done, "visible",
				G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL);

	/* Cancel button */
	priv->cancel = gtk_button_new_with_label (_("Cancel"));
	g_signal_connect (G_OBJECT (priv->cancel), "clicked",
			  G_CALLBACK (response_cb), self);
	gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header), priv->cancel);
	gtk_widget_show_all (priv->header);
	gtk_window_set_titlebar (GTK_WINDOW (self), priv->header);
	gtk_widget_grab_default (GTK_WIDGET (priv->done));

	gtk_container_add (GTK_CONTAINER (container), WID ("pairing_dialog_box"));
	priv->help_label = WID ("help_label");
	priv->label_pin = WID ("label_pin");
	priv->entry_pin = WID ("entry_pin");
	g_signal_connect (G_OBJECT (priv->entry_pin), "notify::text",
			  G_CALLBACK (text_changed_cb), self);
	priv->pin_notebook = WID ("pin_notebook");
	gtk_widget_set_no_show_all (priv->pin_notebook, TRUE);
	gtk_widget_hide (buttonbox);
	gtk_widget_set_no_show_all (buttonbox, TRUE);

	context = gtk_widget_get_style_context (priv->done);
	gtk_style_context_add_class (context, "suggested-action");
	context = gtk_widget_get_style_context (priv->title);
	gtk_style_context_add_class (context, "title");
}

static void
bluetooth_pairing_dialog_finalize (GObject *object)
{
	BluetoothPairingDialogPrivate *priv = BLUETOOTH_PAIRING_DIALOG_GET_PRIVATE (object);

	g_clear_object (&priv->builder);
	g_free (priv->pin);

	G_OBJECT_CLASS(bluetooth_pairing_dialog_parent_class)->finalize(object);
}

static void
bluetooth_pairing_dialog_class_init (BluetoothPairingDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	g_type_class_add_private (klass, sizeof (BluetoothPairingDialogPrivate));

	object_class->constructed = bluetooth_pairing_dialog_constructed;
	object_class->finalize = bluetooth_pairing_dialog_finalize;
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
