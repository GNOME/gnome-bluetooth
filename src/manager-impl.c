#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

#include "btctl.h"
#include <bluetooth/sdp.h>

#include "manager.h"

#define GP "/system/bluetooth"


/* callbacks */
static gint device_list_compare_gconf(gconstpointer gconfname,
									  gconstpointer namestr);
static void count_cb(BTManager *m, gchar*bdaddr, gpointer data);

static void
dump_err(GError *gerr)
{
  g_error("%d %d %s\n", gerr->domain, gerr->code,
		 gerr->message);
}


static gint
device_list_compare_gconf(gconstpointer gconfname,
						  gconstpointer namestr)
{
  gchar *a,*b;
  b=(gchar*)namestr;
  a=(gchar*)gconfname;

  return g_strcasecmp(a,b);
}

static void
add_device_callback(Btctl *bc,
					gchar*    name, guint clsid,
					gpointer data)
{
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(BTMANAGER(data)->client);
  GSList *each, *list=NULL;
  gchar *key=g_strdup_printf(GP"/device/%s/class", name);

  gconf_client_set_int(client, key, clsid, &err);
  g_free(key);

  list=gconf_client_get_list(client, GP"/device/devices", GCONF_VALUE_STRING,
							 &err);

  if (!list || !g_slist_find_custom(list,
						   name,
						   device_list_compare_gconf))  {
	list=g_slist_append(list, name);
	gconf_client_set_list(client, GP"/device/devices", GCONF_VALUE_STRING,
						  list, &err);
	if (err)
	  dump_err(err);

  } else {
	g_message("Already know about %s, preparing for rediscovery", name);

	/**
	 * Some devices may change the channels services are offered
	 * on.  We should therefore purge service descriptions when a device
	 * is added.
	 */
	if (list)
	  g_slist_free(list);

	key=g_strdup_printf(GP"/device/%s", name);
	list=gconf_client_all_entries(client, key, &err);
	each=list;
	while(each) {
	  const gchar *delkey;
	  GConfEntry *entry=(GConfEntry*)each->data;
	  delkey=gconf_entry_get_key(entry);
	  if (g_strstr_len(delkey, strlen(delkey), "svc")) {
		gconf_client_unset(client, delkey, &err);
	  }
	  gconf_entry_free(entry);
	  each=g_slist_next(each);
	}

	g_free(key);
  }

  if (list) {
	g_slist_free(list);
  }

}

static void
device_name_callback(Btctl *bc, gchar *device, gchar *name, gpointer data)
{
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(BTMANAGER(data)->client);
  gchar *key=g_strdup_printf(GP"/device/%s/name", device);

  gconf_client_set_string(client,
						  key,
						  name,
						  &err);
  if (err)
	dump_err(err);
  g_free(key);
}

static void
add_device_service_callback(Btctl *bc,
							gchar *addr, gchar *name,
							guint clsid, guint port,
							gpointer data)
{
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(BTMANAGER(data)->client);
  gchar *key=g_strdup_printf(GP"/device/%s/svc%x",
							 addr, clsid);
  if (port) {
	GSList *list;
	list=gconf_client_get_list(client, key, GCONF_VALUE_INT, &err);
	if (!g_slist_find(list, GINT_TO_POINTER(port))) {
	  list=g_slist_append(list, GINT_TO_POINTER(port));
	  gconf_client_set_list(client, key, GCONF_VALUE_INT, list, &err);
	}
	if (list)
	  g_slist_free(list);
  }
  printf("device %s (%s) port %d\n",
		 addr, name, port);
  g_free(key);
}

static void
remove_device(GConfClient *client,
			  gchar *name)
{
  GError *err=NULL;
  GSList *list=NULL;
  GSList *victim,*each;
  gchar *key;

  // TODO: figure out memory leaks here

  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);

  victim=g_slist_find_custom(list,
							 name,
							 device_list_compare_gconf);
  if (victim) {

	list=g_slist_delete_link(list, victim);

	gconf_client_set_list(client,
						  GP"/device/devices",
						  GCONF_VALUE_STRING,
						  list,
						  &err);

	// now to remove everything in the device's dir
	key=g_strdup_printf(GP"/device/%s", name);
	list=gconf_client_all_entries(client, key, &err);
	each=list;
	while(each) {
	  GConfEntry *entry=(GConfEntry*)each->data;
	  gconf_client_unset(client, gconf_entry_get_key(entry), &err);
	  gconf_entry_free(entry);
	  each=g_slist_next(each);
	}
	if (list)
	  g_slist_free(list);
  }
}

