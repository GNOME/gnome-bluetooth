#include <gtk/gtk.h>
#include "eggtoolbar.h"
#include "eggtoolitem.h"
#include "eggtoolbutton.h"
#include "eggseparatortoolitem.h"

static gboolean
toolbar_drop (EggToolbar *toolbar, GdkDragContext *context,
	      gint x, gint y, guint time)
{
  GtkWidget *source;
  GtkWidget *item;
  gint old_index, drop_index;

  source = gtk_drag_get_source_widget (context);
  if (!source)
    return FALSE;
  item = gtk_widget_get_ancestor (source, EGG_TYPE_TOOL_ITEM);
  if (!item || !EGG_IS_TOOL_ITEM (item))
    return FALSE;

  old_index = egg_toolbar_get_item_index (toolbar, EGG_TOOL_ITEM (item));
  drop_index = egg_toolbar_get_drop_index (toolbar, x, y);
  if (old_index < drop_index)
    drop_index--;

  g_object_ref (item);
  gtk_container_remove (GTK_CONTAINER (toolbar), item);
  egg_toolbar_insert_tool_item (toolbar, EGG_TOOL_ITEM (item), drop_index);
  g_object_unref (item);

  return TRUE;
}

void
toolitem_drag_begin (GtkWidget *widget, GdkDragContext *context)
{
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_get_from_drawable (NULL, widget->window, NULL,
					 widget->allocation.x,
					 widget->allocation.y,
					 0, 0,
					 widget->allocation.width,
					 widget->allocation.height);

  gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
  g_object_unref (pixbuf);
}

static const gchar *tool_items[] = {
  GTK_STOCK_NEW,
  NULL,
  GTK_STOCK_OPEN,
  GTK_STOCK_SAVE,
  GTK_STOCK_CLOSE,
  NULL,
  GTK_STOCK_GO_BACK,
  GTK_STOCK_GO_UP,
  GTK_STOCK_GO_FORWARD,
  NULL,
  GTK_STOCK_HOME,
};
static GtkTargetEntry target_table[] = {
  { "application/x-toolbar-item", 0, 0 }
};

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *toolbar;
  EggToolItem *item;
  GtkWidget *entry;
  gint i;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, -1);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  toolbar = egg_toolbar_new();
  egg_toolbar_set_show_arrow (EGG_TOOLBAR (toolbar), TRUE);
  gtk_container_add (GTK_CONTAINER (window), toolbar);

  gtk_drag_dest_set (toolbar, GTK_DEST_DEFAULT_DROP,
		     target_table, G_N_ELEMENTS (target_table),
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (toolbar, "drag_drop",
		    G_CALLBACK (toolbar_drop), NULL);

  for (i = 0; i < G_N_ELEMENTS (tool_items); i++)
    {
      if (tool_items[i])
	item = egg_tool_button_new_from_stock (tool_items[i]);
      else
	item = egg_separator_tool_item_new ();

      egg_tool_item_set_use_drag_window (EGG_TOOL_ITEM (item), TRUE);
      egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);
      gtk_drag_source_set (GTK_WIDGET (item),
			   GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
      g_signal_connect (item, "drag_begin",
			G_CALLBACK (toolitem_drag_begin), NULL);
    }

  item = egg_tool_item_new();
  egg_tool_item_set_homogeneous (item, FALSE);
  egg_tool_item_set_expandable (item, TRUE);
  entry = gtk_entry_new();
  gtk_container_add (GTK_CONTAINER (item), entry);

  egg_tool_item_set_use_drag_window (EGG_TOOL_ITEM (item), TRUE);
  egg_toolbar_insert_tool_item (EGG_TOOLBAR (toolbar), item, -1);
  gtk_drag_source_set (GTK_WIDGET (item),
		       GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		       target_table, G_N_ELEMENTS (target_table),
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (item, "drag_begin",
		    G_CALLBACK (toolitem_drag_begin), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
