#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gtk/gtk.h>
#include "eggtoolbar.h"

/* XPM */
static char *openfile[] = {
/* width height num_colors chars_per_pixel */
"    20    19       66            2",
/* colors */
".. c None",
".# c #000000",
".a c #dfdfdf",
".b c #7f7f7f",
".c c #006f6f",
".d c #00efef",
".e c #009f9f",
".f c #004040",
".g c #00bfbf",
".h c #ff0000",
".i c #ffffff",
".j c #7f0000",
".k c #007070",
".l c #00ffff",
".m c #00a0a0",
".n c #004f4f",
".o c #00cfcf",
".p c #8f8f8f",
".q c #6f6f6f",
".r c #a0a0a0",
".s c #7f7f00",
".t c #007f7f",
".u c #5f5f5f",
".v c #707070",
".w c #00f0f0",
".x c #009090",
".y c #ffff00",
".z c #0000ff",
".A c #00afaf",
".B c #00d0d0",
".C c #00dfdf",
".D c #005f5f",
".E c #00b0b0",
".F c #001010",
".G c #00c0c0",
".H c #000f0f",
".I c #00007f",
".J c #005050",
".K c #002f2f",
".L c #dfcfcf",
".M c #dfd0d0",
".N c #006060",
".O c #00e0e0",
".P c #00ff00",
".Q c #002020",
".R c #dfc0c0",
".S c #008080",
".T c #001f1f",
".U c #003f3f",
".V c #007f00",
".W c #00000f",
".X c #000010",
".Y c #00001f",
".Z c #000020",
".0 c #00002f",
".1 c #000030",
".2 c #00003f",
".3 c #000040",
".4 c #00004f",
".5 c #000050",
".6 c #00005f",
".7 c #000060",
".8 c #00006f",
".9 c #000070",
"#. c #7f7f80",
"## c #9f9f9f",
/* pixels */
"........................................",
"........................................",
"........................................",
".......................#.#.#............",
".....................#.......#...#......",
"...............................#.#......",
".......#.#.#.................#.#.#......",
".....#.y.i.y.#.#.#.#.#.#.#..............",
".....#.i.y.i.y.i.y.i.y.i.#..............",
".....#.y.i.y.i.y.i.y.i.y.#..............",
".....#.i.y.i.y.#.#.#.#.#.#.#.#.#.#.#....",
".....#.y.i.y.#.s.s.s.s.s.s.s.s.s.#......",
".....#.i.y.#.s.s.s.s.s.s.s.s.s.#........",
".....#.y.#.s.s.s.s.s.s.s.s.s.#..........",
".....#.#.s.s.s.s.s.s.s.s.s.#............",
".....#.#.#.#.#.#.#.#.#.#.#..............",
"........................................",
"........................................",
"........................................"
};

gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}

/*
 * EggToolBar
 */

static GtkWidget*
new_pixmap (char      *filename,
	    GdkWindow *window,
	    GdkColor  *background)
{
  GtkWidget *wpixmap;
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  if (strcmp (filename, "test.xpm") == 0 ||
      !file_exists (filename))
    {
      pixmap = gdk_pixmap_create_from_xpm_d (window, &mask,
					     background,
					     openfile);
    }
  else
    pixmap = gdk_pixmap_create_from_xpm (window, &mask,
					 background,
					 filename);
  
  wpixmap = gtk_image_new_from_pixmap (pixmap, mask);

  return wpixmap;
}


static void
set_toolbar_small_stock (GtkWidget *widget,
			 gpointer   data)
{
  egg_toolbar_set_icon_size (EGG_TOOLBAR (data), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

static void
set_toolbar_large_stock (GtkWidget *widget,
			 gpointer   data)
{
  egg_toolbar_set_icon_size (EGG_TOOLBAR (data), GTK_ICON_SIZE_LARGE_TOOLBAR);
}

static void
set_toolbar_horizontal (GtkWidget *widget,
			gpointer   data)
{
  egg_toolbar_set_orientation (EGG_TOOLBAR (data), GTK_ORIENTATION_HORIZONTAL);
}

static void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  egg_toolbar_set_orientation (EGG_TOOLBAR (data), GTK_ORIENTATION_VERTICAL);
}

static void
set_toolbar_icons (GtkWidget *widget,
		   gpointer   data)
{
  egg_toolbar_set_style (EGG_TOOLBAR (data), GTK_TOOLBAR_ICONS);
}

static void
set_toolbar_text (GtkWidget *widget,
	          gpointer   data)
{
  egg_toolbar_set_style (EGG_TOOLBAR (data), GTK_TOOLBAR_TEXT);
}

static void
set_toolbar_both (GtkWidget *widget,
		  gpointer   data)
{
  egg_toolbar_set_style (EGG_TOOLBAR (data), GTK_TOOLBAR_BOTH);
}

static void
set_toolbar_both_horiz (GtkWidget *widget,
			gpointer   data)
{
  egg_toolbar_set_style (EGG_TOOLBAR (data), GTK_TOOLBAR_BOTH_HORIZ);
}

static void
set_toolbar_enable (GtkWidget *widget,
		    gpointer   data)
{
  egg_toolbar_set_tooltips (EGG_TOOLBAR (data), TRUE);
}

static void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  egg_toolbar_set_tooltips (EGG_TOOLBAR (data), FALSE);
}

