
#include <config.h>

#include <libbonobo.h>
#include <gconf/gconf-client.h>

#include "GNOME_Bluetooth_Manager.h"
#include "manager.h"

static BonoboObject *
btmanager_factory(BonoboGenericFactory *this_factory, 
				  const char *component_id, gpointer closure)
{
  BTManager *btmanager;

  btmanager=btmanager_new();
  if (btmanager==NULL) {
	g_error("failed to create btmanager object");
	return NULL;
  }
  return BONOBO_OBJECT(btmanager);
}

int main (int argc, char *argv [])  
{   
  GError *err;

  if (!bonobo_init (&argc, argv))   
	g_error(_("Could not initialize Bonobo"));

  if (!gconf_init(argc, argv, &err))
	g_error(_("Could not initialize gconf"));

  return bonobo_generic_factory_main
	("OAFIID:GNOME_Bluetooth_Manager_Factory" ,
	 btmanager_factory,  NULL);    
}

/*
  BONOBO_ACTIVATION_FACTORY ("OAFIID:GNOME_Bluetooth_Manager_Factory",
  "GNOME Bluetooth Manager factory", "1.0",
  btmanager_factory, NULL);
*/
