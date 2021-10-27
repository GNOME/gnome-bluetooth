#include "bluetooth-settings-widget.h"
#include <adwaita.h>

int main (int argc, char **argv)
{
	GtkWidget *window, *widget;

	gtk_init ();
	adw_init ();

	window = gtk_window_new ();
	gtk_widget_set_size_request (window, 300, 600);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, -1);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	widget = bluetooth_settings_widget_new ();
	gtk_window_set_child (GTK_WINDOW (window), widget);

	gtk_window_present(GTK_WINDOW(window));

	while (g_list_model_get_n_items (gtk_window_get_toplevels()) > 0)
		g_main_context_iteration (NULL, TRUE);

	return 0;
}
