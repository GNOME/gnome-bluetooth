#ifndef OBEX_TEST_H
#define OBEX_TEST_H

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome.h>

struct context
{
  int serverdone;
  int clientdone;
  char *get_name;	/* Name of last get-request */
  void *app;
  const char *save_dir;
  int to_send;
  int sent;
  void (*update)(void *);
  int fileDesc;
  int error;
  int usercancel;
};

#include "gnomebt-spinner.h"
#include "gnomebt-icons.h"
#include "gnomebt-controller.h"
#include "gnomebt-permissiondialog.h"

typedef struct _appinfo {
  /* stuff needed for client and server */
  struct context *gt;
  obex_t *handle;
  GnomebtController *btctl;
  /* stuff just for the server */
  GnomebtSpinner *spin;
  GtkMenu *menu;
  GtkTooltips *tooltip;
    /* stuff just for the client */
  GladeXML *xml;
  poptContext context;
} MyApp;

#endif
