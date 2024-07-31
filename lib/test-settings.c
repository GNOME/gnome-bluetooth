#include "bluetooth-settings-widget.h"
#include <glib/gi18n.h>
#include <adwaita.h>

int main (int argc, char **argv)
{
        GtkWidget *window, *toolbarview, *headerbar, *widget;

	adw_init ();

	window = adw_window_new ();
        gtk_window_set_title (GTK_WINDOW (window), "Bluetooth");
	gtk_widget_set_size_request (window, 300, 600);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, -1);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

        toolbarview = adw_toolbar_view_new ();
  	adw_window_set_content (ADW_WINDOW (window), toolbarview);

        headerbar = adw_header_bar_new ();
        adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbarview), headerbar);

	widget = bluetooth_settings_widget_new ();
        adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbarview), widget);

	gtk_window_present (GTK_WINDOW (window));

	while (g_list_model_get_n_items (gtk_window_get_toplevels()) > 0)
		g_main_context_iteration (NULL, TRUE);

	return 0;
}
