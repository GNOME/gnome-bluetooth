#include <config.h>
#include <libbonobo.h>

#include "GNOME_Bluetooth_Manager.h"
#include "manager.h"

/* the intent is to try and limit
   most of the CORBA nastiness
   to inside this file :) */

#define bmthrow(exception,returnval)                         \
  {                                                          \
      GNOME_Bluetooth_##exception *exn;                      \
          exn = GNOME_Bluetooth_##exception##__alloc();      \
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
                   ex_GNOME_Bluetooth_##exception, exn);     \
      return returnval;                                      \
  }

#define bmthrow_no_val(exception)                            \
  {                                                          \
      GNOME_Bluetooth_##exception *exn;                      \
          exn = GNOME_Bluetooth_##exception##__alloc();      \
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
                   ex_GNOME_Bluetooth_##exception, exn);     \
      return returnval;                                      \
  }

#define bmthrow_with_settings(exception,returnval,name,commands) \
  {                                                              \
      GNOME_Bluetooth_##exception *name;                         \
          name = GNOME_Bluetooth_##exception##__alloc();             \
          { commands; }                                          \
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
                   ex_GNOME_Bluetooth_##exception, name);\
      return returnval;                                      \
  }

#define bmthrow_no_val_with_settings(exception,name,commands)    \
  {                                                              \
      GNOME_Bluetooth_##exception *name;                         \
          name = GNOME_Bluetooth_##exception##__alloc();             \
          { commands; }                                          \
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
                   ex_GNOME_Bluetooth_##exception, name);\
      return;                                                \
  }

static gpointer parent_class=NULL;

static void
transfer_device_desc_to_corba_val(BTManager *bm,
								  gchar *bdaddr,
								  gpointer data)
{
  BTDeviceDesc *dd;
  GNOME_Bluetooth_DeviceList ** list=data;

  dd=btmanager_impl_new_device_desc(bm, bdaddr);
  (*list)->_buffer[(*list)->_length].name=CORBA_string_dup(dd->name);
  (*list)->_buffer[(*list)->_length].bdaddr=CORBA_string_dup(dd->bdaddr);
  (*list)->_buffer[(*list)->_length].deviceclass=dd->deviceclass;
  btmanager_impl_free_device_desc(bm, dd);
  (*list)->_length++;
}

static void
impl_btmanager_services_for_device(PortableServer_Servant	servant,
		GNOME_Bluetooth_ServiceList ** list,
		const CORBA_char *bdaddr,
		CORBA_Environment *ev)
{
	BTManager *bm;
	GSList *svclist,*item,*citem;
	guint num;
	guint numchans;

	bm=BTMANAGER(bonobo_object(servant));
	svclist=btmanager_impl_services_for_device(bm, bdaddr);
	num=g_slist_length(svclist);

	*list=GNOME_Bluetooth_ServiceList__alloc();
	(*list)->_length=0;
	(*list)->_maximum=num+1;
	(*list)->_buffer=GNOME_Bluetooth_ServiceList_allocbuf(num);
	for(item=svclist; item; item=g_slist_next(item)) {
		BTServiceDesc *desc=(BTServiceDesc *)item->data;
		numchans=g_slist_length(desc->channels);
		(*list)->_buffer[(*list)->_length].service=desc->id;
		(*list)->_buffer[(*list)->_length].channels._length=0;
		(*list)->_buffer[(*list)->_length].channels._maximum=numchans+1;
		(*list)->_buffer[(*list)->_length].channels._buffer=GNOME_Bluetooth_ChannelList_allocbuf(numchans);
		for(citem=desc->channels; citem; citem=g_slist_next(citem)) {
		  (*list)->_buffer[(*list)->_length].channels._buffer[(*list)->_buffer[(*list)->_length].channels._length]=(CORBA_long)citem->data;
		  (*list)->_buffer[(*list)->_length].channels._length++;
		}
		(*list)->_length++;
	}
	btmanager_free_service_list(svclist);
}


