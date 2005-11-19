
#include <gtk/gtk.h>
#include "gnomebt-spinner.h"

guint id = -1;

static gboolean spin_me (GnomebtSpinner *spinner)
{
	gnomebt_spinner_spin (spinner);
	return TRUE;
}

static void
on_start_clicked (GtkButton *button, GnomebtSpinner *spinner)
{
	g_message ("starting spinner");
	if (id != -1)
		return;

	id = g_timeout_add (500, (GSourceFunc) spin_me, spinner);
}

static void
on_stop_clicked (GtkButton *button, GnomebtSpinner *spinner)
{
	g_message ("stopping spinner");
	if (id != -1) {
		g_source_remove (id);
		gnomebt_spinner_reset (spinner);
	}
	id = -1;
}

int main (int argc, char **argv)
{
	GtkWidget *window, *box, *start, *stop;
	GnomebtSpinner *spinner;
	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	start = gtk_button_new_with_label ("Start");
	stop = gtk_button_new_with_label ("Stop");
	box = gtk_vbox_new (FALSE, 8);
	spinner = gnomebt_spinner_new ();

	gtk_container_add (GTK_CONTAINER (window), box);
	gtk_box_pack_end (GTK_BOX (box), start,
			FALSE, FALSE, 4);
	gtk_box_pack_end (GTK_BOX (box), stop,
			FALSE, FALSE, 4);
	gtk_box_pack_end (GTK_BOX (box), GTK_WIDGET (spinner),
			FALSE, FALSE, 4);

	g_signal_connect (G_OBJECT (start), "clicked",
			G_CALLBACK (on_start_clicked), spinner);
	g_signal_connect (G_OBJECT (stop), "clicked",
			G_CALLBACK (on_stop_clicked), spinner);

	gtk_widget_show_all (window);
	gtk_main ();

	return 0;
}

