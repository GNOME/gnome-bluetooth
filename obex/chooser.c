/*
  Copyright (c) 2003 Edd Dumbill.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gnomebt-chooser.h"
#include "chooser.h"

gchar *
choose_bdaddr ()
{
    gchar *ret = NULL;
    GnomebtController *btctl;
    GnomebtChooser *btchooser;
    gint result;

    btctl = gnomebt_controller_new ();
    btchooser = gnomebt_chooser_new (btctl);
    result = gtk_dialog_run (GTK_DIALOG(btchooser));
    if (result == GTK_RESPONSE_OK)
        ret = gnomebt_chooser_get_bdaddr (btchooser);
    gtk_widget_destroy (GTK_WIDGET(btchooser));
    g_object_unref (G_OBJECT (btctl));
    return ret;
}