static void
create_toolbar (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_title (GTK_WINDOW (window), "Toolbar test");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize (window);

      toolbar = egg_toolbar_new ();

      egg_toolbar_insert_stock (EGG_TOOLBAR (toolbar),
				GTK_STOCK_NEW,
				"Stock icon: New", "Toolbar/New",
				G_CALLBACK (set_toolbar_small_stock), toolbar, -1);
      
      egg_toolbar_insert_stock (EGG_TOOLBAR (toolbar),
				GTK_STOCK_OPEN,
				"Stock icon: Open", "Toolbar/Open",
				G_CALLBACK (set_toolbar_large_stock), toolbar, -1);
      
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Horizontal", "Horizontal toolbar layout", "Toolbar/Horizontal",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_horizontal), toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Vertical", "Vertical toolbar layout", "Toolbar/Vertical",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_vertical), toolbar);

      egg_toolbar_append_space (EGG_TOOLBAR(toolbar));

      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Icons", "Only show toolbar icons", "Toolbar/IconsOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_icons), toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Text", "Only show toolbar text", "Toolbar/TextOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_text), toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Both", "Show toolbar icons and text", "Toolbar/Both",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_both), toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Both (horizontal)",
			       "Show toolbar icons and text in a horizontal fashion",
			       "Toolbar/BothHoriz",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_both_horiz), toolbar);
			       
      egg_toolbar_append_space (EGG_TOOLBAR (toolbar));

      entry = gtk_entry_new ();

      egg_toolbar_append_widget (EGG_TOOLBAR (toolbar), entry, "This is an unusable GtkEntry ;)", "Hey don't click me!!!");

      egg_toolbar_append_space (EGG_TOOLBAR (toolbar));


      egg_toolbar_append_space (EGG_TOOLBAR (toolbar));

      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Enable", "Enable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_enable), toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Disable", "Disable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_disable), toolbar);

      egg_toolbar_append_space (EGG_TOOLBAR (toolbar));

      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Frobate", "Frobate tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Baz", "Baz tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);

      egg_toolbar_append_space (EGG_TOOLBAR (toolbar));
      
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Blah", "Blah tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);
      egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			       "Bar", "Bar tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);

      gtk_container_add (GTK_CONTAINER (window), toolbar);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
make_toolbar (GtkWidget *window)
{
  GtkWidget *toolbar;

  if (!GTK_WIDGET_REALIZED (window))
    gtk_widget_realize (window);

  toolbar = egg_toolbar_new ();

  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Horizontal", "Horizontal toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_horizontal), toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Vertical", "Vertical toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_vertical), toolbar);

  egg_toolbar_append_space (EGG_TOOLBAR(toolbar));

  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Icons", "Only show toolbar icons", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_icons), toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Text", "Only show toolbar text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_text), toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Both", "Show toolbar icons and text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_both), toolbar);

  egg_toolbar_append_space (EGG_TOOLBAR (toolbar));

  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Woot", "Woot woot woot", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Blah", "Blah blah blah", "Toolbar/Big",
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);

  egg_toolbar_append_space (EGG_TOOLBAR (toolbar));

  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Enable", "Enable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_enable), toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Disable", "Disable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_disable), toolbar);

  egg_toolbar_append_space (EGG_TOOLBAR (toolbar));
  
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Hoo", "Hoo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);
  egg_toolbar_append_item (EGG_TOOLBAR (toolbar),
			   "Woo", "Woo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);

  return toolbar;
}

gint
main (gint argc, gchar **argv)
{
  gtk_init (&argc, &argv);

  create_toolbar ();

  gtk_main ();

  return 0;
}
