/*
  Copyright (c) 2003 Edd Dumbill.
  Portions copyright (c) 2000, Pontus Fuchs, All Rights Reserved.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include <openobex/obex.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>
#include <gnome.h>

#include "obex_test.h"
#include "obex_test_client.h"
#include "obex_test_server.h"

#include "obexsdp.h"
#include "eggtrayicon.h"

#define IR_SERVICE "OBEX"
#define BT_CHANNEL 4

void obex_event(obex_t *handle, obex_object_t *object,
				int mode, int event, int obex_cmd, int obex_rsp);
int get_peer_addr(char *name, struct sockaddr_in *peer) ;
gboolean main2 (gpointer data);
gboolean obex_poll (gpointer data);
gboolean my_init (gpointer data);
int inet_connect(obex_t *handle);
void my_server_do(void);

static MyApp *app;

static gboolean allowed_to_put (gpointer data, gchar *bdaddr);
 
static gboolean
allowed_to_put (gpointer data, gchar *bdaddr)
{
    struct context *ctxt = (struct context *)data;
    MyApp *app = (MyApp *)ctxt->app;
    GnomebtPermissionDialog *dlg;
    gboolean result;
  
    g_message ("in allowed_to_put");
  
    dlg = gnomebt_permissiondialog_new (app->btctl,
            bdaddr, "File transfer requested",
            "<span weight='bold' size='larger'>Accept a file from '%s'?</span>\n\nThe remote device is attempting to send you a file via Bluetooth.");
    result = gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (GTK_WIDGET (dlg));
    return (result == GTK_RESPONSE_OK);
}

//
// Called by the obex-layer when some event occurs.
//
void
obex_event(obex_t *handle, obex_object_t *object,
		   int mode, int event, int obex_cmd, int obex_rsp)
{
    struct context *gt;
	gt = OBEX_GetUserData(handle);

	switch (event)	{
	case OBEX_EV_PROGRESS:
		gnomebt_spinner_spin(app->spin);
		break;

	case OBEX_EV_ABORT:
		printf("Request aborted!\n");
		break;

	case OBEX_EV_LINKERR:
	  OBEX_TransportDisconnect(handle);
	  g_message("Link error.");
	case OBEX_EV_REQDONE:
	  if(mode == OBEX_CLIENT) {
		client_done(handle, object, obex_cmd, obex_rsp);
	  }	else {
		server_done(handle, object, obex_cmd, obex_rsp);
	  }
	  break;

	case OBEX_EV_REQHINT:
        switch (obex_cmd) {
            case OBEX_CMD_PUT:
                if (allowed_to_put ((gpointer) gt, "00:00:00:00:00:00")) {
                    OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE,
                            OBEX_RSP_SUCCESS);
                } else {
                    g_message ("ReqHint: Denying PUT on user request");
                    OBEX_ObjectSetRsp(object, OBEX_RSP_FORBIDDEN,
                        OBEX_RSP_FORBIDDEN);
                }
                break;
            case OBEX_CMD_CONNECT:
            case OBEX_CMD_DISCONNECT:
                OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
                break;
            case OBEX_CMD_GET:
            case OBEX_CMD_SETPATH:
            default:
                OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_IMPLEMENTED,
                        OBEX_RSP_NOT_IMPLEMENTED);
                g_message ("ReqHint: denying command %02x", obex_cmd);
                break;
        }
		break;

	case OBEX_EV_REQ:
		server_request(handle, object, event, obex_cmd);
		break;

	/*case OBEX_EV_LINKERR:
	  OBEX_TransportDisconnect(handle); 
	  printf("Link broken!\n");
	  app->gt->serverdone = TRUE;
	  break;*/

	case OBEX_EV_STREAMEMPTY:
		fillstream(handle, object);
		break;

	default:
		printf("Unknown event %02x!\n", event);
		break;
	}
}

void
my_server_do(void)
{
  if(OBEX_HandleInput(app->handle, 0) < 0) {
	printf("Error while doing OBEX_HandleInput()\n");
	app->gt->serverdone=TRUE;
  }
}

gboolean
my_init(gpointer data)
{
  int err;

  app->gt->serverdone=FALSE;
  if((err=BtOBEX_ServerRegister(app->handle,
								BDADDR_ANY, BT_CHANNEL)) < 0) {
	printf("Server register error! (Bluetooth) (%d)\n", err);
	// TODO: do some GUI error here, and quit
  } else {
	gnomebt_spinner_reset(app->spin);
	g_timeout_add(100, obex_poll, NULL);
    g_message("Polling for requests");
  }

  return 0;
}

gboolean
obex_poll(gpointer data)
{
  // active, poll once
  if (!app->gt->serverdone) {
	my_server_do();
  } else {
	// we're done, so add a call to re-init
	g_idle_add(my_init, NULL);
	return 0;
  }
  return 1; // call us again next idle
}

static char *
get_save_dir ()
{
    char *dir = NULL;
    GConfClient *client = gconf_client_get_default();

    if (client) {
        dir = gconf_client_get_string (client,
                "/system/bluetooth/obex-savedir", NULL);
        g_object_unref (client);
    }
    if (dir == NULL) {
        dir = (char *) g_get_home_dir();
    }
    return dir;
}

