/* -*- Mode: C; c-basic-offset: 4 -*-
 * pygtk- Python bindings for the GTK toolkit.
 * Copyright (C) 1998-2003  James Henstridge
 *
 *   gtkobject-support.c: some helper routines for the GTK module.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* this module provides some of the base functionality of the GtkObject
 * wrapper system */

#include "pyegg-private.h"

/* ------------------- object support */

void
pygtk_custom_destroy_notify(gpointer user_data)
{
    PyGtkCustomNotify *cunote = user_data;

    pyg_block_threads();
    Py_XDECREF(cunote->func);
    Py_XDECREF(cunote->data);
    pyg_unblock_threads();
    g_free(cunote);
}

/* extra stuff for libegg, Edd Dumbill */

/* when egg_icon_list_item_set_user_data is called, we increase
 * the reference count of the data concerned.  on destroy, we need
 * to decrease that count
 */
void
pyegg_user_data_destroy_notify(gpointer user_data)
{
    PyObject *data = user_data;
    
    pyg_block_threads();
    if (data) {
        Py_XDECREF(data);
    }
    pyg_unblock_threads();
}
