

#include <pygobject.h>
 
void chooser_register_classes (PyObject *d); 
DL_EXPORT(void) initchooser(void);
extern PyMethodDef chooser_functions[];
 
DL_EXPORT(void)
initchooser(void)
{
	PyObject *m, *d;

	init_pygobject ();

	m = Py_InitModule ("chooser", chooser_functions);
	d = PyModule_GetDict (m);

	chooser_register_classes (d);

	if (PyErr_Occurred ()) {
		Py_FatalError ("can't initialise module chooser");
	}
}