static void
remove_all_devices(GConfClient *client)
{
  GError *err=NULL;
  GSList *item,*list=NULL;

  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);
  item=list;
  while(item) {
	remove_device(client, (gchar*)item->data);
	item=g_slist_next(item);
  }
  g_slist_free(list);

}

void
btmanager_impl_remove_all_devices(BTManager *mgr)
{
  remove_all_devices(mgr->client);
}

void
btmanager_impl_discover_devices(BTManager *mgr)
{
  btctl_discover_devices(mgr->bc);
}

BTDeviceDesc *
btmanager_impl_new_device_desc(BTManager *mgr,
							   gchar     *bdaddr)
{ GError *err=NULL;
  GConfClient *client=mgr->client;
  GSList *victim,*list=NULL;
  BTDeviceDesc *ret=NULL;


  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);

  victim=g_slist_find_custom(list,
							 bdaddr,
							 device_list_compare_gconf);
  if (victim) {
	ret=g_new0(BTDeviceDesc, 1);
	if (ret) {
	  gchar *key=g_strdup_printf(GP"/device/%s/name", bdaddr);
	  ret->name=g_strdup(gconf_client_get_string(client, key, &err));
	  ret->bdaddr=g_strdup(bdaddr);
	  g_free(key);
	  key=g_strdup_printf(GP"/device/%s/class", bdaddr);
	  ret->deviceclass=gconf_client_get_int(client, key, &err);
	  g_free(key);
	}
  }

  g_slist_free(list);

  return ret;
}

void
btmanager_impl_free_device_desc(BTManager *mgr, BTDeviceDesc *dd) {
  if (dd) {
	if (dd->name)
	  g_free(dd->name);
	if (dd->bdaddr)
	  g_free(dd->bdaddr);
	g_free(dd);
  }
}

static void
count_cb(BTManager *m, gchar*bdaddr, gpointer data)
{
  guint *count=(guint*)data;
  (*count)++;
}

guint
btmanager_impl_num_known_devices(BTManager *mgr)
{
  guint count=0;

  btmanager_impl_for_each_known_device(mgr, count_cb, &count);
  return count;
}

guint
btmanager_impl_num_known_devices_filtered(BTManager *mgr,
										  BTDeviceFilter filt,
										  gpointer data)
{
  guint count=0;

  btmanager_impl_for_each_known_device_filtered(mgr, count_cb, filt,
												&count, data);
  return count;
}

void
btmanager_impl_for_each_known_device(BTManager *mgr,
									 BTDeviceCallback cb,
									 gpointer data)
{
  btmanager_impl_for_each_known_device_filtered(mgr, cb, NULL, data, NULL);
}

void
btmanager_impl_for_each_known_device_filtered(BTManager *mgr,
											  BTDeviceCallback cb,
											  BTDeviceFilter filt,
											  gpointer data,
											  gpointer filterdata)
{
  GError *err=NULL;
  GConfClient *client=mgr->client;
  GSList *item,*list=NULL;

  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);

  for(item=list; item!=NULL; item=g_slist_next(item)) {
	if (!filt || ((*filt)(mgr, (gchar*)item->data, filterdata))) {
	  (*cb)(mgr, (gchar*)item->data, data);
	}
  }

  g_slist_free(list);
}
void
btmanager_impl_init(BTManager *mgr)
{
  Btctl *bc;
  mgr->client=gconf_client_get_default();

  bc=BTCTL(btctl_new());

  g_signal_connect (G_OBJECT(bc), "add_device",
					G_CALLBACK(add_device_callback),
					(gpointer)mgr);
  g_signal_connect (G_OBJECT(bc), "device_name",
					G_CALLBACK(device_name_callback),
					(gpointer)mgr);
  g_signal_connect (G_OBJECT(bc), "add_device_service",
					G_CALLBACK(add_device_service_callback),
					(gpointer)mgr);

  mgr->bc=bc;
}

