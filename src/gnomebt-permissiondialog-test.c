
#include <stdlib.h>

#include "gnomebt-permissiondialog.h"

int main(int argc, char **argv)
{
    GnomebtController *btctl;
    GnomebtPermissionDialog *dlg;
    gint result;
    
    gtk_init (&argc, &argv);
    btctl = gnomebt_controller_new ();
    dlg = gnomebt_permissiondialog_new (btctl, 
            "00:80:37:2A:B6:BC", "File transfer requested",
            "<span weight='bold' size='larger'>Accept a file from '%s'?</span>\n\nThe remote device is attempting to send you a file via Bluetooth.",
			"Always accept files from this device.");
    result = gtk_dialog_run (GTK_DIALOG (dlg));
    switch (result) {
        case GTK_RESPONSE_OK:
            g_message ("user said yes");
            break;
        default:
            g_message ("user said no");
            break;
    }
    if (gnomebt_permissiondialog_remember(dlg)) {
        g_message ("remember this result");
    }
    
    gtk_widget_destroy(GTK_WIDGET(dlg));
    g_object_unref(btctl);

    return 0;
}

