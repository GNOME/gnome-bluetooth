/*
 *  GSource source for OBEX
 *
 *  Copyright (C) 2004  Edd Dumbill <edd@usefulinc.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

#include <glib.h>

#include "obex-server-source.h"
#include "obex-server-source-private.h"


/* GSourceFuncs */

static gboolean gnomebt_obexserver_source_prepare (GSource *source, gint *timeout);
static gboolean gnomebt_obexserver_source_check (GSource *source);
static gboolean gnomebt_obexserver_source_dispatch (GSource *source,
        GnomebtObexserverSourceFunc callback, gpointer user_data);

static void get_peer_bdaddr (GnomebtObexserverSource *s, bdaddr_t *addr);
static void close_client_connection (GnomebtObexserverSource *s);
static void gnomebt_obexserver_source_finalize (GSource *source);
static void obex_event (obex_t *handle, obex_object_t *object,
        int mode, int event, int obex_cmd, int obex_rsp);
static void server_request(GnomebtObexserverSource *s, obex_object_t *object,
        int event, int cmd);

/* externs from obex lib */
extern int obex_transport_accept(obex_t *self);
extern int obex_data_indication(obex_t *self, char *buf, int buflen);
extern void obex_deliver_event(obex_t *self, int event, int cmd, int rsp,
        int del);

static GSourceFuncs gobex_source = {
    gnomebt_obexserver_source_prepare,
    gnomebt_obexserver_source_check,
    (gboolean (*)(GSource *, GSourceFunc, gpointer))
        gnomebt_obexserver_source_dispatch,
    gnomebt_obexserver_source_finalize,
    NULL,
    NULL
};

