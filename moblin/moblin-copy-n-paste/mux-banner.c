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
G_DEFINE_TYPE (MuxBanner, mux_banner, GTK_TYPE_BOX);

static void
mux_banner_realize (GtkWidget *widget)
{
  MuxBanner *banner = MUX_BANNER (widget);

  GTK_WIDGET_CLASS (mux_banner_parent_class)->realize (widget);

  gdk_color_parse ("#d7d9d6", &banner->priv->colour);
}

static gboolean
mux_banner_draw (GtkWidget *widget, cairo_t *cr)
{
  MuxBanner *banner = MUX_BANNER (widget);
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  gdk_cairo_set_source_color (cr, &banner->priv->colour);
  cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
  cairo_paint (cr);

  return GTK_WIDGET_CLASS (mux_banner_parent_class)->draw (widget, cr);
}

static void
mux_banner_class_init (MuxBannerClass *klass)
{
    GtkWidgetClass *w_class = (GtkWidgetClass *)klass;

    w_class->realize = mux_banner_realize;
    w_class->draw = mux_banner_draw;

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

  widget = g_object_new (MUX_TYPE_BANNER, "orientation", GTK_ORIENTATION_HORIZONTAL, NULL);
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
