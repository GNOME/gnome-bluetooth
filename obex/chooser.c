/*
 * gnome-obex-send
 * Copyright (c) 2003-4 Edd Dumbill <edd@usefulinc.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
    result = gtk_dialog_run (GTK_DIALOG (btchooser));

    if (result == GTK_RESPONSE_OK)
        ret = gnomebt_chooser_get_bdaddr (btchooser);

    gtk_widget_destroy (GTK_WIDGET (btchooser));

	/* consume events, so dialog gets removed from screen */
	while (gtk_events_pending ())
		gtk_main_iteration ();

    g_object_unref (G_OBJECT (btctl));
    return ret;
}
