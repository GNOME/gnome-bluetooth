/*
 * Mux - a connection panel for the Moblin Netbook
 * Copyright (C) 2010 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Written by - Ross Burton <ross@linux.intel.com>
 */

#include <gtk/gtk.h>
#include "mux-banner.h"

struct _MuxBannerPrivate {
  GdkColor colour;
  GtkWidget *label;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MUX_TYPE_BANNER, MuxBannerPrivate))
G_DEFINE_TYPE (MuxBanner, mux_banner, GTK_TYPE_HBOX);

static void
mux_banner_realize (GtkWidget *widget)
{
  MuxBanner *banner = MUX_BANNER (widget);

  GTK_WIDGET_CLASS (mux_banner_parent_class)->realize (widget);

  gdk_color_parse ("#d7d9d6", &banner->priv->colour);
  gdk_colormap_alloc_color (gtk_widget_get_colormap (widget),
                            &banner->priv->colour,
                            FALSE, TRUE);
}

static gboolean
mux_banner_expose (GtkWidget *widget, GdkEventExpose *event)
{
  MuxBanner *banner = MUX_BANNER (widget);
  GdkWindow *window;
  GdkGC *gc;

  window = gtk_widget_get_window (widget);
  gc = gdk_gc_new (window);
  gdk_gc_set_foreground (gc, &banner->priv->colour);

  gdk_draw_rectangle (window, gc, TRUE,
                      event->area.x, event->area.y,
                      event->area.width, event->area.height);


  return GTK_WIDGET_CLASS (mux_banner_parent_class)->expose_event (widget, event);
}

static void
mux_banner_class_init (MuxBannerClass *klass)
{
    GtkWidgetClass *w_class = (GtkWidgetClass *)klass;

    w_class->realize = mux_banner_realize;
    w_class->expose_event = mux_banner_expose;

    g_type_class_add_private (klass, sizeof (MuxBannerPrivate));
}

static void
mux_banner_init (MuxBanner *self)
{
  self->priv = GET_PRIVATE (self);

  gtk_container_set_border_width (GTK_CONTAINER (self), 8);

  self->priv->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (self->priv->label), 0.0f, 0.5f);
  gtk_widget_show (self->priv->label);
  gtk_box_pack_start (GTK_BOX (self), self->priv->label, FALSE, FALSE, 8);
}

GtkWidget *
mux_banner_new (const char *text)
{
  GtkWidget *widget;

  widget = g_object_new (MUX_TYPE_BANNER, NULL);
  if (text)
    mux_banner_set_text ((MuxBanner *)widget, text);
  return widget;
}

void
mux_banner_set_text (MuxBanner *banner, const char *text)
{
  char *s;

  s = g_strconcat ("<big><b>", text, "</b></big>", NULL);
  gtk_label_set_markup (GTK_LABEL (banner->priv->label), s);
  g_free (s);
}
