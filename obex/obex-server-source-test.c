
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <glib.h>

#include "obex-server-source.h"

/* main callback handler */

int
main(int argc, char *argv[])
{
    GMainLoop *loop;
    GnomebtObexserverSource *gobsrc;

    printf("Ensure channel 4 is registered as OPUSH with sdptool\n");
    printf("Then beam files at me.\n");
    printf("Hit Ctrl-C to quit.\n");

    loop = g_main_loop_new (NULL, FALSE);

    gobsrc = gnomebt_obexserver_source_new ();

    if (gobsrc) {

        gnomebt_obexserver_source_attach (gobsrc, NULL);

        /*
        g_timeout_add (10*1000, (GSourceFunc)g_main_loop_quit,
        (gpointer)loop);
        */

        g_main_loop_run (loop);

        gnomebt_obexserver_source_destroy (gobsrc);
        gnomebt_obexserver_source_unref (gobsrc);

        g_main_loop_unref (loop);
    } else {
        g_error ("couldn't create source");
    }

	return 0;
}

