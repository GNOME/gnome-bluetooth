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
response_cb (BluetoothPairingDialog *dialog,
	     int                     response,
	     gpointer                user_data)
{
	GMainLoop *mainloop = user_data;
	g_message ("Received response '%d' (%s)",
		   response, response_to_str (response));

	if (response == GTK_RESPONSE_CANCEL ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (response != GTK_RESPONSE_DELETE_EVENT)
			adw_dialog_close (ADW_DIALOG (dialog));
		g_main_loop_quit (mainloop);
		return;
	}

	if (bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (user_data)) == BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION) {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (user_data),
						   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL,
						   "234567",
						   "My device");
	} else {
		adw_dialog_close (ADW_DIALOG (dialog));
		g_main_loop_quit (mainloop);
	}
}

int main (int argc, char **argv)
{
	g_autoptr(GMainLoop) mainloop = NULL;
	GtkWidget *window;
	GtkWidget *dialog;
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

	window = adw_window_new ();
	gtk_window_set_default_size (GTK_WINDOW (window), 360, 600);
	dialog = bluetooth_pairing_dialog_new ();
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (dialog),
					   mode,
					   pin,
					   device);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_cb), mainloop);

	gtk_window_present (GTK_WINDOW (window));
	adw_dialog_present (ADW_DIALOG (dialog), window);

	g_main_loop_run (mainloop);

	return 0;
}
