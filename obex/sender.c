/*
  Copyright (c) 2003 Edd Dumbill.
  Portions copyright (c) 2000, Pontus Fuchs, All Rights Reserved.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <libbonobo.h>
#include "GNOME_Bluetooth_Manager.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include <openobex/obex.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome.h>

#include "obex_test.h"
#include "obex_test_client.h"

#include "obex_io.h"
#include "obex_test_server.h"

#include "chooser.h"

#define ERR_ERROR 1
#define ERR_TIMEOUT 2

#define GLADE_FILE DATA_DIR "/gnome-obex-send.glade"

typedef struct _appinfo {
  struct context *gt;
  obex_t *handle;
  GladeXML *xml;
  poptContext context;
} MyApp;


uint8_t get_obex_channel(char *bdaddr);
void obex_event(obex_t *handle, obex_object_t *object, int mode,
				int event, int obex_cmd, int obex_rsp);
gboolean my_syncwait(gpointer userdata);
void mainloop(MyApp *app);

static GnomeProgram *prog;

//
// Called by the obex-layer when some event occurs.
//
void
obex_event(obex_t *handle, obex_object_t *object,
		   int mode, int event, int obex_cmd, int obex_rsp)
{
	switch (event)	{
	case OBEX_EV_PROGRESS:
	  /* printf("Made some progress...\n"); */
		break;

	case OBEX_EV_ABORT:
		printf("Request aborted!\n");
		break;

	case OBEX_EV_REQDONE:
		if(mode == OBEX_CLIENT) {
			client_done(handle, object, obex_cmd, obex_rsp);
		}
		else	{
			server_done(handle, object, obex_cmd, obex_rsp);
		}
		break;
	case OBEX_EV_REQHINT:
		/* Accept any command. Not rellay good, but this is a test-program :) */
		OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
		break;

	case OBEX_EV_REQ:
		server_request(handle, object, event, obex_cmd);
		break;

	case OBEX_EV_LINKERR:
		OBEX_TransportDisconnect(handle);
		printf("Link broken!\n");
		break;

	case OBEX_EV_STREAMEMPTY:
		fillstream(handle, object);
		break;

	default:
		printf("Unknown event %02x!\n", event);
		break;
	}
}

/* this function is not idempotent */
uint8_t
get_obex_channel(char *bdaddr)
{
  GNOME_Bluetooth_Manager btmanager;
  CORBA_Environment ev;
  GNOME_Bluetooth_ChannelList *list;
  int ret=0;

  bonobo_activate();

  btmanager=bonobo_get_object("OAFIID:GNOME_Bluetooth_Manager",
								"GNOME/Bluetooth/Manager", NULL);

  if (btmanager==CORBA_OBJECT_NIL) {
    g_warning("Couldn't get instance of BT Manager");
    bonobo_debug_shutdown();
	return -1;
  }

  CORBA_exception_init (&ev);
  list=NULL;

  GNOME_Bluetooth_Manager_channelsForService
	(btmanager,
	 &list,
	 (const CORBA_char*)bdaddr,
	 GNOME_Bluetooth_OBEX_OBJPUSH_SVCLASS_ID,
	 &ev);

  if (BONOBO_EX(&ev)) {
    char *err=bonobo_exception_get_text (&ev);
    g_warning (_("An exception occured '%s'"), err);
    g_free (err);
    CORBA_exception_free (&ev);
  } else {
	guint j;

	for(j=0; j<list->_length; j++) {
	  ret=list->_buffer[j];
	}
	CORBA_free(list);
  }
  bonobo_object_release_unref (btmanager, NULL);
  bonobo_debug_shutdown ();
  return ret;
}

static gchar *bdaddrstr=NULL;

static const struct poptOption options[] = {
  {"dest", 'd', POPT_ARG_STRING, &bdaddrstr,
   0, N_("Bluetooth address of destination device"), N_("BDADDR")},
  {NULL, '\0', 0, NULL, 0}
};

