
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gnomebt-icons.h"

GdkPixbuf *
gnomebt_icon(void)
{
  GError *err=NULL;
  return gdk_pixbuf_new_from_file(DATA_DIR"/pixmaps/blueradio-48.png",
								  &err);
}
