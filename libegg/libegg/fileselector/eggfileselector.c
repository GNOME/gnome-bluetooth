/* eggfileselector.h
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

#include "eggfileselector.h"

#include <string.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkvbox.h>

#include <eggentry.h>
#include "eggfilesystem-vfs.h"

#ifndef EGG_COMPILATION
#ifndef _
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x
#endif
#else
#define _(x) x
#define N_(x) x
#endif

enum {
  FILENAME_COL,
  PIX_COL,
  FS_COL,
  NUM_COLS
};

struct _EggFileSelectorPrivate
{
  GtkWidget *cancel_button;
  GtkWidget *ok_button;

  GtkWidget *refresh_button;
  GtkWidget *go_up_button;
  GtkWidget *new_dir_button;
  GtkWidget *fs_option_menu;
  GtkWidget *file_entry;
  GtkWidget *filter_menu;

  EggFileSystem *file_system;
  EggFileSystemItem *current_folder;
  EggFileFilter *current_filter;

  GtkWidget *icon_list;
  GtkTreeSelection *selection;
};

static void egg_file_selector_class_init (EggFileSelectorClass *klass);
static void egg_file_selector_init       (EggFileSelector      *selector);

static void egg_file_selector_populate   (EggFileSelector      *selector,
					  EggFileSystemItem    *folder);
static void egg_file_selector_item_activated (GtkTreeView *treeview,
					     GtkTreePath *arg1,
					     GtkTreeViewColumn *arg2,
					     gpointer user_data);
static void egg_file_selector_refresh (GtkWidget *button,
				       EggFileSelector *selector);
static void egg_file_selector_go_up (GtkWidget *button,
				     EggFileSelector *selector);
static void egg_file_selector_new_dir (GtkWidget *button,
				       EggFileSelector *selector);
static void egg_file_selector_filter_menu_activated (GtkMenuItem     *menu_item,
						     EggFileSelector *selector);

GType
egg_file_selector_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{	  
	  sizeof (EggFileSelectorClass),
	  NULL,		/* base_init */
	  NULL,		/* base_finalize */
	  (GClassInitFunc) egg_file_selector_class_init,
	  NULL,		/* class_finalize */
	  NULL,		/* class_data */
	  sizeof (EggFileSelector),
	  0,            /* n_preallocs */
	  (GInstanceInitFunc) egg_file_selector_init
	};
      
      object_type = g_type_register_static (GTK_TYPE_DIALOG, "EggFileSelector", &object_info, 0);

    }

  return object_type;
  
}

static void
egg_file_selector_class_init (EggFileSelectorClass *klass)
{
}

static void
egg_file_selector_error (const char *msg)
{
  GtkWidget *error_dialog;

  error_dialog = gtk_message_dialog_new (NULL,
		  			 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 "%s", msg);
  gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
		  		   GTK_RESPONSE_OK);
  gtk_widget_show (error_dialog);
  gtk_dialog_run (GTK_DIALOG (error_dialog));
  gtk_widget_destroy (error_dialog);
}

static void
init_columns (GtkTreeView *treeview, EggFileSelector *selector)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* the icons before it */
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
		  "pixbuf", PIX_COL, NULL);
  gtk_tree_view_append_column (treeview, column);

  /* Labels */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
		  "text", FILENAME_COL, NULL);

  g_signal_connect (G_OBJECT (treeview), "row-activated",
		  G_CALLBACK (egg_file_selector_item_activated), selector);
}

static gboolean
egg_file_selector_selection_changed (GtkTreeSelection *selection,
				     GtkTreeModel *model,
				     GtkTreePath *path,
				     gboolean path_currently_selected,
				     gpointer user_data)
{
  EggFileSelector *selector = (EggFileSelector *) user_data;
  EggFileSystemItem *item;
  GtkTreeIter iter;
  char *name;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
		  FILENAME_COL, &name,
		  FS_COL, &item, -1);
  if (egg_file_system_item_get_item_type (item) == EGG_FILE_SYSTEM_ITEM_FILE)
    egg_entry_set_text (EGG_ENTRY (selector->priv->file_entry), name);
  else if (path_currently_selected == FALSE)
    egg_entry_set_text (EGG_ENTRY (selector->priv->file_entry), "");

  g_free (name);

  return TRUE;
}