gboolean
main2 (gpointer data)
{
  struct context global_context = {0,};
  uint32_t sdphandle;

  register_sdp(&sdphandle);

  app->gt=&global_context;
  app->gt->serverdone=FALSE;
  app->gt->app=(void*)app;
  app->gt->save_dir = get_save_dir();

  if(! (app->handle = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_event, 0)))      {
	perror("OBEX_Init failed");
	return 0;
  }

  OBEX_SetUserData(app->handle, app->gt);

  gtk_idle_add(my_init, NULL);

  gtk_main();

  g_free((gchar*)app->gt->save_dir);
  deregister_sdp(sdphandle);

  return 0;
}

/*
static gboolean
prefs_activated(GtkMenuItem *item, gpointer data)
{
	return TRUE;
}
*/

static gboolean
quit_activated(GtkMenuItem *item, gpointer data)
{
  gtk_main_quit();
  return TRUE;
}

static gboolean
about_activated(GtkMenuItem *item, gpointer data)
{
  static gpointer about = NULL;
  GdkPixbuf *pixbuf = NULL;
  const gchar *authors[] = { "Edd Dumbill <edd@usefulinc.com>", NULL };
  const gchar *documenters[] = { NULL };
  const gchar *translator_credits = _("translator_credits");

  if (about != NULL) {
	gdk_window_raise (GTK_WIDGET(about)->window);
	gdk_window_show (GTK_WIDGET(about)->window);
	return TRUE;
  }

  pixbuf = gnomebt_icon();

  about = (gpointer)gnome_about_new(_("Bluetooth File Sharing"), VERSION,
						  "Copyright \xc2\xa9 2003 Edd Dumbill",
						  _("Receive files from Bluetooth devices"),
						  (const char **)authors,
						  (const char **)documenters,
						  strcmp (translator_credits,
								  "translator_credits") != 0 ?
						  translator_credits : NULL,
						  pixbuf);

  if (pixbuf != NULL)
	gdk_pixbuf_unref (pixbuf);

  g_signal_connect (G_OBJECT (about), "destroy",
					G_CALLBACK (gtk_widget_destroyed), &about);

  g_object_add_weak_pointer (G_OBJECT (about), &about);

  gtk_widget_show(GTK_WIDGET(about));

  return TRUE;
}

static gboolean
tray_icon_press (GtkWidget *widget, GdkEventButton *event, MyApp *myapp)
{
  if (event->button == 3)
	{
	  gtk_menu_popup (GTK_MENU (app->menu), NULL, NULL, NULL,
					  NULL, event->button, event->time);
	  return TRUE;
	}
  return FALSE;
}

static gboolean
tray_icon_release (GtkWidget *widget, GdkEventButton *event, MyApp *myapp)
{
  if (event->button == 3) {
	gtk_menu_popdown (GTK_MENU (app->menu));
	return FALSE;
  }

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *item;

  GnomebtSpinner *spin;
  EggTrayIcon *tray_icon;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-obex-server", VERSION,
					  LIBGNOMEUI_MODULE,
					  argc, argv,
					  NULL);

  app=g_new0 (MyApp, 1);

  tray_icon = egg_tray_icon_new (_("Bluetooth File Sharing"));
  spin = gnomebt_spinner_new();
  app->spin = spin;
  app->btctl = gnomebt_controller_new ();

  app->tooltip = gtk_tooltips_new();
  gtk_tooltips_set_tip (app->tooltip,
						GTK_WIDGET(app->spin),
						_("Ready for Bluetooth file transfers"),
						NULL);

  app->menu = GTK_MENU (gtk_menu_new());

  /*
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
											 NULL);
  g_signal_connect (G_OBJECT(item), "activate",
					G_CALLBACK (prefs_activated), (gpointer) app);

  gtk_widget_show (item);
  gtk_menu_shell_append(GTK_MENU_SHELL(app->menu), item);
  */

  item = gtk_image_menu_item_new_from_stock (GNOME_STOCK_ABOUT,
											 NULL);
  g_signal_connect (G_OBJECT(item), "activate",
					G_CALLBACK (about_activated), (gpointer) app);

  gtk_widget_show (item);
  gtk_menu_shell_append(GTK_MENU_SHELL(app->menu), item);

  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT,
											 NULL);
  g_signal_connect (G_OBJECT(item), "activate",
					G_CALLBACK (quit_activated), (gpointer) app);

  gtk_widget_show (item);
  gtk_menu_shell_append(GTK_MENU_SHELL(app->menu), item);


  g_signal_connect (GTK_OBJECT (spin), "button_press_event",
					G_CALLBACK (tray_icon_press),
					(gpointer) app);

  g_signal_connect (GTK_OBJECT (spin), "button_release_event",
					G_CALLBACK (tray_icon_release),
					(gpointer) app);

  gtk_container_add (GTK_CONTAINER (tray_icon), GTK_WIDGET(spin));
  gtk_widget_show_all (GTK_WIDGET (tray_icon));

  main2 (NULL);

  g_object_unref (app->spin);
  g_object_unref (app->btctl);

  g_free(app);

  return 0;
}

/**
 * TODO:
 * fix bug: after "Link broken!" condition, it doesn't recover well.
 * this bug exists in the parent app i copied from, so *shrug*
 *
 * ensure it only gets invoked once
 * display sensible error dialogs
 * prefs via gconf
 * restart if tray disappears/reappears
 * think about fancy received-file actions
 *   - should we send vcards to evo, etc?
*/
