#ifndef _GNOMEBT_OBEXSERVER_SOURCE_H
#define _GNOMEBT_OBEXSERVER_SOURCE_H

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <glib.h>
#include <openobex/obex.h>

G_BEGIN_DECLS

typedef struct _GnomebtObexserverSource GnomebtObexserverSource;

struct _GnomebtObexserverSource {
    GSource gsource;
    GPollFD fd;
    GPollFD serverfd;
    gboolean initialised;
    obex_t *handle;
    bdaddr_t peer_bdaddr;
    char peer_name[20];
};

typedef struct _GnomebtObexserverEvent GnomebtObexserverEvent;

struct _GnomebtObexserverEvent {
    int cmd;
    void *data;
};

enum {
    GNOMEBT_OBEXSERVER_PREPUSH,
    GNOMEBT_OBEXSERVER_PUSH
};

typedef gboolean (*GnomebtObexserverSourceFunc) 
    (GnomebtObexserverSource *source, GnomebtObexserverEvent *event,
     gpointer data);

#define gnomebt_obexserver_source_ref(x) g_source_ref((GSource*)x)
#define gnomebt_obexserver_source_unref(x) g_source_unref((GSource*)x)

void gnomebt_obexserver_source_destroy (GnomebtObexserverSource *source);
void gnomebt_obexserver_source_attach (GnomebtObexserverSource *source,
        GMainContext *ctxt);
GnomebtObexserverSource * gnomebt_obexserver_source_new (void);
void gnomebt_obexserver_source_set_callback (GnomebtObexserverSource *source,
        GnomebtObexserverSourceFunc func, gpointer data,
        GDestroyNotify notify);

G_END_DECLS

#endif /* _GNOMEBT_OBEXSERVER_SOURCE_H */
