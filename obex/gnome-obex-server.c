/*
 * gnome-obex-server
 * Copyright (c) 2003-4 Edd Dumbill <edd@usefulinc.com>.
 * Copyright (C) 2006 Arjan Timmerman
 * Copyright (C) 2006 Christian Persch
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <utime.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>
#include <gnome.h>

#include <btobex.h>

#include "gnomebt-controller.h"
#include "gnomebt-permissiondialog.h"
#include "gnomebt-fileactiondialog.h"
#include "util.h"

#define N_FRAMES	6
#define REST_FRAME	4

typedef struct _appinfo {
	GnomebtController	*btctl;
	BtctlObex		*obex;
	GtkStatusIcon		*status_icon;
	GdkPixbuf               *frames[N_FRAMES];
	gint                     frame;
	guint                    spincount;
	gint			 status_icon_size;
	GtkMenu			*menu;
	gboolean 		 decision;
} MyApp;

static gboolean allowed_to_put (MyApp *app, const gchar *bdaddr);
static int get_display_notifications ();

static MyApp *app;

/* FIXME: we should install icon animations in theme (like ephy-spinner icons) */

static void
load_icons (MyApp *app)
{
	guint i;

	for (i = 0; i < N_FRAMES; ++i) {
		char *filename;
		GError *error = NULL;

		filename = g_strdup_printf (ICON_DIR G_DIR_SEPARATOR_S "frame%d.png", i + 1);
		app->frames[i] = gdk_pixbuf_new_from_file_at_size (filename,
								   app->status_icon_size,
								   app->status_icon_size,
								   &error);
		if (error != NULL) {
			g_warning ("Failed to load frame %d: %s\n", i + 1, error->message);
			g_error_free (error);
		}
		g_free (filename);
	}
}

static void
unload_icons (MyApp *app)
{
	guint i;

	for (i = 0; i < N_FRAMES; ++i) {
		if (app->frames[i]) {
			g_object_unref (app->frames[i]);
			app->frames[i] = NULL;
		}
	}
}

static gboolean
status_icon_size_changed (GtkStatusIcon *status_icon,
			  gint size,
			  MyApp *app)
{
	if (app->status_icon_size == size)
		return TRUE;

	/* FIXME remove this once gtk bug #340107 is fixed */
	if (size == 200)
		return TRUE;

	unload_icons (app);

	app->status_icon_size = size;
	load_icons (app);

	gtk_status_icon_set_from_pixbuf (app->status_icon,
					 app->frames[app->frame]);

	return TRUE;
}

static void
step_animation (MyApp *app)
{
	app->frame++;
	if (app->frame >= N_FRAMES)
		app->frame = 0;

	gtk_status_icon_set_from_pixbuf (app->status_icon,
					 app->frames[app->frame]);
}

static void
stop_animation (MyApp *app)
{
	app->frame = REST_FRAME;
	gtk_status_icon_set_from_pixbuf (app->status_icon,
					 app->frames[app->frame]);
}

static gboolean
allowed_to_put (MyApp *app, const gchar *bdaddr)
{
	GnomebtPermissionDialog *dlg;
	gboolean result;
	gint perm;
	
	perm = gnomebt_controller_get_device_permission (app->btctl, bdaddr);
	if (perm == GNOMEBT_PERM_ALWAYS)
		return TRUE;
	else if (perm == GNOMEBT_PERM_NEVER)
		return FALSE;

	dlg = gnomebt_permissiondialog_new (app->btctl,
			bdaddr, _("Accept a file from '%s'?"),
			_("The remote device is attempting to send you a file via Bluetooth. Do you wish to allow the connection?"),
			_("Always accept files from this device."));
	result = gtk_dialog_run (GTK_DIALOG (dlg));

	if (result == GTK_RESPONSE_OK &&
			gnomebt_permissiondialog_remember (dlg)) {
		gnomebt_controller_set_device_permission (app->btctl, bdaddr,
				GNOMEBT_PERM_ALWAYS);
	}
	
	gtk_widget_destroy (GTK_WIDGET (dlg));

	
	return (result == GTK_RESPONSE_OK);
}

