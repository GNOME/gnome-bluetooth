/* testiconlist.c
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include "eggiconlist.h"

static void
fill_icon_list (EggIconList *icon_list)
{
  GdkPixbuf *pixbuf;
  GTimer *timer;
  EggIconListItem *item = NULL;
  int i, j;
  
  pixbuf = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);
  
  timer = g_timer_new ();

  i = 0;
  while (i < 1000)
    {
      char *str;

      j = g_random_int_range (0, 1000);
      
      str = g_strdup_printf ("Icon %d", j);
      item = egg_icon_list_item_new (pixbuf, str);
      egg_icon_list_item_set_data (item, GINT_TO_POINTER (j));
      egg_icon_list_append_item (icon_list, item);

      g_free (str);

      i++;
    }

  g_print ("Prepending %d icons took %fs\n", i, g_timer_elapsed (timer, NULL));
}

static void
foreach_print_func (EggIconList *icon_list, EggIconListItem *item, gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 0x12345678);
  
  g_print ("%s\n", egg_icon_list_item_get_label (item));
}

static void
foreach_print (GtkWidget *button, EggIconList *icon_list)
{
  egg_icon_list_foreach (icon_list, foreach_print_func, GINT_TO_POINTER (0x12345678));
}


static void
foreach_selected_print_func (EggIconList *icon_list, EggIconListItem *item, gpointer data)
{
  g_assert (GPOINTER_TO_INT (data) == 0x12345678);
  
  g_print ("%s\n", egg_icon_list_item_get_label (item));
}

static void
foreach_selected_print (GtkWidget *button, EggIconList *icon_list)
{
  egg_icon_list_selected_foreach (icon_list, foreach_selected_print_func, GINT_TO_POINTER (0x12345678));
}


static void
selection_changed (EggIconList *icon_list)
{
  g_print ("Selection changed!\n");
}

static void
item_added (EggIconList *icon_list, EggIconListItem *item)
{
/*  g_print ("Item added: %s\n", egg_icon_list_item_get_label (item));*/
}


static void
item_removed (EggIconList *icon_list, EggIconListItem *item)
{
  g_print ("Item removed: %s\n", egg_icon_list_item_get_label (item));
}

static void
remove_selected (GtkWidget *button, EggIconList *icon_list)
{
  GList *selected, *p;

  selected = egg_icon_list_get_selected (icon_list);

  for (p = selected; p; p = p->next)
    {
      egg_icon_list_remove_item (icon_list, p->data);
    }
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *paned;
  GtkWidget *window, *icon_list, *scrolled_window;
  GtkWidget *vbox, *bbox;
  GtkWidget *button;
  
  gtk_init (&argc, &argv);
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 700, 400);

  paned = gtk_hpaned_new ();
  gtk_container_add (GTK_CONTAINER (window), paned);

  icon_list = egg_icon_list_new ();
  egg_icon_list_set_selection_mode (EGG_ICON_LIST (icon_list), GTK_SELECTION_MULTIPLE);
  
  g_signal_connect (icon_list, "selection_changed",
		    G_CALLBACK (selection_changed), NULL);
  g_signal_connect (icon_list, "item_added",
		    G_CALLBACK (item_added), NULL);
  g_signal_connect (icon_list, "item_removed",
		    G_CALLBACK (item_removed), NULL);
  
  fill_icon_list (EGG_ICON_LIST (icon_list));
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), icon_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
  				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  bbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Foreach print");
  g_signal_connect (button, "clicked", G_CALLBACK (foreach_print), icon_list);
  gtk_box_pack_start_defaults (GTK_BOX (bbox), button);

  button = gtk_button_new_with_label ("Foreach selected print");
  g_signal_connect (button, "clicked", G_CALLBACK (foreach_selected_print), icon_list);
  gtk_box_pack_start_defaults (GTK_BOX (bbox), button);
  
  button = gtk_button_new_with_label ("Remove selected");
  g_signal_connect (button, "clicked", G_CALLBACK (remove_selected), icon_list);
  gtk_box_pack_start_defaults (GTK_BOX (bbox), button);
  
  gtk_paned_pack1 (GTK_PANED (paned), vbox, TRUE, FALSE);

  icon_list = egg_icon_list_new ();
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), icon_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_pack2 (GTK_PANED (paned), scrolled_window, TRUE, FALSE);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