static void
impl_btmanager_channels_for_service(PortableServer_Servant  servant,
									GNOME_Bluetooth_ChannelList ** list,
									const CORBA_char * bdaddr,
									GNOME_Bluetooth_ServiceID service,
									CORBA_Environment *ev)
{
  BTManager *bm;
  GSList *chanlist,*item;
  guint num;

  bm=BTMANAGER(bonobo_object(servant));
  chanlist=btmanager_impl_channels_for_service(bm, (gchar*)bdaddr, service);
  num=g_slist_length(chanlist);
  *list=GNOME_Bluetooth_ChannelList__alloc();
  (*list)->_length=0;
  (*list)->_maximum=num+1;
  (*list)->_buffer=GNOME_Bluetooth_ChannelList_allocbuf(num);
  for(item=chanlist; item; item=g_slist_next(item)) {
	(*list)->_buffer[(*list)->_length]=(GNOME_Bluetooth_ServiceID)item->data;
	(*list)->_length++;
  }
  g_slist_free(chanlist);
}

static void
impl_btmanager_known_devices (PortableServer_Servant  servant,
							  GNOME_Bluetooth_DeviceList ** list,
							  CORBA_Environment *ev)
{
  BTManager *bm;
  guint num;

  bm=BTMANAGER(bonobo_object(servant));

  num=btmanager_impl_num_known_devices(bm);

  *list=GNOME_Bluetooth_DeviceList__alloc();
  (*list)->_length=0;
  (*list)->_maximum=num+1;
  (*list)->_buffer=GNOME_Bluetooth_DeviceList_allocbuf(num);
  btmanager_impl_for_each_known_device(bm, transfer_device_desc_to_corba_val,
									   (gpointer)list);
}

static int
filter_by_class(BTManager *bm, gchar *bdaddr, gpointer data)
{
  BTDeviceDesc *dd;
  guint *clsptr=data;
  int res;
  dd=btmanager_impl_new_device_desc(bm, bdaddr);
  res=IS_MAJOR_CLASS(dd->deviceclass, *clsptr);
  btmanager_impl_free_device_desc(bm, dd);
  return res;
}

static void
impl_btmanager_known_devices_of_class (PortableServer_Servant  servant,
									   GNOME_Bluetooth_DeviceList ** list,
									   const GNOME_Bluetooth_MajorDeviceClass clsid,
									   CORBA_Environment *ev)
{
  BTManager *bm;
  gint num;

  bm=BTMANAGER(bonobo_object(servant));
  num=btmanager_impl_num_known_devices_filtered(bm, filter_by_class,
												(gpointer)&clsid);

  *list=GNOME_Bluetooth_DeviceList__alloc();
  (*list)->_length=0;
  (*list)->_maximum=num+1;
  (*list)->_buffer=GNOME_Bluetooth_DeviceList_allocbuf(num);
  btmanager_impl_for_each_known_device_filtered(bm,
												transfer_device_desc_to_corba_val,
												filter_by_class,
												(gpointer)list,
												(gpointer)&clsid);
}

static CORBA_long
impl_btmanager_get_rfcomm_port(PortableServer_Servant servant,
							   const CORBA_char * bdaddr,
							   const CORBA_long channel,
							   CORBA_Environment * ev)
{
  BTManager *bm;
  gint num;

  bm=BTMANAGER(bonobo_object(servant));

  num=btmanager_impl_get_rfcomm_port(bm, (gchar*)bdaddr, channel);

  return num;
}

static CORBA_long
impl_btmanager_get_rfcomm_port_by_service(PortableServer_Servant servant,
										  const CORBA_char * bdaddr,
										  const GNOME_Bluetooth_ServiceID svcid,
										  CORBA_Environment * ev)
{
  BTManager *bm;
  GSList *chanlist,*item;
  gint num=-1;

  bm=BTMANAGER(bonobo_object(servant));

  chanlist=btmanager_impl_channels_for_service(bm, (gchar*)bdaddr, svcid);
  for(item=chanlist; item && (!num>=0); item=g_slist_next(item)) {
	num=btmanager_impl_get_rfcomm_port(bm, (gchar*)bdaddr,
									   (guint)item->data);

  }

  return num;
}

static CORBA_long
impl_btmanager_connect_rfcomm_port(PortableServer_Servant servant,
								   const CORBA_char * bdaddr,
								   const CORBA_long channel,
								   CORBA_Environment * ev)
{
  BTManager *bm;
  gint num;

  bm=BTMANAGER(bonobo_object(servant));

  num=btmanager_impl_get_rfcomm_port(bm, (gchar*)bdaddr, channel);

  // NB for some reason the compiler didn't do num<0 properly :(
  if (!num>=0) {
	num=btmanager_impl_connect_rfcomm_port(bm, (gchar*)bdaddr, channel);
  }

  return num;
}

