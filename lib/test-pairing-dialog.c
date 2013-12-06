#include "bluetooth-pairing-dialog.h"

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
	g_message ("Received response '%d' (%s)",
		   response, response_to_str (response));

	if (response == GTK_RESPONSE_CANCEL ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (response != GTK_RESPONSE_DELETE_EVENT)
			gtk_widget_destroy (GTK_WIDGET (dialog));
		gtk_main_quit ();
		return;
	}

	if (bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (user_data)) == BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION) {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (user_data),
						   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL,
						   "234567",
						   "My device");
	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		gtk_main_quit ();
	}
}

int main (int argc, char **argv)
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	window = bluetooth_pairing_dialog_new ();
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (window),
					   BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION,
					   "123456",
					   "My device");
	g_signal_connect (G_OBJECT (window), "response",
			  G_CALLBACK (response_cb), window);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
