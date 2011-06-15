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

#ifndef __MUX_BANNER_H__
#define __MUX_BANNER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MUX_TYPE_BANNER (mux_banner_get_type())
#define MUX_BANNER(obj)                                             \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                MUX_TYPE_BANNER,                    \
                                MuxBanner))
#define MUX_BANNER_CLASS(klass)                                     \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             MUX_TYPE_BANNER,                       \
                             MuxBannerClass))
#define IS_MUX_BANNER(obj)                                          \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                MUX_TYPE_BANNER))
#define IS_MUX_BANNER_CLASS(klass)                                  \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             MUX_TYPE_BANNER))
#define MUX_BANNER_GET_CLASS(obj)                                   \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               MUX_TYPE_BANNER,                     \
                               MuxBannerClass))

typedef struct _MuxBannerPrivate MuxBannerPrivate;
typedef struct _MuxBanner      MuxBanner;
typedef struct _MuxBannerClass MuxBannerClass;

struct _MuxBanner {
  GtkBox parent;
  MuxBannerPrivate *priv;
};

struct _MuxBannerClass {
  GtkBoxClass parent_class;
};

GType mux_banner_get_type (void) G_GNUC_CONST;

GtkWidget *mux_banner_new (const char *text);

void mux_banner_set_text (MuxBanner *banner, const char *text);

G_END_DECLS

#endif /* __MUX_BANNER_H__ */