static GtkWidget *
new_icon_list (EggFileSelector *selector)
{
  GtkTreeModel *model;
  GtkWidget *treeview;

  /* the model */
  model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
			  G_TYPE_STRING,
			  GDK_TYPE_PIXBUF,
			  EGG_TYPE_FILE_SYSTEM_ITEM));

  /* the treeview */
  treeview = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  g_object_unref (G_OBJECT (model));

  init_columns (GTK_TREE_VIEW (treeview), selector);

  /* Set the selection */
  selector->priv->selection = gtk_tree_view_get_selection
	  (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selector->priv->selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function (selector->priv->selection,
		  			  egg_file_selector_selection_changed,
					  selector, NULL);

  gtk_widget_show (treeview);

  return treeview;
}

static GtkWidget *
egg_file_selector_menu_item_with_stock (const char *label_str, const char *id)
{
  GtkWidget *item, *hbox, *label, *image;

  item = gtk_menu_item_new ();
  hbox = gtk_hbox_new (FALSE, 4);
  label = gtk_label_new (label_str);
  if (id != NULL)
  {
    image = gtk_image_new_from_stock (id, GTK_ICON_SIZE_MENU);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  }

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (item), hbox);

  gtk_widget_show_all (item);

  return item;
}

static void
egg_file_selector_init (EggFileSelector *selector)
{
  GtkWidget *vbox, *scrolled_window, *table, *label, *hbox;
  GtkWidget *menu, *menu_item;
  GtkWidget *fs_menu, *fs_menu_item;
  EggFileSystemItem *item;

  selector->priv = g_new0 (_EggFileSelectorPrivate, 1);
  selector->priv->file_system = g_object_new (EGG_TYPE_FILE_SYSTEM_VFS, NULL);

  selector->priv->cancel_button = gtk_dialog_add_button (GTK_DIALOG (selector),
						   GTK_STOCK_CANCEL,
						   GTK_RESPONSE_CANCEL);
  selector->priv->ok_button = gtk_dialog_add_button (GTK_DIALOG (selector),
					       GTK_STOCK_OK,
					       GTK_RESPONSE_OK);
  gtk_widget_grab_default (selector->priv->ok_button);
  
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (selector)->vbox),
		      vbox, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 4);

  /* Refresh button */
  selector->priv->refresh_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (selector->priv->refresh_button),
		     gtk_image_new_from_stock (GTK_STOCK_REFRESH,
			     		       GTK_ICON_SIZE_SMALL_TOOLBAR));
  gtk_box_pack_start (GTK_BOX (hbox), selector->priv->refresh_button,
		      FALSE, FALSE, 0);
  g_signal_connect (selector->priv->refresh_button, "clicked",
		    G_CALLBACK (egg_file_selector_refresh), selector);

  /* Go up button */
  selector->priv->go_up_button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
  g_signal_connect (selector->priv->go_up_button, "clicked",
		    G_CALLBACK (egg_file_selector_go_up), selector);
  gtk_box_pack_start (GTK_BOX (hbox), selector->priv->go_up_button,
		      FALSE, FALSE, 0);

  /* New directory button */
  selector->priv->new_dir_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
  g_signal_connect (selector->priv->new_dir_button, "clicked",
		  G_CALLBACK (egg_file_selector_new_dir), selector);
  gtk_box_pack_start (GTK_BOX (hbox), selector->priv->new_dir_button,
		      FALSE, FALSE, 0);

  /* Quick directory access drop-down */
  selector->priv->fs_option_menu = gtk_option_menu_new ();
  fs_menu = gtk_menu_new ();
  fs_menu_item = egg_file_selector_menu_item_with_stock (_("Home Directory"),
		  					 GTK_STOCK_HOME);
  gtk_menu_shell_append (GTK_MENU_SHELL (fs_menu), fs_menu_item);
  fs_menu_item = egg_file_selector_menu_item_with_stock (_("Root Directory"),
		  					 GTK_STOCK_GOTO_TOP);
  gtk_menu_shell_append (GTK_MENU_SHELL (fs_menu), fs_menu_item);
  //FIXME this is ugly
  fs_menu_item = egg_file_selector_menu_item_with_stock (_("Network"),
		  					 NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (fs_menu), fs_menu_item);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (selector->priv->fs_option_menu),
		  	    fs_menu);
  gtk_box_pack_start (GTK_BOX (hbox), selector->priv->fs_option_menu,
		      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* Icon List */
  selector->priv->icon_list = new_icon_list (selector);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
  				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (scrolled_window),
		     selector->priv->icon_list);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 8);

  /* Filename entry */
  label = gtk_label_new (_("Filename:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);
  selector->priv->file_entry = egg_entry_new ();

  /* disabled for now -- Kris
   *egg_entry_completion_set_model (EGG_ENTRY (selector->priv->file_entry),
   *  gtk_tree_view_get_model (GTK_TREE_VIEW (selector->priv->icon_list)), 0);
   *egg_entry_completion_enable (EGG_ENTRY (selector->priv->file_entry));
   */
  gtk_table_attach (GTK_TABLE (table), selector->priv->file_entry,
		    1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 2);

  /* File types drop-down */
  label = gtk_label_new (_("File Types:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  selector->priv->filter_menu = gtk_option_menu_new ();
  gtk_table_attach (GTK_TABLE (table), selector->priv->filter_menu,
		    1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

  menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_label (_("All Files"));
  g_signal_connect (menu_item, "activate",
		    G_CALLBACK (egg_file_selector_filter_menu_activated),
		    selector);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (selector->priv->filter_menu),
		  	    menu);
  gtk_widget_show_all (vbox);

  item = egg_file_system_get_folder (selector->priv->file_system,
		  		     EGG_FILE_SYSTEM_FOLDER_HOME, NULL);

  egg_file_selector_populate (selector, item);
  egg_file_system_item_unref (item);
}

static void
egg_file_selector_item_activated (GtkTreeView *treeview, GtkTreePath *arg1,
		GtkTreeViewColumn *arg2, gpointer user_data)
{
  EggFileSelector *selector = (EggFileSelector *)user_data;
  EggFileSystemItem *item;
  GtkListStore *store;
  GtkTreeIter iter;

  store = GTK_LIST_STORE (gtk_tree_view_get_model
      (GTK_TREE_VIEW (selector->priv->icon_list)));

  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, arg1);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, FS_COL, &item, -1);

  egg_file_system_item_ref (item);

  if (egg_file_system_item_get_item_type (item) == EGG_FILE_SYSTEM_ITEM_FOLDER)
    egg_file_selector_populate (selector, item);
  else
    gtk_dialog_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);

  egg_file_system_item_unref (item);
}

