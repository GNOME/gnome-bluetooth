#include <gtk/gtk.h>
#include "eggtoolbar.h"
#include "eggtoolitem.h"
#include "eggtoolbutton.h"
#include "eggtoggletoolbutton.h"
#include "eggseparatortoolitem.h"
#include "eggradiotoolbutton.h"

static void
reload_clicked (GtkWidget *widget)
{
  static GdkAtom atom_rcfiles = GDK_NONE;

  GdkEventClient sev;
  int i;
  
  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);

  for(i = 0; i < 5; i++)
    sev.data.l[i] = 0;
  sev.data_format = 32;
  sev.message_type = atom_rcfiles;
  gdk_event_send_clientmessage_toall ((GdkEvent *) &sev);
}

static void
change_orientation (GtkWidget *button, GtkWidget *toolbar)
{
  GtkWidget *table;
  GtkOrientation orientation;

  table = gtk_widget_get_parent (toolbar);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    orientation = GTK_ORIENTATION_VERTICAL;
  else
    orientation = GTK_ORIENTATION_HORIZONTAL;

  g_object_ref (toolbar);
  gtk_container_remove (GTK_CONTAINER (table), toolbar);
  egg_toolbar_set_orientation (EGG_TOOLBAR (toolbar), orientation);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_table_attach (GTK_TABLE (table), toolbar,
			0,2, 0,1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
    }
  else
    {
      gtk_table_attach (GTK_TABLE (table), toolbar,
			0,1, 0,4, GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
    }
  g_object_unref (toolbar);
}

static void
change_show_arrow (GtkWidget *button, GtkWidget *toolbar)
{
  egg_toolbar_set_show_arrow (EGG_TOOLBAR (toolbar),
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
set_toolbar_style_toggled (GtkCheckButton *button, EggToolbar *toolbar)
{
  GtkWidget *option_menu;
  int style;
  
  option_menu = g_object_get_data (G_OBJECT (button), "option-menu");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      style = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));

      egg_toolbar_set_style (toolbar, style);
      gtk_widget_set_sensitive (option_menu, TRUE);
    }
  else
    {
      egg_toolbar_unset_style (toolbar);
      gtk_widget_set_sensitive (option_menu, FALSE);
    }
}

static void
change_toolbar_style (GtkWidget *option_menu, GtkWidget *toolbar)
{
  GtkToolbarStyle style;

  style = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
  egg_toolbar_set_style (EGG_TOOLBAR (toolbar), style);
}

static void
set_visible_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		 GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  EggToolItem *tool_item;
  gboolean visible;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_get (G_OBJECT (tool_item), "visible", &visible, NULL);
  g_object_set (G_OBJECT (cell), "active", visible, NULL);
  g_object_unref (tool_item);
}

static void
visibile_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		 GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  EggToolItem *tool_item;
  gboolean visible;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  g_object_get (G_OBJECT (tool_item), "visible", &visible, NULL);
  g_object_set (G_OBJECT (tool_item), "visible", !visible, NULL);
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
set_expandable_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		    GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  EggToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (G_OBJECT (cell), "active", tool_item->expandable, NULL);
  g_object_unref (tool_item);
}

static void
expandable_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		   GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  EggToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  egg_tool_item_set_expandable (tool_item, !tool_item->expandable);
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
set_pack_end_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		  GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  EggToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (G_OBJECT (cell), "active", tool_item->pack_end, NULL);
  g_object_unref (tool_item);
}

static void
pack_end_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		 GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  EggToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  egg_tool_item_set_pack_end (tool_item, !tool_item->pack_end);
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
set_homogeneous_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  EggToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (G_OBJECT (cell), "active", tool_item->homogeneous, NULL);
  g_object_unref (tool_item);
}

