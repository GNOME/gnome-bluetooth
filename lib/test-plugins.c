#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include "bluetooth-plugin-manager.h"

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
	GtkWidget *window, *vbox;
	GList *list, *l;
	DBusGConnection *bus;
	const char *uuids[] = { "PANU", NULL};

	gtk_init (&argc, &argv);

	/* Init the dbus-glib types */
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	dbus_g_connection_unref (bus);

	bluetooth_plugin_manager_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window), "delete-event",
			  G_CALLBACK (delete_event_cb), NULL);
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