static void
update_gui(void *user_data)
{
  struct context *gt=(struct context *)user_data;
  MyApp *app=gt->app;
  GladeXML *xml=app->xml;
  GtkWidget *bar;
  gdouble frac=0.0;

  if (gt->to_send>0)
	frac=(gdouble)gt->sent/(gdouble)gt->to_send;

  bar=glade_xml_get_widget(xml, "progressbar1");

  /* g_message("Update %d of %d bytes", gt->sent,
	 gt->to_send); */

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), frac);
}

gboolean
my_syncwait(gpointer userdata)
{
  MyApp *app;
  obex_t *handle;
  struct context *gt;
  int ret;

  app=(MyApp*)userdata;
  handle=app->handle;

  /* g_message("in my_syncwait"); */

  gt = OBEX_GetUserData(handle);

  if (!gt->clientdone) {
	ret = OBEX_HandleInput(handle, 10);
	if(ret < 0) {
	  printf("Error while doing OBEX_HandleInput()\n");
	  gt->error=ERR_ERROR;
	} else if(ret == 0) {
	  /* If running cable. We get no link-errors, so cancel on timeout */
	  printf("Timeout waiting for data. Aborting\n");
	  OBEX_CancelRequest(handle, FALSE);
	  gt->error=ERR_TIMEOUT;
	} else {

	  /* everything went OK, so update the GUI */

	  if (gt->update) {
		(*gt->update)(gt);
	  }
	  return TRUE; /* call us again next idle */
	}

  }

  gt->clientdone=FALSE; /* disconnecting needs this */

  g_message("exiting main loop");

  gtk_main_quit();
  return FALSE; /* don't call us again */
}

void
my_push_client(obex_t *handle, const char *fullname)
{
	obex_object_t *object;

	unsigned int uname_size;
	char *bfname;
	char *uname;

	obex_headerdata_t hd;
	uint8_t *buf;
	int file_size;
	struct context *gt;

	gt = OBEX_GetUserData(handle);
	gt->error = 0;

	bfname = g_path_get_basename(fullname);


	buf = easy_readfile(fullname, &file_size);
	if(buf == NULL)
		return;

    gt->fileDesc = open(fullname, O_RDONLY, 0);

	/* TODO: find more sensible way of finding the file size
	   than calling easy readfile!! */

    if (gt->fileDesc < 0) {
        free(buf);
        free(bfname);
        return;
    }

	/* g_message("Going to send %s(%s), %d bytes\n",fullname,bfname, file_size); */

	gt->to_send=file_size;
	gt->sent=0;

	/* Build object */
	object = OBEX_ObjectNew(handle, OBEX_CMD_PUT);

	uname_size = (strlen(bfname)+1)<<1;
	uname = g_malloc(uname_size);
	OBEX_CharToUnicode(uname, bfname, uname_size);

	hd.bs = uname;
	OBEX_ObjectAddHeader(handle, object,
						 OBEX_HDR_NAME, hd, uname_size, 0);

	hd.bq4 = file_size;
	OBEX_ObjectAddHeader(handle, object,
						 OBEX_HDR_LENGTH, hd, sizeof(uint32_t), 0);

	hd.bs = NULL;
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY, hd, 0,
						 OBEX_FL_STREAM_START);

	free(buf);
	g_free(uname);
	g_free(bfname);

	OBEX_Request(handle, object);

	gt->clientdone=FALSE;
	gtk_idle_add(my_syncwait, (gpointer)gt->app);
	/* syncwait(handle); */
}

static void
report_error(const gchar *msg) {
	GtkWidget *dialog;
	dialog=gnome_error_dialog(msg);
	gtk_signal_connect_object (GTK_OBJECT (dialog),
			"close", GTK_SIGNAL_FUNC (gtk_main_quit),
			(gpointer) dialog);
	gtk_main();
}

static void
cancel_clicked(GtkWidget *button, gpointer userdata)
{
  MyApp *app=(MyApp*)userdata;

  app->gt->usercancel=TRUE;
  g_message("user hit cancel");
  /* TODO: actually figure out how to cleanly abort
	 a connection mid-transfer. */
}