static CORBA_long
impl_btmanager_connect_rfcomm_port_by_service(PortableServer_Servant servant,
										  const CORBA_char * bdaddr,
										  const GNOME_Bluetooth_ServiceID svcid,
										  CORBA_Environment * ev)
{
  BTManager *bm;
  GSList *chanlist,*item;
  gint num=-1;

  bm=BTMANAGER(bonobo_object(servant));

  chanlist=btmanager_impl_channels_for_service(bm, (gchar*)bdaddr, svcid);
  for(item=chanlist; item; item=g_slist_next(item)) {
	num=btmanager_impl_get_rfcomm_port(bm, (gchar*)bdaddr,
									   (guint)item->data);
	g_message("sniffing for %s chan %d, got %d",
			  (gchar*)bdaddr,(guint)item->data,num);

	if (num<0) {
	  num=btmanager_impl_connect_rfcomm_port(bm, (gchar*)bdaddr,
											 (guint)item->data );
	  g_message("attempting to connect %s chan %d, got %d",
				(gchar*)bdaddr,(guint)item->data,num);
	}
	if (num>=0)
	  break;
  }

  return num;
}

static void
impl_btmanager_remove_all_devices(PortableServer_Servant  servant,
								   CORBA_Environment *ev)
{
  BTManager *bm;
  bm=BTMANAGER(bonobo_object(servant));
  btmanager_impl_remove_all_devices(bm);
}

static void
impl_btmanager_discover_devices(PortableServer_Servant  servant,
								CORBA_Environment *ev)
{
  BTManager *bm;
  bm=BTMANAGER(bonobo_object(servant));
  btmanager_impl_discover_devices(bm);
}

static void
impl_btmanager_get_device_info(PortableServer_Servant  servant,
							   GNOME_Bluetooth_Device ** dev,
							   const CORBA_char * bdaddr,
							   CORBA_Environment *ev)
{
  BTManager *bm;
  gchar *res;
  bm=BTMANAGER(bonobo_object(servant));
  *dev=GNOME_Bluetooth_Device__alloc();
  (*dev)->bdaddr=CORBA_string_dup(bdaddr);
  res=btmanager_impl_get_device_name(bm, bdaddr);
  if (res==NULL) {
	/* oh dear, invalid or unknown bdaddr, throw an
	 UnknownDevice exception */
	bmthrow_no_val_with_settings(UnknownDevice,exn,exn->bdaddr=CORBA_string_dup(bdaddr));
  } else {
	(*dev)->name=CORBA_string_dup(res);
	g_free(res);
	(*dev)->deviceclass=btmanager_impl_get_device_class(bm, bdaddr);
  }
}

static void
btmanager_init(BTManager *bm)
{
  btmanager_impl_init(bm);
}

static void
btmanager_object_finalize(GObject *obj)
{
  BTManager *bm;
  bm=BTMANAGER(obj);
  // custom destroy stuff here
  btmanager_impl_shutdown(bm);
  G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
btmanager_class_init(BTManagerClass *klass)
{
  GObjectClass *object_class;
  POA_GNOME_Bluetooth_Manager__epv *epv;

  parent_class = g_type_class_peek_parent(klass);

  object_class = (GObjectClass*)klass;
  epv=&klass->epv;
  object_class->finalize=btmanager_object_finalize;

  epv->knownDevices=impl_btmanager_known_devices;
  epv->knownDevicesOfClass=impl_btmanager_known_devices_of_class;
  epv->removeAllDevices=impl_btmanager_remove_all_devices;
  epv->discoverDevices=impl_btmanager_discover_devices;
  epv->getRFCOMMPort=impl_btmanager_get_rfcomm_port;
  epv->getRFCOMMPortByService=impl_btmanager_get_rfcomm_port_by_service;
  epv->connectRFCOMMPort=impl_btmanager_connect_rfcomm_port;
  epv->connectRFCOMMPortByService=impl_btmanager_connect_rfcomm_port_by_service;
  epv->channelsForService=impl_btmanager_channels_for_service;
  epv->servicesForDevice=impl_btmanager_services_for_device;
  epv->getDeviceInfo=impl_btmanager_get_device_info;
}

BONOBO_TYPE_FUNC_FULL(BTManager,
					  GNOME_Bluetooth_Manager,
					  BONOBO_TYPE_OBJECT,
					  btmanager);

BTManager *
btmanager_new(void)
{
  return g_object_new (btmanager_get_type(), NULL);
}
