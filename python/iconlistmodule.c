
#include <pygobject.h>
 
void iconlist_register_classes (PyObject *d); 
void initiconlist (void);
extern PyMethodDef iconlist_functions[];
 
DL_EXPORT(void)
initiconlist(void)
{
    PyObject *m, *d;
 
    init_pygobject ();
 
    m = Py_InitModule ("iconlist", iconlist_functions);
    d = PyModule_GetDict (m);
 
    iconlist_register_classes (d);
 
    if (PyErr_Occurred ()) {
        Py_FatalError ("can't initialise module iconlist");
    }
}

