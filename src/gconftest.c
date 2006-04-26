
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

#include "btctl.h"

#define GP "/system/bluetooth"

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
add_device_callback(BtctlController *bc,
					gchar*    name, guint clsid,
					gpointer data) 
{
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(data);
  GSList *list=NULL;
  gchar *key=g_strdup_printf(GP"/device/%s/class", name);

  gconf_client_set_int(client, key, clsid, &err);
  g_free(key);

  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);

  if (!list || !g_slist_find_custom(list,
						   name,
						   device_list_compare_gconf))  {
	list=g_slist_append(list, name);
	gconf_client_set_list(client,
						  GP"/device/devices",
						  GCONF_VALUE_STRING,
						  list,
						  &err);
	if (err)
	  dump_err(err);

  } else {
	g_message("already know about %s", name);
  }
  
  if (list) {
	g_slist_free(list);
  }
}

static void 
device_name_callback(BtctlController *bc,
					 gchar*   device,
					 gchar*   name,
					 gpointer data) 
{ 
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(data);
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
add_device_service_callback(BtctlController *bc,
							gchar *addr, gchar *name, 
							guint clsid, guint port, 
							gpointer data)
{
  GError *err=NULL;
  GConfClient *client=GCONF_CLIENT(data);
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
  // TODO: figure out memory leaks here

  list=gconf_client_get_list(client,
							 GP"/device/devices",
							 GCONF_VALUE_STRING,
							 &err);
  item=list;
  while(item) {
	remove_device(client, (gchar*)item->data);
	item=g_slist_next(item);
  }
}

int
main (int argc, char **argv) 
{
  GConfClient *client;
  GError *err;
  BtctlController *bc;

  g_type_init();

  if (!gconf_init(argc, argv, &err)) {
	g_error("Error initializing gconf");
	return 1;
  }

  client=gconf_client_get_default();

  remove_all_devices(client);

  bc=btctl_controller_new(NULL);

  g_signal_connect (G_OBJECT(bc), "add_device",
					G_CALLBACK(add_device_callback),
					(gpointer)client);
  g_signal_connect (G_OBJECT(bc), "device_name",
					G_CALLBACK(device_name_callback), 
					(gpointer)client); 
  g_signal_connect (G_OBJECT(bc), "add_device_service",
					G_CALLBACK(add_device_service_callback),
					(gpointer)client);
  btctl_controller_discover_devices(bc, NULL);
  g_object_unref(bc);

  g_object_unref(client);
  return 0;
}
