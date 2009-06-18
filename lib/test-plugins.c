#include <gtk/gtk.h>
#include "bluetooth-plugin-manager.h"

int main (int argc, char **argv)
{
	GtkWidget *window, *vbox;
	GList *list, *l;
	const char *uuids[] = { "PANU", NULL};

	gtk_init (&argc, &argv);

	bluetooth_plugin_manager_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	list = bluetooth_plugin_manager_get_widgets ("11:22:33:44:55:66", uuids);
	if (list == NULL) {
		g_message ("no plugins");
		return 1;
	}

	for (l = list; l != NULL; l = l->next) {
		GtkWidget *widget = l->data;
		gtk_container_add (GTK_CONTAINER (vbox), widget);
	}

	gtk_widget_show_all (window);

	gtk_main ();

	gtk_widget_destroy (window);

	bluetooth_plugin_manager_cleanup ();

	return 0;
}