static void
egg_file_selector_refresh (GtkWidget *button,
			   EggFileSelector *selector)
{
  egg_file_selector_populate (selector, selector->priv->current_folder);
}

static void
egg_file_selector_go_up (GtkWidget       *button,
			 EggFileSelector *selector)
{
  EggFileSystemItem *item;

  item = egg_file_system_item_get_parent (selector->priv->current_folder, NULL);

  egg_file_selector_populate (selector, item);
  
  egg_file_system_item_unref (item);
}

static void
egg_file_selector_new_dir (GtkWidget *button,
			   EggFileSelector *selector)
{
  EggFileSystemItem *item;
  const char *name;
  GtkWidget *dialog, *entry, *label, *hbox;

  dialog = gtk_dialog_new_with_buttons (_("Create Directory"),
		  	       GTK_WINDOW (selector),
			       GTK_DIALOG_MODAL
			       | GTK_DIALOG_DESTROY_WITH_PARENT,
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_REJECT,
			       GTK_STOCK_OK,
			       GTK_RESPONSE_ACCEPT,
			       NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  label = gtk_label_new_with_mnemonic (_("_Directory Name:"));
  entry = egg_entry_new ();
  hbox = gtk_hbox_new (FALSE, 2);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
  gtk_container_add (GTK_CONTAINER (hbox), label);
  gtk_container_add (GTK_CONTAINER (hbox), entry);
  gtk_widget_show_all (hbox);

dialog_run:
  gtk_widget_grab_focus (entry);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
  {
    gtk_widget_destroy (dialog);
    return;
  }

  name = egg_entry_get_text (EGG_ENTRY (entry));
  if (name == NULL || strcmp (name, "") == 0)
  {
    egg_file_selector_error (_("You need to enter a directory name for the "
			     "new directory"));
    goto dialog_run;
    return;
  }

  //FIXME err
  item = egg_file_system_create_new_folder (selector->priv->file_system,
		  		     name, NULL);

  gtk_widget_destroy (dialog);
}

static void
egg_file_selector_populate (EggFileSelector *selector, EggFileSystemItem *item)
{
  GList *list, *p;
  EggFileFilter *base_filter;
  GtkListStore *store;
  GtkTreeIter iter, *last_dir;
  GdkPixbuf *pixbuf;

  base_filter = egg_file_selector_get_base_filter ();

  store = GTK_LIST_STORE (gtk_tree_view_get_model
		  	 (GTK_TREE_VIEW (selector->priv->icon_list)));
  gtk_list_store_clear (store);

  list = egg_file_system_item_get_children (item, NULL);
  p = list;
  last_dir = NULL;

  while (p)
    {
      EggFileSystemItem *current = p->data;
      gchar *name;

      pixbuf = egg_file_system_item_get_icon (current,
		      EGG_FILE_SYSTEM_ICON_REGULAR);
      name = egg_file_system_item_get_name (current);

      if (!egg_file_filter_should_show (base_filter, current)
	  || (egg_file_system_item_get_item_type (current) != EGG_FILE_SYSTEM_ITEM_FOLDER
	  && selector->priv->current_filter
	  && !egg_file_filter_should_show (selector->priv->current_filter, current)))
	{
	  p = p->next;
	  continue;
	}

      if (egg_file_system_item_get_item_type (current)
	  == EGG_FILE_SYSTEM_ITEM_FOLDER)
      {
        gtk_list_store_insert_after (store, &iter, last_dir);
	if (last_dir != NULL)
          gtk_tree_iter_free (last_dir);
	last_dir = gtk_tree_iter_copy (&iter);
      } else {
        gtk_list_store_append (store, &iter);
      }

      gtk_list_store_set (store, &iter,
          PIX_COL, pixbuf,
	  FILENAME_COL, name,
	  FS_COL, current,
	  -1);
      g_free (name);

      p = p->next;
    }

  if (selector->priv->current_folder != item)
  {
    if (selector->priv->current_folder)
      egg_file_system_item_unref (selector->priv->current_folder);
    selector->priv->current_folder = egg_file_system_item_ref (item);
  }

  if (last_dir != NULL)
    gtk_tree_iter_free (last_dir);

  /* Update the drop-down menu */
  //FIXME

  gtk_widget_set_sensitive (selector->priv->go_up_button,
			    !egg_file_system_item_is_root (item));
}

static gboolean
egg_file_selector_base_filter_func (EggFileFilter     *filter,
				    EggFileSystemItem *item,
				    gpointer           data)
{
  return TRUE;
}

/* Public API */

/* This should go into eggfilesystem */
EggFileFilter *
egg_file_selector_get_base_filter (void)
{
  static EggFileFilter *base_filter = NULL;

  if (!base_filter)
    base_filter = egg_file_filter_new ("Base Filter",
		    		       egg_file_selector_base_filter_func,
				       NULL, NULL);

  return base_filter;
}

GtkWidget *
egg_file_selector_new (const gchar *title)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (g_object_new (EGG_TYPE_FILE_SELECTOR, NULL));

  if (title != NULL)                         
    gtk_window_set_title (GTK_WINDOW (widget), title);

  gtk_window_resize (GTK_WINDOW (widget), 350, 400);

  return widget;
}