static void
homogeneous_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		    GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  EggToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  egg_tool_item_set_homogeneous (tool_item, !tool_item->homogeneous);
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static GtkListStore *
create_items_list (GtkWidget **tree_view_p)
{
  GtkWidget *tree_view;
  GtkListStore *list_store;
  GtkCellRenderer *cell;
  
  list_store = gtk_list_store_new (2, EGG_TYPE_TOOL_ITEM, G_TYPE_STRING);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Tool Item",
					       gtk_cell_renderer_text_new (),
					       "text", 1, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (visibile_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Visible",
					      cell,
					      set_visible_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (expandable_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Expandable",
					      cell,
					      set_expandable_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (pack_end_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Pack End",
					      cell,
					      set_pack_end_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (homogeneous_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Homogeneous",
					      cell,
					      set_homogeneous_func, NULL,NULL);

  g_object_unref (list_store);

  *tree_view_p = tree_view;

  return list_store;
}

static void
add_item_to_list (GtkListStore *store, EggToolItem *item, const gchar *text)
{
  GtkTreeIter iter;

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, item,
		      1, text,
		      -1);
  
}

static void
bold_toggled (EggToggleToolButton *button)
{
  g_message ("Bold toggled (active=%d)",
	     egg_toggle_tool_button_get_active (button));
}

static void
set_icon_size_toggled (GtkCheckButton *button, EggToolbar *toolbar)
{
  GtkWidget *option_menu;
  int icon_size;
  
  option_menu = g_object_get_data (G_OBJECT (button), "option-menu");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      icon_size = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
      icon_size += GTK_ICON_SIZE_SMALL_TOOLBAR;

      egg_toolbar_set_icon_size (toolbar, icon_size);
      gtk_widget_set_sensitive (option_menu, TRUE);
    }
  else
    {
      egg_toolbar_unset_icon_size (toolbar);
      gtk_widget_set_sensitive (option_menu, FALSE);
    }
}

static void
icon_size_history_changed (GtkOptionMenu *menu, EggToolbar *toolbar)
{
  int icon_size;

  icon_size = gtk_option_menu_get_history (menu);
  icon_size += GTK_ICON_SIZE_SMALL_TOOLBAR;
  
  egg_toolbar_set_icon_size (toolbar, icon_size);
}

static gboolean
toolbar_drag_drop (GtkWidget *widget, GdkDragContext *context,
		   gint x, gint y, guint time, GtkWidget *label)
{
  gchar buf[32];

  g_snprintf(buf, sizeof(buf), "%d",
	     egg_toolbar_get_drop_index (EGG_TOOLBAR (widget), x, y));
  gtk_label_set_label (GTK_LABEL (label), buf);

  return TRUE;
}

static GtkTargetEntry target_table[] = {
  { "application/x-toolbar-item", 0, 0 }
};

gint
main (gint argc, gchar **argv)
{
  GtkWidget *window, *toolbar, *table, *treeview, *scrolled_window;
  GtkWidget *hbox, *checkbox, *option_menu, *menu;
  gint i;
  static const gchar *toolbar_styles[] = { "icons", "text", "both (vertical)",
					   "both (horizontal)" };
  EggToolItem *item;
  GtkListStore *store;
  GtkWidget *image;
  GtkWidget *menuitem;
  GtkWidget *button;
  GtkWidget *label;
  GSList *group;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  table = gtk_table_new (4, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (window), table);

  toolbar = egg_toolbar_new ();
  gtk_table_attach (GTK_TABLE (table), toolbar,
		    0,2, 0,1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_table_attach (GTK_TABLE (table), hbox,
		    1,2, 1,2, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

  checkbox = gtk_check_button_new_with_mnemonic("_Vertical");
  gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);
  g_signal_connect (checkbox, "toggled",
		    G_CALLBACK (change_orientation), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Show Arrow");
  gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);
  g_signal_connect (checkbox, "toggled",
		    G_CALLBACK (change_show_arrow), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Set Toolbar Style:");
  g_signal_connect (checkbox, "toggled", G_CALLBACK (set_toolbar_style_toggled), toolbar);
  gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);
  
  option_menu = gtk_option_menu_new();
  gtk_widget_set_sensitive (option_menu, FALSE);  
  g_object_set_data (G_OBJECT (checkbox), "option-menu", option_menu);
  
  menu = gtk_menu_new();
  for (i = 0; i < G_N_ELEMENTS (toolbar_styles); i++)
    {
      GtkWidget *menuitem;

      menuitem = gtk_menu_item_new_with_label (toolbar_styles[i]);
      gtk_container_add (GTK_CONTAINER (menu), menuitem);
      gtk_widget_show (menuitem);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       EGG_TOOLBAR (toolbar)->style);
  gtk_box_pack_start (GTK_BOX (hbox), option_menu, FALSE, FALSE, 0);
  g_signal_connect (option_menu, "changed",
		    G_CALLBACK (change_toolbar_style), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Set Icon Size:");
  g_signal_connect (checkbox, "toggled", G_CALLBACK (set_icon_size_toggled), toolbar);
  gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);

  option_menu = gtk_option_menu_new();
  g_signal_connect (option_menu, "changed",
		    G_CALLBACK (icon_size_history_changed), toolbar);
  
  g_object_set_data (G_OBJECT (checkbox), "option-menu", option_menu);
  gtk_widget_set_sensitive (option_menu, FALSE);
  menu = gtk_menu_new();
  menuitem = gtk_menu_item_new_with_label ("small toolbar");
  g_object_set_data (G_OBJECT (menuitem), "value-id", GINT_TO_POINTER (GTK_ICON_SIZE_SMALL_TOOLBAR));
  gtk_container_add (GTK_CONTAINER (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("large toolbar");
  g_object_set_data (G_OBJECT (menuitem), "value-id", GINT_TO_POINTER (GTK_ICON_SIZE_LARGE_TOOLBAR));
  gtk_container_add (GTK_CONTAINER (menu), menuitem);
  gtk_widget_show (menuitem);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_box_pack_start (GTK_BOX (hbox), option_menu, FALSE, FALSE, 0);
  
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_table_attach (GTK_TABLE (table), scrolled_window,
		    1,2, 2,3, GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

  store = create_items_list (&treeview);
  gtk_container_add (GTK_CONTAINER (scrolled_window), treeview);
  
  item = egg_tool_button_new_from_stock (GTK_STOCK_NEW);
  add_item_to_list (store, item, "New");
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_tool_button_new_from_stock (GTK_STOCK_OPEN);
  add_item_to_list (store, item, "Open");
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");    
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);
  
  item = egg_tool_button_new_from_stock (GTK_STOCK_REFRESH);
  add_item_to_list (store, item, "Refresh");  
  g_signal_connect (item, "clicked", G_CALLBACK (reload_clicked), NULL);
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
  item = egg_tool_item_new ();
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (item), image);
  add_item_to_list (store, item, "(Custom Item)");    
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  
  item = egg_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
  add_item_to_list (store, item, "Back");    
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");  
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);
  
  item = egg_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  add_item_to_list (store, item, "Forward");  
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_toggle_tool_button_new_from_stock (GTK_STOCK_BOLD);
  g_signal_connect (item, "toggled", G_CALLBACK (bold_toggled), NULL);
  add_item_to_list (store, item, "Bold");  
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");  
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_radio_tool_button_new_from_stock (NULL, GTK_STOCK_JUSTIFY_LEFT);
  group = egg_radio_tool_button_get_group (EGG_RADIO_TOOL_BUTTON (item));
  add_item_to_list (store, item, "Left");
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);
  
  item = egg_radio_tool_button_new_from_stock (group, GTK_STOCK_JUSTIFY_CENTER);
  group = egg_radio_tool_button_get_group (EGG_RADIO_TOOL_BUTTON (item));  
  add_item_to_list (store, item, "Center");
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  item = egg_radio_tool_button_new_from_stock (group, GTK_STOCK_JUSTIFY_RIGHT);
  add_item_to_list (store, item, "Right");
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_table_attach (GTK_TABLE (table), hbox,
		    1,2, 3,4, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

  button = gtk_button_new_with_label ("Drag me to the toolbar");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  label = gtk_label_new ("Drop index:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  gtk_drag_source_set (button, GDK_BUTTON1_MASK,
		       target_table, G_N_ELEMENTS (target_table),
		       GDK_ACTION_MOVE);
  gtk_drag_dest_set (toolbar, GTK_DEST_DEFAULT_DROP,
		     target_table, G_N_ELEMENTS (target_table),
		     GDK_ACTION_MOVE);
  g_signal_connect (toolbar, "drag_drop",
		    G_CALLBACK (toolbar_drag_drop), label);

  gtk_widget_show_all (window);
  
  gtk_main ();
  
  return 0;
}
