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
#ifndef __EGG_FILE_SELECTOR_H__
#define __EGG_FILE_SELECTOR_H__

#include <gtk/gtkdialog.h>
#include <gtk/gtkliststore.h>

#include "eggfilesystem.h"
#include "eggfilefilter.h"

G_BEGIN_DECLS

#define EGG_TYPE_FILE_SELECTOR		  (egg_file_selector_get_type ())
#define EGG_FILE_SELECTOR(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_FILE_SELECTOR, EggFileSelector))
#define EGG_FILE_SELECTOR_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_FILE_SELECTOR, EggFileSelectorClass))
#define EGG_IS_FILE_SELECTOR(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_FILE_SELECTOR))
#define EGG_IS_FILE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_FILE_SELECTOR))
#define EGG_FILE_SELECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_FILE_SELECTOR, EggFileSelectorClass))

typedef struct _EggFileSelector      EggFileSelector;
typedef struct _EggFileSelectorClass EggFileSelectorClass;
typedef struct _EggFileSelectorPrivate _EggFileSelectorPrivate;

struct _EggFileSelector
{
  GtkDialog parent;
  _EggFileSelectorPrivate *priv;
};

struct _EggFileSelectorClass
{
  GtkDialogClass parent_class;
};

GType    egg_file_selector_get_type          (void);
GtkWidget *egg_file_selector_new (const gchar *title);

EggFileFilter *egg_file_selector_get_base_filter (void);

void egg_file_selector_set_select_multiple (EggFileSelector *filesel,
					    gboolean select_multiple);
gboolean egg_file_selector_get_select_multiple (EggFileSelector *filesel);

void     egg_file_selector_set_confirm_stock (EggFileSelector *selector,
					      const gchar     *stock_id);
void     egg_file_selector_add_choosable_filter (EggFileSelector *selector,
						 EggFileFilter   *filter);

void egg_file_selector_set_filename (EggFileSelector *selector,
			             const gchar *path);

gchar *egg_file_selector_get_filename (EggFileSelector *selector);

GList *egg_file_selector_get_filename_list (EggFileSelector *selector);

G_END_DECLS

#endif /* __EGG_FILE_SELECTOR_H__ */