void
egg_file_selector_set_select_multiple (EggFileSelector *selector,
				       gboolean select_multiple)
{
  g_return_if_fail (EGG_IS_FILE_SELECTOR (selector));

  if (select_multiple == FALSE)
    gtk_tree_selection_set_mode (selector->priv->selection,
		    		 GTK_SELECTION_SINGLE);
  else
    gtk_tree_selection_set_mode (selector->priv->selection,
		    		 GTK_SELECTION_MULTIPLE);
}

gboolean
egg_file_selector_get_select_multiple (EggFileSelector *selector)
{
  g_return_val_if_fail (EGG_IS_FILE_SELECTOR (selector), FALSE);

  return (gtk_tree_selection_get_mode (selector->priv->selection)
		  == GTK_SELECTION_SINGLE);
}

void
egg_file_selector_set_confirm_stock (EggFileSelector *selector,
				     const gchar     *stock_id)
{
  g_return_if_fail (EGG_IS_FILE_SELECTOR (selector));
  g_return_if_fail (stock_id != NULL);
  
  gtk_button_set_use_stock (GTK_BUTTON (selector->priv->ok_button), TRUE);
  gtk_button_set_label (GTK_BUTTON (selector->priv->ok_button), stock_id);
}

static void
egg_file_selector_filter_menu_activated (GtkMenuItem     *menu_item,
					 EggFileSelector *selector)
{
  EggFileFilter *filter;

  filter = g_object_get_data (G_OBJECT (menu_item), "egg-filter");

  selector->priv->current_filter = filter;

  egg_file_selector_populate (selector, selector->priv->current_folder);

}
   
