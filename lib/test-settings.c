#include "bluetooth-settings-widget.h"

static gboolean
delete_event_cb (GtkWidget *widget,
		 GdkEvent  *event,
		 gpointer   user_data)
{
	gtk_main_quit ();
	return FALSE;
}

int main (int argc, char **argv)
{
	GtkWidget *window, *widget;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window), "delete-event",
			  G_CALLBACK (delete_event_cb), NULL);
	widget = bluetooth_settings_widget_new ();
	gtk_container_add (GTK_CONTAINER (window), widget);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