static char *
get_save_dir (void)
{
	char *dir = NULL;
	GConfClient *client = gconf_client_get_default();

	if (client) {
		dir = gconf_client_get_string (client,
				"/apps/gnome-obex-server/savedir", NULL);
		g_object_unref (client);

		/* Starts with ~/ ? */
		if (dir != NULL && dir[0] == '~' && dir[1] == '/') {
			dir = g_build_filename (g_get_home_dir (),
					dir + 2, NULL);
		}
	}

	/* Try ~/Desktop/Downloads */
	if (dir == NULL || strlen (dir) == 0 || g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE) {
		const char *translated_folder;
		char *converted;

		g_free (dir);
		/* The name of the default downloads folder,
		 * Needs to be the same as Epiphany's */
		translated_folder = _("Downloads");

		converted = g_filename_from_utf8 (translated_folder, -1, NULL,
				NULL, NULL);
		dir = g_build_filename (g_get_home_dir (), "Desktop",
				converted, NULL);
		g_free (converted);
	}

	/* Try ~/Desktop */
	if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE) {
		g_free (dir);
		dir = g_build_filename (g_get_home_dir (), "Desktop", NULL);
	}

	/* Try ~/ */
	if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE) {
		g_free (dir);
		dir = g_strdup (g_get_home_dir ());
	}
	return dir;
}

static gboolean
get_display_notifications (void)
{
	gboolean yesno = TRUE;
	GConfClient *client = gconf_client_get_default();

	if (client) {
		yesno = gconf_client_get_bool (client,
				  "/apps/gnome-obex-server/receive_notification", NULL);
		g_object_unref (client);
	}
	return yesno;
}

static void
progress_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{
	//g_message ("Progress from %s", bdaddr);
	app->spincount++;
	if (app->spincount > 3) {
		step_animation (app);
		app->spincount = 0;
	}
}

static void
complete_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{
	//g_message ("Operation complete from %s", bdaddr);

	stop_animation (app);
}

static void
connect_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{
	g_message ("Incoming connection from %s", bdaddr);
	/* Connection from a stranger: time to ask their name */

	btctl_controller_request_name (BTCTL_CONTROLLER (app->btctl),
			bdaddr);
}

static void
error_callback (BtctlObex *bo, gchar * bdaddr, guint reason, MyApp *app)
{
	/* g_message ("Error %d from %s",reason,  bdaddr); */
	switch (reason) {
		case BTCTL_OBEX_ERR_LINK:
			g_message ("Link error");
			break;
		case BTCTL_OBEX_ERR_PARSE:
			g_message ("Malformed incoming data");
			break;
		case BTCTL_OBEX_ERR_ABORT:
			g_message ("Client aborted transfer");
			break;
	}

	stop_animation (app);
}

static void
request_put_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{

	g_message ("Device %s is about to send an object.", bdaddr);
	gtk_status_icon_set_tooltip (app->status_icon,
				     _("Incoming Bluetooth file transfer"));
	/* Here we'd decide whether or not to accept a PUT
	 * from this device.  For testing purposes, we will.
	 */
	app->decision = allowed_to_put (app, bdaddr);
	if (app->decision)
		btctl_obex_set_response (bo, TRUE);
}

static void
file_action_response (GtkDialog *dialog, gint arg, gpointer user_data)
{
	switch (arg) {
		case GNOMEBT_FILEACTION_OPEN:
			gnomebt_fileactiondialog_open (GNOMEBT_FILEACTIONDIALOG (dialog));
			break;
		case GNOMEBT_FILEACTION_DELETE:
			gnomebt_fileactiondialog_delete (GNOMEBT_FILEACTIONDIALOG (dialog));
			break;
		default:
			/* will be SAVE, so we do nothing */
			break;
	}
}

static void
put_callback (BtctlObex *bo, gchar * bdaddr, gchar *fname,
		BtctlObexData *data, guint timestamp, MyApp *app)
{
	GError *error = NULL;
	char *targetname = NULL, *dir;
	struct utimbuf utim;
	GnomebtFileActionDialog *dlg;

	if (! app->decision) {
		btctl_obex_set_response (bo, FALSE);
		return;
	}

	g_message ("File arrived from %s", bdaddr);
	g_message ("Filename '%s' Length %d", fname, data->body_len);
	gtk_status_icon_set_tooltip (app->status_icon,
				     _("Ready for Bluetooth file transfers"));

	dir = get_save_dir ();
	targetname = get_safe_unique_filename (fname, dir);
	g_free (dir);

	g_message ("Saving to '%s'", targetname);
	if (g_file_set_contents (targetname, data->body, data->body_len, &error) != FALSE) {
		if(timestamp) {
			utim.actime = utim.modtime = (time_t) timestamp;
			utime(targetname, &utim);
		}
		
		btctl_obex_set_response (bo, TRUE);

		/* if notify, then display this dialog */

		if (get_display_notifications ()) {
			dlg = gnomebt_fileactiondialog_new (
					gnomebt_controller_get_device_preferred_name 
						(app->btctl, bdaddr),
					targetname);
			g_signal_connect (G_OBJECT (dlg), "response",
					G_CALLBACK (file_action_response), dlg);
			g_signal_connect_swapped (G_OBJECT (dlg),
					"response", G_CALLBACK (gtk_widget_destroy), dlg);
		}
	} else {
		g_warning ("Couldn't save file: %s", error->message);
		g_error_free (error);
		btctl_obex_set_response (bo, FALSE);
	}
	g_free (targetname);
}

