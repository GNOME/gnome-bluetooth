

#include <pygobject.h>
 
void controller_register_classes (PyObject *d); 
DL_EXPORT(void) initcontroller(void);
extern PyMethodDef controller_functions[];
 
DL_EXPORT(void)
initcontroller(void)
{
	PyObject *m, *d;

	init_pygobject ();

	m = Py_InitModule ("controller", controller_functions);
	d = PyModule_GetDict (m);

	controller_register_classes (d);

	if (PyErr_Occurred ()) {
		Py_FatalError ("can't initialise module controller");
	}
}
