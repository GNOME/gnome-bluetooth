
#include <stdlib.h>

#include "gnomebt-chooser.h"

int main(int argc, char **argv)
{
    GnomebtController *btctl;
    GnomebtChooser *chooser;
    gint result;
    gchar *choice;
    
    gtk_init (&argc, &argv);
    btctl = gnomebt_controller_new ();
    chooser = gnomebt_chooser_new (btctl);
    result = gtk_dialog_run (GTK_DIALOG (chooser));
    switch (result) {
        case GTK_RESPONSE_OK:
            choice = gnomebt_chooser_get_bdaddr (chooser);
            g_message ("user chose %s", choice);
            g_free (choice);
            break;
        default:
            g_message ("user cancelled choice");
            break;
    }
    
    gtk_widget_destroy(GTK_WIDGET(chooser));
    g_message ("choice widget unreffed");
    g_object_unref(btctl);

    return 0;
}