static gboolean
quit_activated (GtkMenuItem *item, gpointer data)
{
	gtk_main_quit();
	return TRUE;
}

static gboolean
about_activated(GtkMenuItem *item, gpointer data)
{
	const gchar *authors[] = { "Edd Dumbill <edd@usefulinc.com>", NULL };
	const gchar *documenters[] = { NULL };
	GdkPixbuf *pixbuf = NULL;

	pixbuf = gnomebt_icon();

	gtk_show_about_dialog (NULL,
				"name", _("Bluetooth File Sharing"),
				"version", VERSION,
				"copyright", "Copyright \xc2\xa9 2003-4 Edd Dumbill",
				"comments", _("Receive files from Bluetooth devices"),
				"authors", authors,
				"documenters", documenters,
				/* add your name and email address in here if you've translated
				 * this program
				 */
				"translator-credits", _("translator-credits"),
				"logo", pixbuf,
				/* FIXME: website? */
				NULL);

	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);

	return TRUE;
}

static gboolean
status_icon_press (GtkStatusIcon *status_icon,
		   guint button,
                   guint32 activate_time,
		   MyApp *app)
{
	gtk_menu_popup (GTK_MENU (app->menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			button, activate_time);
	if (button == 0)
		gtk_menu_shell_select_first (GTK_MENU_SHELL (app->menu), FALSE);

	return TRUE;
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;
	GtkWidget *item;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-obex-server", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_APP_DATADIR, DATA_DIR,
				      NULL);

	app=g_new0 (MyApp, 1);

	app->status_icon = gtk_status_icon_new ();
	app->frame = REST_FRAME;

	g_signal_connect (app->status_icon, "size-changed",
			  G_CALLBACK (status_icon_size_changed), app);
	g_signal_connect (app->status_icon, "popup-menu",
			  G_CALLBACK (status_icon_press), app);

	app->obex = btctl_obex_new ();

	if (! app->obex || ! btctl_obex_is_initialised (app->obex)) {
		g_warning ("Couldn't initialise OBEX listener");
		return 1;
	}

	g_signal_connect (G_OBJECT(app->obex), "request-put",
				G_CALLBACK(request_put_callback), (gpointer) app);
	g_signal_connect (G_OBJECT(app->obex), "put",
				G_CALLBACK(put_callback), (gpointer) app);
	g_signal_connect (G_OBJECT(app->obex), "progress",
				G_CALLBACK(progress_callback), (gpointer) app);
	g_signal_connect (G_OBJECT(app->obex), "complete",
				G_CALLBACK(complete_callback), (gpointer) app);
	g_signal_connect (G_OBJECT(app->obex), "error",
				G_CALLBACK(error_callback), (gpointer) app);
	g_signal_connect (G_OBJECT(app->obex), "connect",
				G_CALLBACK(connect_callback), (gpointer) app);
	


	app->btctl = gnomebt_controller_new ();

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
	gtk_menu_shell_append (GTK_MENU_SHELL(app->menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT,
											 NULL);
	g_signal_connect (G_OBJECT(item), "activate",
					G_CALLBACK (quit_activated), (gpointer) app);

	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL(app->menu), item);

	gtk_status_icon_set_tooltip (app->status_icon,
				     _("Ready for Bluetooth file transfers"));
	gtk_status_icon_set_visible (app->status_icon, TRUE);
		
	gtk_main ();

	unload_icons (app);
	g_object_unref (app->status_icon);
	g_object_unref (app->btctl);
	g_object_unref (app->obex);
	g_free (app);

	g_object_unref (program);

	return 0;
}

/**
 * TODO:
 *
 * ensure it only gets invoked once
 * display sensible error dialogs
 * think about fancy received-file actions
 *	 - should we send vcards to evo, etc?
*/
