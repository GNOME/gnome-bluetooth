#ifndef _PYEGG_PRIVATE_H
#define _PYEGG_PRIVATE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>
#include <gtk/gtk.h>
#include "pygobject.h"

typedef struct {
        PyObject *func, *data;
} PyGtkCustomNotify;
 
void pygtk_custom_destroy_notify(gpointer user_data);
void pyegg_user_data_destroy_notify(gpointer user_data);

void iconlist_register_classes(PyObject *d);


#endif
