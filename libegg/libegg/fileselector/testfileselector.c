#include <gtk/gtk.h>
#include "eggfileselector.h"

/* Single selection, open */
//#define MULTIPLE_SELECTION FALSE
//#define SAVE FALSE
/* Multiple selection, open */
//#define MULTIPLE_SELECTION TRUE
//#define SAVE FALSE
/* Save selector */
#define MULTIPLE_SELECTION FALSE
#define SAVE TRUE

gboolean
jpeg_filter (EggFileFilter *filter, EggFileSystemItem *item, gpointer data)
{
  static GPatternSpec *pspec = NULL;
  gchar *name;
  gboolean retval;
  
  if (!pspec)
    pspec = g_pattern_spec_new ("*.jpg");

  name = egg_file_system_item_get_name (item);

  retval = g_pattern_match_string (pspec, name);

  g_free (name);
  
  return retval;
}

gboolean
no_filter (EggFileFilter *filter, EggFileSystemItem *item, gpointer data)
{
  return FALSE;
}

static void
show_selection (EggFileSelector *selector)
{
	if (MULTIPLE_SELECTION == FALSE)
	{
		g_print ("Selected path: %s\n", egg_file_selector_get_filename
				(EGG_FILE_SELECTOR (selector)));
	} else {
		GList *list, *i;

		list = egg_file_selector_get_filename_list (selector);
		g_print ("Selected paths:\n");

		for (i = list; i != NULL; i = i->next)
		{
			g_print ("path: %s\n", (char *) i->data);
		}
	}
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *selector;
  EggFileFilter *filter;
  
  gtk_init (&argc, &argv);
  
  selector = egg_file_selector_new ("Test File Selector");
  if (SAVE == FALSE)
    egg_file_selector_set_confirm_stock (EGG_FILE_SELECTOR (selector), GTK_STOCK_OPEN);
  else
    egg_file_selector_set_confirm_stock (EGG_FILE_SELECTOR (selector), GTK_STOCK_SAVE);

  filter = egg_file_filter_new ("JPEG files", jpeg_filter, NULL, NULL);
  egg_file_selector_add_choosable_filter (EGG_FILE_SELECTOR (selector), filter);
  g_object_unref (filter);

  filter = egg_file_filter_new ("No files", no_filter, NULL, NULL);
  egg_file_selector_add_choosable_filter (EGG_FILE_SELECTOR (selector), filter);
  g_object_unref (filter);
  egg_file_selector_set_select_multiple (EGG_FILE_SELECTOR (selector),
		  MULTIPLE_SELECTION);

  if (argv[1] != NULL)
    egg_file_selector_set_filename (EGG_FILE_SELECTOR (selector), argv[1]);

  gtk_widget_show (selector);

  gtk_dialog_run (GTK_DIALOG (selector));
  gtk_widget_hide (selector);
  show_selection (EGG_FILE_SELECTOR (selector));

  return 0;
}
