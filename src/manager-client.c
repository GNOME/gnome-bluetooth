
#include <config.h>
#include <libbonobo.h>
#include <unistd.h>

#include "GNOME_Bluetooth_Manager.h"

int
main (int argc, char **argv)
{
  GNOME_Bluetooth_Manager btmanager;
  CORBA_Environment ev;
  GNOME_Bluetooth_DeviceList *list;
  GNOME_Bluetooth_ServiceList *slist;
  GNOME_Bluetooth_Device *dev;
  guint i,j;

  if (!bonobo_init(&argc, argv))
	g_error("Couldn't initialize bonobo");

  bonobo_activate();

  btmanager=bonobo_get_object("OAFIID:GNOME_Bluetooth_Manager",
							  "GNOME/Bluetooth/Manager", NULL);

  if (btmanager==CORBA_OBJECT_NIL) {
	g_warning("Couldn't get instance of BT Manager");
	return bonobo_debug_shutdown();
  }

  CORBA_exception_init (&ev);
  list=NULL;

#ifdef NONONONONNO
  for(i=0; i<1; i++) {
	GNOME_Bluetooth_Manager_removeAllDevices(btmanager, &ev);
	GNOME_Bluetooth_Manager_discoverDevices(btmanager, &ev);

  }
#endif

  GNOME_Bluetooth_Manager_knownDevices(btmanager, &list, &ev);
  for(i=0; i<list->_length; i++) {
	g_message("Found device addr %s name %s class %x\n",
			  list->_buffer[i].bdaddr,
			  list->_buffer[i].name,
			  list->_buffer[i].deviceclass);
	g_message("Asking separately about its name\n");
	GNOME_Bluetooth_Manager_getDeviceInfo(btmanager, &dev,
										  list->_buffer[i].bdaddr,  &ev);
		g_message("Info on device addr %s name %s class %x\n",
			  dev->bdaddr,
			  dev->name,
			  dev->deviceclass);
	CORBA_free(dev);
  }

  g_message("testing unknown device exception");
  GNOME_Bluetooth_Manager_getDeviceInfo(btmanager, &dev,
										"Froom!", &ev);
  if (BONOBO_EX(&ev)) {
	char *err=bonobo_exception_get_text (&ev);
	g_warning (_("An exception occurred '%s'"), err);
	g_free (err);
	CORBA_exception_free (&ev);
  }

  CORBA_free(dev);

  g_message("now looking for phones");
  GNOME_Bluetooth_Manager_knownDevicesOfClass(btmanager,
											  &list,
											  GNOME_Bluetooth_MAJOR_PHONE,
											  &ev);
  for(i=0; i<list->_length; i++) {
	int port=-1;
	g_message("Found device addr %s name %s class %x\n",
			  list->_buffer[i].bdaddr,
			  list->_buffer[i].name,
			  list->_buffer[i].deviceclass);
	g_message("Now looking for rfcomm port");
	port=GNOME_Bluetooth_Manager_getRFCOMMPort(btmanager,
											   list->_buffer[i].bdaddr,
											   0,
											   &ev);
	if (port>=0) {
	  g_message("Found /dev/rfcomm%d", port);
	} else {
	  g_message("Got error code %d", port);
	  /*
	  g_message("Now attempting to connect an rfcomm port to channel 1 of %s",
				list->_buffer[i].bdaddr);
	  port=GNOME_Bluetooth_Manager_connectRFCOMMPort(btmanager,
												 list->_buffer[i].bdaddr,
												 1,
												 &ev);
	  if (port>=0) {
		g_message("Connected /dev/rfcomm%d", port);
	  } else {
		g_message("Failed");
	  }
	  */
	}
  }

  g_message("now looking for computers");
  GNOME_Bluetooth_Manager_knownDevicesOfClass(btmanager,
											  &list,
											  GNOME_Bluetooth_MAJOR_COMPUTER,
											  &ev);
  for(i=0; i<list->_length; i++) {
	g_message("Found device addr %s name %s class %x\n",
			  list->_buffer[i].bdaddr,
			  list->_buffer[i].name,
			  list->_buffer[i].deviceclass);
	g_message("Now finding out which services exist on that device");

	

	GNOME_Bluetooth_Manager_servicesForDevice(btmanager,
											  &slist,
											  list->_buffer[i].bdaddr,
											  &ev);

	for(j=0; j<slist->_length; j++) {
	  GNOME_Bluetooth_ChannelList *clist;
	  guint k;

	  g_message("Found service %x", slist->_buffer[j].service);
	  clist=&slist->_buffer[j].channels;
	  for(k=0; k<clist->_length; k++)
		g_message("  on channel %d", clist->_buffer[k]);
	}

	CORBA_free(slist);
  }

  if (BONOBO_EX (&ev)) {
	char *err=bonobo_exception_get_text (&ev);
	g_warning (_("An exception occurred '%s'"), err);
	g_free (err);
  }

  CORBA_free(list);
  CORBA_exception_free (&ev);
  bonobo_object_release_unref (btmanager, NULL);
  return bonobo_debug_shutdown ();
}
