#include <stdio.h>
#include <gtk/gtk.h>
#include "eggtraymanager.h"

static void
lost_selection (EggTrayManager *manager)
{
  g_print ("We lost our selection");
}

static void
tray_added (EggTrayManager *manager, GtkWidget *icon, GtkWidget *box)
{
  gtk_widget_show (icon);
  gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);

  g_print ("tray added: \"%s\"\n", egg_tray_manager_get_child_title (manager, icon));
}

static void
tray_removed (EggTrayManager *manager, GtkWidget *icon, GtkWidget *box)
{
  g_print ("tray removed: %p\n", icon);
}

static void
message_sent (EggTrayManager *manager, GtkWidget *icon, const char *text, glong id, glong timeout)
{
  g_print ("got message: \"%s\" (%ld)\n", text, id);
}

static void
message_cancelled (EggTrayManager *manager, GtkWidget *icon, glong id)
{
  g_print ("message cancelled (%ld)\n", id);
    
}

gint
main (gint argc, gchar **argv)
{
  EggTrayManager *manager;
  GtkWidget *window, *hbox;
  
  gtk_init (&argc, &argv);
#if 0
  /* FIXME multihead */
  if (egg_tray_manager_check_running_default_screen ())
    {
      g_warning ("a tray manager is already running.");
      return 0;
    }
#endif
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  manager = egg_tray_manager_new ();
  g_signal_connect (manager, "lost_selection",
		    G_CALLBACK (lost_selection), NULL);
  g_signal_connect (manager, "tray_icon_added",
		    G_CALLBACK (tray_added), hbox);
  g_signal_connect (manager, "tray_icon_removed",
		    G_CALLBACK (tray_removed), hbox);
  g_signal_connect (manager, "message_sent",
		    G_CALLBACK (message_sent), NULL);
  g_signal_connect (manager, "message_cancelled",
		    G_CALLBACK (message_cancelled), NULL);

  if (!egg_tray_manager_manage_screen (manager,
                                       gdk_screen_get_default ()))
    {
      g_warning ("could not set default screen.");
    }

  gtk_widget_show_all (window);
  
  gtk_main ();
  return 0;
}