void
egg_file_selector_add_choosable_filter (EggFileSelector *selector,
					EggFileFilter   *filter)
{
  GtkWidget *menu_item, *menu;
  
  g_return_if_fail (EGG_IS_FILE_SELECTOR (selector));
  g_return_if_fail (EGG_IS_FILE_FILTER (filter));

  menu_item = gtk_menu_item_new_with_label (filter->description);
  g_signal_connect (menu_item, "activate",
		    G_CALLBACK (egg_file_selector_filter_menu_activated),
		    selector);
  gtk_widget_show (menu_item);
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU
		  		  (selector->priv->filter_menu));
  g_object_set_data (G_OBJECT (menu_item), "egg-filter", filter);
  g_object_ref (filter);
  
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menu_item,
			 g_list_length (GTK_MENU_SHELL (menu)->children) - 1);
}

void
egg_file_selector_set_filename (EggFileSelector *selector,
				const gchar *path)
{
  EggFileSystemItem *item;
  char *dir, *file;

  g_return_if_fail (EGG_IS_FILE_SELECTOR (selector));
  g_return_if_fail (path != NULL);

  item = egg_file_system_get_item_by_name (selector->priv->file_system,
		  			   path, NULL);
  if (item == NULL
      || egg_file_system_item_get_item_type (item) == EGG_FILE_SYSTEM_ITEM_FILE)
  {
    if (item != NULL)
      egg_file_system_item_unref (item);

    dir = g_path_get_dirname (path);
    item = egg_file_system_get_item_by_name (selector->priv->file_system,
		    			     dir, NULL);
    g_free (dir);

    if (item == NULL)
      return;

    file = g_path_get_basename (path);
  } else {
    file = g_strdup ("");
  }

  egg_file_selector_populate (selector, item);
  egg_entry_set_text (EGG_ENTRY (selector->priv->file_entry), file);

  egg_file_system_item_unref (item);
  g_free (file);
}

gchar *
egg_file_selector_get_filename (EggFileSelector *selector)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  EggFileSystemItem *item;
  const char *filename;

  g_return_val_if_fail (egg_file_selector_get_select_multiple (selector),
		        NULL);

  filename = egg_entry_get_text (EGG_ENTRY (selector->priv->file_entry));
  if (filename != NULL && strcmp (filename, "") != 0)
  {
    char *dir, *path;

    dir = egg_file_system_item_get_current_path (selector->priv->current_folder, NULL);
    path = g_build_filename (G_DIR_SEPARATOR_S, dir, filename, NULL);
    g_free (dir);
    return path;
  }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector->priv->icon_list));
  if (!gtk_tree_selection_get_selected (selector->priv->selection, NULL, &iter))
    return NULL;

  gtk_tree_model_get (model, &iter, FS_COL, &item, -1);
  g_return_val_if_fail (item != NULL, NULL);
  return egg_file_system_item_get_current_path (item, NULL);
}

GList *
egg_file_selector_get_filename_list (EggFileSelector *selector)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  EggFileSystemItem *item;
  GList *select_list, *l, *retval;

  retval = NULL;

  if (egg_file_selector_get_select_multiple (selector) == FALSE)
    return NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector->priv->icon_list));

  select_list = gtk_tree_selection_get_selected_rows
	  (selector->priv->selection, NULL);

  if (select_list == NULL)
    return NULL;

  for (l = select_list; l != NULL; l = l->next)
  {
    GtkTreePath *treepath = l->data;

    if (gtk_tree_model_get_iter (model, &iter, treepath) == FALSE)
      continue;

    gtk_tree_model_get (model, &iter, FS_COL, &item, -1);
    g_return_val_if_fail (item != NULL, NULL);

    retval = g_list_prepend (retval, (gpointer)
		    egg_file_system_item_get_current_path (item, NULL));
  }

  g_list_foreach (select_list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (select_list);

  retval = g_list_reverse (retval);

  return retval;
}