gint
btmanager_impl_connect_rfcomm_port(BTManager *mgr, gchar* bdaddr, guint chan)
{
  return btctl_establish_rfcomm_connection(mgr->bc, bdaddr, chan);
}

gint
btmanager_impl_get_rfcomm_port(BTManager *mgr, gchar* bdaddr, guint chan)
{
  return btctl_get_established_rfcomm_connection(mgr->bc, bdaddr, chan);
}
void
btmanager_impl_shutdown(BTManager *mgr)
{
  g_object_unref(mgr->bc);
  g_object_unref(mgr->client);

}

GSList *
btmanager_impl_channels_for_service(BTManager *mgr, const gchar *bdaddr, guint svcid)
{
  GConfClient *client;
  GSList *list;
  GError *err=NULL;
  gchar *key=g_strdup_printf(GP"/device/%s/svc%x",
							 bdaddr, svcid);

  client=mgr->client;

  /* first, we clear the existing list and ask the device
   * itself.
   */
  gconf_client_set_list(client, key, GCONF_VALUE_INT, NULL, &err);
  btctl_scan_for_service(mgr->bc, bdaddr, svcid);

  /* g_message("Getting list for key %s", key); */
  list=gconf_client_get_list(client, key, GCONF_VALUE_INT, &err);
  g_free(key);
  return list;
}

GSList *
btmanager_impl_services_for_device(BTManager *mgr, const gchar *bdaddr)
{
  GConfClient *client;
  GSList *result=NULL;
  GConfEntry *entry;
  GSList *item,*entries;
  GError *err=NULL;
  guint svcno, keyoffset;
  BTServiceDesc *desc=NULL;

  gchar *key=g_strdup_printf(GP"/device/%s", bdaddr);

  keyoffset=strlen(key)+1;
  client=mgr->client;
  entries=gconf_client_all_entries(client, key, &err);
  g_free(key);

  for(item=entries; item!=NULL; item=g_slist_next(item)) {
	  entry=(GConfEntry *)item->data;
      key=(gchar*)gconf_entry_get_key(entry);
	  if (g_ascii_strncasecmp("svc", &key[keyoffset], 3)==0) {
		svcno=(guint)strtol(&key[keyoffset+3], NULL, 16);
		desc=g_new0(BTServiceDesc, 1);
		desc->id=svcno;
		desc->channels=gconf_client_get_list(client, key, GCONF_VALUE_INT, &err);
		result=g_slist_append(result, (gpointer)desc);
	  }
  }

  return result;
}

void
btmanager_free_service_list(GSList *list)
{
	GSList *item;
	BTServiceDesc *desc;

	for(item=list; item!=NULL; item=g_slist_next(item)) {
		desc=(BTServiceDesc *)item->data;
		if (desc) {
			g_slist_free(desc->channels);
		}
		g_free(desc);
	}
	g_slist_free(item);
}

gchar *
btmanager_impl_get_device_name(BTManager *bm, const gchar *bdaddr)
{
  GError *err=NULL;
  GConfClient *client=bm->client;
  gchar *name;
  gchar *key=g_strdup_printf(GP"/device/%s/name", bdaddr);
  name=gconf_client_get_string(client, key, &err);
  g_free(key);
  if (err) {
	g_clear_error(&err);
	return NULL;
  } else {
	name=g_strdup(name);
  }
  return name;
}

guint
btmanager_impl_get_device_class(BTManager *bm, const gchar *bdaddr)
{
  GError *err=NULL;
  GConfClient *client=bm->client;
  guint dclass;

  gchar *key=g_strdup_printf(GP"/device/%s/class", bdaddr);
  dclass=(guint)gconf_client_get_int(client, key, &err);
  g_free(key);

  if (err) {
	g_clear_error(&err);
	return 0;
  }
  return dclass;
}
