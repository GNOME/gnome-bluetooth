/*
 * gnome-obex-server
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>

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

#include <btobex.h>

#include "eggtrayicon.h"
#include "gnomebt-controller.h"
#include "gnomebt-spinner.h"
#include "gnomebt-permissiondialog.h"
#include "util.h"

typedef struct _appinfo {
	GnomebtController	*btctl;
	BtctlObex		*obex;
	GnomebtSpinner	*spin;
	GtkTooltips		*tooltip;
	EggTrayIcon	*tray_icon;
	GtkMenu	*menu;
	gboolean 	decision;
} MyApp;

static gboolean allowed_to_put (MyApp *app, const gchar *bdaddr);
static gboolean tray_destroy_cb (GtkObject *obj, MyApp *app);
static const gchar * get_save_dir ();

static MyApp *app;
static gint spincount = 0;

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
			bdaddr, _("Incoming Bluetooth Transfer"),
			_("<span weight='bold' size='larger'>Accept a file from '%s'?</span>\n\nThe remote device is attempting to send you a file via Bluetooth. Do you wish to allow the connection?"),
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

static const gchar *
get_save_dir ()
{
	const gchar *dir = NULL;
	GConfClient *client = gconf_client_get_default();

	if (client) {
		dir = gconf_client_get_string (client,
				"/system/bluetooth/obex-savedir", NULL);
		g_object_unref (client);
	}
	if (dir == NULL) {
		dir = g_get_home_dir();
	}
	return dir;
}

/*
static gboolean
prefs_activated(GtkMenuItem *item, gpointer data)
{
	return TRUE;
}
*/

static void
progress_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{
	/* g_message ("Progress from %s", bdaddr); */
	spincount++;
	if (spincount > 3) {
		gnomebt_spinner_spin (app->spin);
		spincount = 0;
	}
}

static void
complete_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{
	/* g_message ("Operation complete from %s", bdaddr); */
	gnomebt_spinner_reset (app->spin);
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
	gnomebt_spinner_reset (app->spin);
}

static void
request_put_callback (BtctlObex *bo, gchar * bdaddr, MyApp *app)
{

	g_message ("Device %s is about to send an object.", bdaddr);
	/* Here we'd decide whether or not to accept a PUT
	 * from this device.  For testing purposes, we will.
	 */
	app->decision = allowed_to_put (app, bdaddr);
	if (app->decision)
		btctl_obex_set_response (bo, TRUE);
}

static void
put_callback (BtctlObex *bo, gchar * bdaddr, gchar *fname,
		gpointer body, guint len, MyApp *app)
{
	int fd;
	gchar *targetname = NULL;

	if (! app->decision) {
		btctl_obex_set_response (bo, FALSE);
		return;
	}

	g_message ("File arrived from %s", bdaddr);
	g_message ("Filename '%s' Length %d", fname, len);

	targetname = get_safe_unique_filename (fname,
			get_save_dir ());
	g_message ("Saving to '%s'", targetname);
	fd = open (targetname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	g_free (targetname);
	if (fd >= 0) {
		write (fd, body, len);
		close (fd);
		btctl_obex_set_response (bo, TRUE);
	} else {
		g_warning ("Couldn't save file.");
		btctl_obex_set_response (bo, FALSE);
	}
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
tray_icon_press (GtkWidget *widget, GdkEventButton *event, MyApp *app)
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
tray_icon_release (GtkWidget *widget, GdkEventButton *event, MyApp *app)
{
	if (event->button == 3) {
		gtk_menu_popdown (GTK_MENU (app->menu));
		return FALSE;
	}

	return TRUE;
}

static gboolean
tray_destroy_cb (GtkObject *obj, MyApp *app)
{
	/* When try icon is destroyed, recreate it.  This happens
	   when the notification area is removed. */

	app->tray_icon = egg_tray_icon_new (_("Bluetooth File Sharing"));
	app->spin = gnomebt_spinner_new ();
	gnomebt_spinner_reset (app->spin);

	g_signal_connect (G_OBJECT (app->tray_icon), "destroy",
		G_CALLBACK (tray_destroy_cb), (gpointer) app);

	g_signal_connect (GTK_OBJECT (app->spin), "button_press_event",
		G_CALLBACK (tray_icon_press), (gpointer) app);

	g_signal_connect (GTK_OBJECT (app->spin), "button_release_event",
		G_CALLBACK (tray_icon_release), (gpointer) app);

	gtk_container_add (GTK_CONTAINER (app->tray_icon),
			GTK_WIDGET (app->spin));

	gtk_widget_show_all (GTK_WIDGET (app->tray_icon));

	return TRUE;
}

int
main (int argc, char *argv[])
{
	GtkWidget *item;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-obex-server", VERSION,
						LIBGNOMEUI_MODULE,
						argc, argv,
						NULL);

	app=g_new0 (MyApp, 1);

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

	app->tray_icon = egg_tray_icon_new (_("Bluetooth File Server"));
	app->btctl = gnomebt_controller_new ();

	app->tooltip = gtk_tooltips_new();

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

	tray_destroy_cb (NULL, app);

	gtk_tooltips_set_tip (app->tooltip,
						GTK_WIDGET (app->spin),
						_("Ready for Bluetooth file transfers"),
						NULL);

	gtk_main ();

	/* NB: tray icon not tidied here. to do it properly,
	 * detach its destroy handler, then destroy it.  that
	 * will destroy its contained spin widget too. */
	g_object_unref (app->btctl);
	g_object_unref (app->obex);

	g_free (app);

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
