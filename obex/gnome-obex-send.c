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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome.h>

#include <bluetooth/sdp.h>
#include <btobex.h>
#include <btobex-client.h>

#include "chooser.h"
#include "gnomebt-controller.h"

typedef struct _appinfo {
	GladeXML	*xml;
	poptContext	context;
	BtctlObexClient	*obex;
	GnomeProgram	*prog;
	gchar	*data;
	gchar	*error;
} MyApp;

#define GLADE_FILE DATA_DIR "/gnome-obex-send.glade"

static void mainloop (MyApp *app);
static uint8_t get_obex_channel (const gchar *bdaddr);
static gboolean send_one (MyApp *app);

static gchar *bdaddrstr = NULL;

static const struct poptOption options[] = {
	{"dest", 'd', POPT_ARG_STRING, &bdaddrstr,
	 0, _("Bluetooth address of destination device"), "BDADDR"},
	{NULL, '\0', 0, NULL, 0}
};

static uint8_t
get_obex_channel (const gchar *bdaddr)
{
	GnomebtController *btmanager;
	GSList *list, *item;
	uint8_t ret=0;

	btmanager = gnomebt_controller_new ();

	if (! btmanager) {
		g_warning ("gnomebt_controller_new() failed");
		return -1;
	}

	list = gnomebt_controller_channels_for_service (btmanager,
			(const gchar *) bdaddr,
			OBEX_OBJPUSH_SVCLASS_ID);

	for (item=list; item != NULL; item = g_slist_next(item)) {
		ret = (uint8_t)((guint) item->data);
	}

	g_object_unref (btmanager);

	return ret;
}

static void
update_gui (MyApp *app)
{
	GladeXML *xml=app->xml;
	GtkWidget *bar;
	gdouble frac = btctl_obex_client_progress (app->obex);
	bar = glade_xml_get_widget (xml, "progressbar1");
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR (bar), frac);
}

static void
report_error (const gchar *msg) {
	GtkWidget *dialog;

	dialog = gnome_error_dialog (msg);
	gtk_signal_connect_object (GTK_OBJECT (dialog),
			"close", GTK_SIGNAL_FUNC (gtk_main_quit),
			(gpointer) dialog);

	gtk_main ();
}

static void
cancel_clicked (GtkWidget *button, gpointer userdata)
{
	gtk_main_quit ();
}

static void
progress_callback (BtctlObexClient *bo, gchar * bdaddr, gpointer data) 
{
	update_gui ((MyApp *)data);
}

static void
complete_callback (BtctlObexClient *bo, gchar * bdaddr, gpointer data) 
{
	MyApp *app = (MyApp *)data;

	if (app->data) {
		g_free (app->data);
		app->data = NULL;
	}

	g_idle_add ((GSourceFunc) send_one, data);
}

static gboolean
send_one (MyApp *app)
{
	const gchar *fname;
	gchar *bname;
	gsize len;
	GError *err = NULL;
	GtkWidget *label;

	if ((fname = poptGetArg (app->context))) {
		/* there's a file to send */
		app->data = NULL;
		if (g_file_get_contents (fname, &(app->data), &len, &err)) {
			bname = g_path_get_basename (fname);
			label = glade_xml_get_widget (app->xml, "filename_label");
			gtk_label_set_text (GTK_LABEL (label), bname);
			btctl_obex_client_push_data (app->obex,
					bname, app->data, len);
			/* TODO: check error return here from command submit */
			g_free (bname);
		} else {
			app->error = g_strdup_printf (_("Unable to read file '%s'."), fname);
			/* TODO: implement error handling */
			g_error_free (err);
			/* error, so we quit */
			gtk_main_quit ();
		}
	} else {
		/* nothing left to send, time to quit */
		gtk_main_quit ();
	}
	
	return FALSE;
}

static void
connected_callback (BtctlObexClient *bo, gchar * bdaddr, gpointer data) 
{
	/* OBEX connect has succeeded, so start sending */
	g_idle_add ((GSourceFunc) send_one, data);
}