void
mainloop(MyApp *app)
{
  GtkWidget *dialog,*label,*button;
  const gchar *fname = NULL;

  app->gt->usercancel=FALSE;

  dialog = glade_xml_get_widget(app->xml, "send_dialog");

  gtk_widget_show_all(dialog);

  label = glade_xml_get_widget(app->xml, "filename_label");
  button = glade_xml_get_widget(app->xml, "cancelbutton");
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
					 GTK_SIGNAL_FUNC (cancel_clicked),
					 (gpointer)app);

  while ( (fname = poptGetArg(app->context))) {
	gtk_label_set_text(GTK_LABEL(label), fname);
	my_push_client(app->handle, fname);
	gtk_main();

	if (app->gt->error) {
	  gtk_widget_hide_all(dialog);
	  switch(app->gt->error) {
	  case ERR_TIMEOUT:
		report_error(_("A timeout occurred while sending data. Check the receiving device."));
		break;
	  default:
		report_error(_("An error occurred while sending data. Check the receiving device."));
		break;
	  }
	  return;
	}
	if (app->gt->usercancel) {
	  /* canceled by user intervention
	   TODO: is there anything more graceful we need here? */
	  return;
	}
  }
}

int
main (int argc, char *argv[])
{
	obex_t *handle;
	bdaddr_t bdaddr;
	uint8_t channel=0;
	gchar *dest=NULL;
	GValue context_as_value = { 0 };
	const gchar *fname=NULL;
	struct context global_context = {0,};
	MyApp *app;

	app=g_new0(MyApp, 1);
	global_context.app=(void *)app;
	global_context.update=update_gui;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	prog = gnome_program_init ("gnome-obex-send", VERSION,
							   LIBGNOMEUI_MODULE,
							   argc, argv,
							   GNOME_PARAM_POPT_TABLE, options,
							   GNOME_PARAM_NONE);

	g_object_get_property (G_OBJECT (prog),
						   GNOME_PARAM_POPT_CONTEXT,
						   g_value_init (&context_as_value, G_TYPE_POINTER));

	app->context = g_value_get_pointer (&context_as_value);

	if (!bonobo_init(&argc, argv)) {
	  g_warning("Couldn't initialize bonobo");
	  return 1;
	}

	/* find GLADE ui
	  TODO use the nifty gnome file find functions */

	if (g_file_exists(GLADE_FILE)) {
	  fname=GLADE_FILE;
    } else if (g_file_exists("../ui/gnome-obex-send.glade")) {
	  fname="../ui/gnome-obex-send.glade";
    }

    if (fname) {
	  app->xml = glade_xml_new(fname, NULL, NULL);
	  glade_xml_signal_autoconnect(app->xml);
    } else {
	  report_error(_("Can't find user interface file, check your gnome-bluetooth installation."));
	  return 1;
	}

	if (!bdaddrstr) {
	    bdaddrstr = choose_bdaddr ();
        if (!bdaddrstr) {
            /* cancelled, so quit */
            return 0;
        }
	}


	if(!(handle = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_event, 0))) {
	  g_error("OBEX_Init failed");
	  return 1;
	}

	app->handle=handle;
	app->gt=&global_context;

	OBEX_SetUserData(handle, &global_context);

	dest=g_ascii_strup(bdaddrstr, strlen(bdaddrstr));

	str2ba(dest, &bdaddr);
	g_message("Sending to %s", dest);

	/* arguments: first arg is dest btaddr
	   each arg thereafter is a file to send */

	if (bacmp(&bdaddr, BDADDR_ANY) == 0) {
	  report_error(_("The Bluetooth device specified is unknown."));
	  return 1;
	}

	/* now time to find out which channel OBEX PUSH is available
	   at on the destination host */

	channel = get_obex_channel(dest);

	if (dest)
	  g_free(dest);

	if (channel <= 0) {
	  report_error(_("The device you are trying to send to doesn't support receiving objects."));
	  return 1;
	}

	g_message ("Attempting to connect on channel %d", channel);

	if(BtOBEX_TransportConnect(handle, BDADDR_ANY, &bdaddr, channel) <0) {
	  report_error(_("Unable to make a Bluetooth connection."));
	  return 1;
	}

	g_message ("Connected OK");

	connect_client(handle);

	mainloop(app);

	disconnect_client(handle);

	g_free(app);

	return 0;
}
