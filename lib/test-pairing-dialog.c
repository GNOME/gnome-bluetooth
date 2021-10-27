#include "bluetooth-pairing-dialog.h"
#include <adwaita.h>

static const char *
response_to_str (int response)
{
	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		return "accept";
	case GTK_RESPONSE_CANCEL:
		return "cancel";
	case GTK_RESPONSE_DELETE_EVENT:
		return "delete-event";
	default:
		g_message ("response %d unhandled", response);
		g_assert_not_reached ();
	}
}

static void
response_cb (GtkDialog *dialog,
	     int        response,
	     gpointer   user_data)
{
	GMainLoop *mainloop = user_data;
	g_message ("Received response '%d' (%s)",
		   response, response_to_str (response));

	if (response == GTK_RESPONSE_CANCEL ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (response != GTK_RESPONSE_DELETE_EVENT)
			gtk_window_destroy (GTK_WINDOW (dialog));
		g_main_loop_quit (mainloop);
		return;
	}

	if (bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (user_data)) == BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION) {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (user_data),
						   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL,
						   "234567",
						   "My device");
	} else {
		gtk_window_destroy (GTK_WINDOW (dialog));
		g_main_loop_quit (mainloop);
	}
}

int main (int argc, char **argv)
{
	g_autoptr(GMainLoop) mainloop = NULL;
	GtkWidget *window;
	BluetoothPairingMode mode;
	const char *pin = "123456";
	const char *device = "My device";

	gtk_init ();
	adw_init ();

	if (g_strcmp0 (argv[1], "pin-confirmation") == 0 ||
	    argv[1] == NULL) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION;
	} else if (g_strcmp0 (argv[1], "pin-display-keyboard") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD;
		pin = "123456⏎";
	} else if (g_strcmp0 (argv[1], "pin-display-icade") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE;
		pin = "⬆⬆⬅⬅➡➡❍";
	} else if (g_strcmp0 (argv[1], "pin-query") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_QUERY;
	} else if (g_strcmp0 (argv[1], "pin-match") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_MATCH;
	} else if (g_strcmp0 (argv[1], "yes-no") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_YES_NO;
	} else if (g_strcmp0 (argv[1], "confirm-auth") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH;
	} else {
		g_print ("Mode '%s' not supported, must be one of:\n", argv[1]);
		g_print ("\tpin-confirmation\n");
		g_print ("\tpin-display-keyboard\n");
		g_print ("\tpin-display-icade\n");
		g_print ("\tpin-query\n");
		g_print ("\tpin-match\n");
		g_print ("\tyes-no\n");
		g_print ("\tconfirm-auth\n");

		return 1;
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	window = bluetooth_pairing_dialog_new ();
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (window),
					   mode,
					   pin,
					   device);
	g_signal_connect (G_OBJECT (window), "response",
			  G_CALLBACK (response_cb), mainloop);

	gtk_widget_show (window);

	g_main_loop_run (mainloop);

	return 0;
}