static void
error_callback (BtctlObexClient *bo, gchar * bdaddr, guint reason,
		gpointer data)
{
	MyApp *app = (MyApp *) data;

	switch (reason) {
		case BTCTL_OBEX_ERR_LINK:
		case BTCTL_OBEX_ERR_PARSE:
			app->error = g_strdup (_("An error occurred while sending data."));
			break;
		case BTCTL_OBEX_ERR_ABORT:
			app->error = g_strdup (_("The remote device canceled the transfer."));
			break;
		default:
			app->error = g_strdup (_("An unknown error occurred."));
	}
	/* Error condition, so quit. */
	gtk_main_quit ();
}

static void
mainloop (MyApp *app)
{
	GtkWidget *dialog, *label, *button;

	g_signal_connect (G_OBJECT (app->obex), "progress",
		G_CALLBACK (progress_callback), (gpointer) app);
	g_signal_connect (G_OBJECT (app->obex), "complete",
		G_CALLBACK (complete_callback), (gpointer) app);
	g_signal_connect (G_OBJECT (app->obex), "connected",
		G_CALLBACK (connected_callback), (gpointer) app);
	g_signal_connect (G_OBJECT (app->obex), "error",
		G_CALLBACK (error_callback), (gpointer) app);

	dialog = glade_xml_get_widget (app->xml, "send_dialog");

	gtk_widget_show_all (dialog);

	label = glade_xml_get_widget (app->xml, "filename_label");
	button = glade_xml_get_widget (app->xml, "cancelbutton");

	g_signal_connect (G_OBJECT (button), "clicked",
						G_CALLBACK (cancel_clicked),
						(gpointer) app);

	gtk_label_set_text (GTK_LABEL (label), _("Connecting..."));

	gtk_main ();

	gtk_widget_hide (dialog);
}

int
main (int argc, char *argv[])
{
	GValue context_as_value = { 0 };
	const gchar *fname = NULL;
	uint8_t channel;
	MyApp *app;
	int ret = 0;

	app = g_new0 (MyApp, 1);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app->prog = gnome_program_init ("gnome-obex-send", VERSION,
				LIBGNOMEUI_MODULE,
				argc, argv,
				GNOME_PARAM_POPT_TABLE, options,
				GNOME_PARAM_NONE);

	g_object_get_property (G_OBJECT (app->prog),
				GNOME_PARAM_POPT_CONTEXT,
				g_value_init (&context_as_value, G_TYPE_POINTER));

	app->context = g_value_get_pointer (&context_as_value);

	/* find GLADE ui
		TODO use the nifty gnome file find functions */

	if (g_file_exists(GLADE_FILE)) {
		fname=GLADE_FILE;
	} else if (g_file_exists("../ui/gnome-obex-send.glade")) {
		fname="../ui/gnome-obex-send.glade";
	}

	if (fname) {
		app->xml = glade_xml_new (fname, NULL, NULL);
		glade_xml_signal_autoconnect (app->xml);
	} else {
		report_error (_("Can't find user interface file, check your gnome-bluetooth installation."));
		g_free (app);
		return 1;
	}

	if (! bdaddrstr) {
		bdaddrstr = choose_bdaddr ();
		if (! bdaddrstr) {
			return 0;
		}
	}

	channel = get_obex_channel (bdaddrstr);

	if (channel < 1) {
		report_error (_("Remote device doesn't support receiving objects."));
		g_free (app);
		return 1;
	}

	app->obex = BTCTL_OBEX_CLIENT (
			btctl_obex_client_new_and_connect (bdaddrstr, channel));

	if (! app->obex) {
		report_error (_("Can't initialize program."));
		ret = 1;
	} else {
		if (btctl_obex_client_is_initialised (app->obex)) {
			mainloop (app);
		} else {
			report_error (_("Unable to connect to remote device."));
			ret = 1;
		}
		g_object_unref (app->obex);
	}

	if (app->data)
		g_free (app->data);

	if (app->error) {
		report_error (app->error);
		g_free (app->error);
	}

	g_free (app);
	return ret;
}
