#include "eggstatusicon.h"
#include <gtk/gtk.h>
#include <libgnomeui/gnome-icon-theme.h>

typedef enum
{
  TEST_STATUS_FILE,
  TEST_STATUS_DIRECTORY
} TestStatus;

static GnomeIconTheme *icon_theme = NULL;
static TestStatus status = TEST_STATUS_FILE;

static void
update_icon (EggStatusIcon *status_icon)
{
  gchar *icon_name;
  gchar *path;
  gchar *tooltip;

  if (status == TEST_STATUS_FILE)
    {
      icon_name = "gnome-fs-regular";
      tooltip = "Regular File";
    }
  else
    {
      icon_name = "gnome-fs-directory";
      tooltip = "Directory";
    }

  egg_status_icon_set_tooltip (status_icon, tooltip, NULL);

  path = gnome_icon_theme_lookup_icon (icon_theme, icon_name,
				       egg_status_icon_get_size (status_icon),
				       NULL, NULL);
  if (path)
    {
      GdkPixbuf *pixbuf;
      GError *error = NULL;

      pixbuf = gdk_pixbuf_new_from_file (path, &error);
      if (error)
        {
	  g_warning ("Failed to load '%s': %s", path, error->message);
	  g_error_free (error);
        }
      else
	{
	  egg_status_icon_set_from_pixbuf (status_icon, pixbuf);
	  g_object_unref (pixbuf);
	}

      g_free (path);
    }
  else
    {
      g_warning ("Failed to lookup icon '%s'", icon_name);
    }
}

static gboolean
timeout_handler (gpointer data)
{
  EggStatusIcon *icon = EGG_STATUS_ICON (data);

  if (status == TEST_STATUS_FILE)
    status = TEST_STATUS_DIRECTORY;
  else
    status = TEST_STATUS_FILE;

  update_icon (icon);

  return TRUE;
}

static void
blink_toggle_toggled (GtkToggleButton *toggle,
		      EggStatusIcon   *icon)
{
  egg_status_icon_set_is_blinking (icon, gtk_toggle_button_get_active (toggle));
}

static void
icon_activated (EggStatusIcon *icon)
{
  GtkWidget *dialog;
  GtkWidget *toggle;

  dialog = g_object_get_data (G_OBJECT (icon), "test-status-icon-dialog");
  if (dialog == NULL)
    {
      dialog = gtk_message_dialog_new (NULL, 0,
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_CLOSE,
				       "You wanna test the status icon ?");

      g_object_set_data_full (G_OBJECT (icon), "test-status-icon-dialog",
			      dialog, (GDestroyNotify) gtk_widget_destroy);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_hide), NULL);
      g_signal_connect (dialog, "delete_event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      toggle = gtk_toggle_button_new_with_mnemonic ("_Blink the icon");
      gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), egg_status_icon_get_is_blinking (icon));
      g_signal_connect (toggle, "toggled", G_CALLBACK (blink_toggle_toggled), icon);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

int
main (int argc, char **argv)
{
  EggStatusIcon *icon;

  gtk_init (&argc, &argv);

  icon_theme = gnome_icon_theme_new ();

  icon = egg_status_icon_new ();
  update_icon (icon);

  egg_status_icon_set_is_blinking (EGG_STATUS_ICON (icon), TRUE);

  g_signal_connect (icon, "activate",
		    G_CALLBACK (icon_activated), NULL);

  g_timeout_add (2000, timeout_handler, icon);

  gtk_main ();

  g_object_unref (icon_theme);

  return 0;
}
