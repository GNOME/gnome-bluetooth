#include <gtk/gtk.h>
#include "eggtrayicon.h"

static void
first_button_pressed (GtkWidget *button, EggTrayIcon *icon)
{
  guint id;
  
  g_print ("woohoo, first button is pressed!\n");

  id = egg_tray_icon_send_message (icon, 0, "This is a message from the first tray icon", -1);
  g_print ("Sent message, id is: %d\n", id);
}

static void
second_button_pressed (GtkWidget *button, EggTrayIcon *icon)
{
  g_print ("woohoo, second button is pressed!\n");

  egg_tray_icon_cancel_message (icon, 12345);
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *button;
  EggTrayIcon *tray_icon;

  gtk_init (&argc, &argv);

  tray_icon = egg_tray_icon_new ("Our cool tray icon");
  button = gtk_button_new_with_label ("This is a cool\ntray icon");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (first_button_pressed), tray_icon);
  gtk_container_add (GTK_CONTAINER (tray_icon), button);
  gtk_widget_show_all (GTK_WIDGET (tray_icon));

  tray_icon = egg_tray_icon_new ("Our other cool tray icon");
  button = gtk_button_new_with_label ("This is a another\ncool tray icon");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (second_button_pressed), tray_icon);

  gtk_container_add (GTK_CONTAINER (tray_icon), button);
  gtk_widget_show_all (GTK_WIDGET (tray_icon));

  gtk_main ();
  
  return 0;
}