static void
server_request (GnomebtObexserverSource *s, obex_object_t *obj,
        int event, int cmd)
{
    switch (cmd) {
        case OBEX_CMD_CONNECT:
        case OBEX_CMD_DISCONNECT:
            OBEX_ObjectSetRsp(obj, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
            break;

        case OBEX_CMD_PUT:
            g_message ("PUT happened");
            OBEX_ObjectSetRsp(obj, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
            break;

        default:
            g_warning ("Rejecting request %d", cmd);
            OBEX_ObjectSetRsp(obj, OBEX_RSP_NOT_IMPLEMENTED, 
                    OBEX_RSP_NOT_IMPLEMENTED);
            break;
    }
}

static void
obex_event (obex_t *handle, obex_object_t *object,
        int mode, int event, int obex_cmd, int obex_rsp)
{
    GnomebtObexserverSource *s =(GnomebtObexserverSource*)
        OBEX_GetUserData(handle);

    switch (event) {
        case OBEX_EV_PROGRESS:
            g_message ("Progress");
            break;

        case OBEX_EV_REQHINT:
            g_message ("Request hint, cmd %d", obex_cmd);
            switch(obex_cmd) {
                case OBEX_CMD_PUT:
                    g_message ("Wants to PUT");
                    OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
                    break;
                case OBEX_CMD_CONNECT:
                case OBEX_CMD_DISCONNECT:
                    OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
                    break;
                default:
                    OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_IMPLEMENTED,
                            OBEX_RSP_NOT_IMPLEMENTED);
                    break;
            }
            break;

        case OBEX_EV_REQ:
            g_message ("Incoming request, command %d", obex_cmd);
            server_request (s, object, event, obex_cmd);
            break;

        case OBEX_EV_REQDONE:
            g_message ("Request finished");
            break;
            
        case OBEX_EV_PARSEERR:
            g_warning ("Malformed incoming data");
            break;

        case OBEX_EV_ABORT:
            g_message ("Request aborted");
            break;

        case OBEX_EV_STREAMEMPTY:
            g_message ("OBEX_EV_STREAMEMPTY");
            break;

        case OBEX_EV_STREAMAVAIL:
            g_message ("OBEX_EV_STREAMAVAIL");
            break;

        case OBEX_EV_LINKERR:
            g_warning ("OBEX_EV_LINKERR");
            break;

        default:
            g_warning ("Unhandled event %d", event);
            break;
    }
}


static gboolean
gnomebt_obexserver_source_prepare (GSource *source, gint *timeout)
{
    return FALSE;
}

static void
get_peer_bdaddr (GnomebtObexserverSource *s, bdaddr_t *addr)
{
    struct sockaddr_rc sa;
    socklen_t salen;

    salen = sizeof (sa);
    if (getpeername (s->fd.fd, (struct sockaddr *)&sa, &salen) < 0)
    {
        g_error ("couldn't get peer name, err %d", errno);
    }
    bacpy(addr, &sa.rc_bdaddr);
}

/* warning: this function uses internals from openobex */

static void
close_client_connection (GnomebtObexserverSource *s)
{
    struct obex *obex_priv;

    g_message ("disconnecting client");
    OBEX_TransportDisconnect (s->handle);
    g_source_remove_poll ((GSource*) s, &s->fd);
    s->fd.fd = -1;
    obex_priv = (struct obex *)s->handle;
    obex_priv->state = MODE_SRV | STATE_IDLE;
    if (obex_priv->object != NULL) {
        /* no idea why this sometimes isn't null */
        OBEX_ObjectDelete (s->handle, obex_priv->object);
        obex_priv->object = NULL;
    }
}

static gboolean
gnomebt_obexserver_source_check (GSource *source)
{
    GnomebtObexserverSource *s = (GnomebtObexserverSource*)source;
    int ret;

    if (!s->initialised)
        return FALSE;

    /* some boring debug checks */
    if (s->serverfd.revents & G_IO_ERR)
        g_message ("G_IO_ERR on server");
    if (s->serverfd.revents & G_IO_IN)
        g_message ("G_IO_IN on server");
    if (s->serverfd.revents & G_IO_HUP)
        g_message ("G_IO_HUP on server");

    if (s->fd.fd >= 0 && (s->fd.revents & G_IO_ERR))
        g_message ("G_IO_ERR on client");
    if (s->fd.fd >= 0 && (s->fd.revents & G_IO_IN))
        g_message ("G_IO_IN on client");
    if (s->fd.fd >= 0 && (s->fd.revents & G_IO_HUP))
        g_message ("G_IO_HUP on client");

    if (s->serverfd.revents & G_IO_IN) {
        /* an event happened on the server file descriptor */
        g_message ("accepting client connection");
        /* accept the connection and create a new client socket */
        ret = obex_transport_accept (s->handle);

        if (ret < 0) {
            g_warning ("error while accepting client connection");
            return FALSE;
        } else {
            /* now ensure we poll on this one too: GetFD magically knows
             * to return the client FD when one is available */
            s->fd.fd = OBEX_GetFD (s->handle);
            s->fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
            s->fd.revents = 0;

            /* store away friendly details of our peer */
            get_peer_bdaddr (s, &s->peer_bdaddr);
            ba2str (&s->peer_bdaddr, s->peer_name);

            g_message ("accepted peer is %s", s->peer_name);
            g_source_add_poll ((GSource*) s, &s->fd);
        }

        /* we may choose, at this point, to notify via callback
         * that an acceptance of a socket has been made */
 
        return TRUE;
    } else if (s->fd.fd >= 0) {
        if (s->fd.revents & G_IO_IN) {
            /* an event on the client descriptor */

            /* read the data: this dispatches a bunch of stuff
             * to the obex_event handler. */

            ret = obex_data_indication (s->handle, NULL, 0);

            return TRUE;
        } else if (s->fd.revents & G_IO_HUP) {
            /* end of transmission */
            close_client_connection (s);
        }
    }

    return FALSE;
}

static gboolean
gnomebt_obexserver_source_dispatch (GSource *source,
        GnomebtObexserverSourceFunc callback,
        gpointer user_data)
{
    GnomebtObexserverSource *s = (GnomebtObexserverSource*)source;
    g_message ("gnomebt_obexserver_source_dispatch");

    if (callback) {
        (*callback)(s, 0, user_data);
    }
    return TRUE;
}

static void
gnomebt_obexserver_source_finalize (GSource *source)
{
    GnomebtObexserverSource *s = (GnomebtObexserverSource*)source;
    g_message ("gnomebt_obexserver_source_finalize");

    if (! s->initialised)
        return;

    OBEX_Cleanup (s->handle);
}

void
gnomebt_obexserver_source_destroy (GnomebtObexserverSource *source)
{
    g_message ("gnomebt_obexserver_source_destroy");
    g_source_destroy ((GSource *)source);
}

void
gnomebt_obexserver_source_attach (GnomebtObexserverSource *source,
        GMainContext *ctxt)
{
    g_source_attach ((GSource *)source, ctxt);
}

GnomebtObexserverSource *
gnomebt_obexserver_source_new ()
{
    GnomebtObexserverSource *source;
    int err;

    source = (GnomebtObexserverSource*) g_source_new (
		    &gobex_source, sizeof(GnomebtObexserverSource));
    if (!source) {
	    g_assert_not_reached();
	    return NULL;
    }

    source->initialised = FALSE;

    source->handle = OBEX_Init (OBEX_TRANS_BLUETOOTH, obex_event, 0);

    if (! source->handle) {
        gnomebt_obexserver_source_unref (source);
        return NULL;
    }

    /* ensure handler routines can get back at the GSource if
     * they need to. */
    OBEX_SetUserData (source->handle, (void *)source);

    /* register on channel 4 */
    err = BtOBEX_ServerRegister (source->handle, BDADDR_ANY, 4);

    if (err < 0) {
        g_warning ("OBEX server register error: %d", err);
        gnomebt_obexserver_source_unref (source);
        return NULL;
    }

    source->initialised = TRUE;

    source->fd.fd = -1;

    source->serverfd.fd = OBEX_GetFD (source->handle);
    source->serverfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
    source->serverfd.revents = 0;

    g_source_add_poll ((GSource*) source, &source->serverfd);

    return source;
}

void
gnomebt_obexserver_source_set_callback (GnomebtObexserverSource *source,
        GnomebtObexserverSourceFunc func, gpointer data,
        GDestroyNotify notify)
{
    g_source_set_callback ((GSource*)source,
            (GSourceFunc)func, data, notify);
}
