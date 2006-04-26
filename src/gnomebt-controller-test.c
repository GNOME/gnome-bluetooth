
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <unistd.h>

#include "gnomebt-controller.h"

/* change this to a device you know works */

#define TBDADDR "00:80:37:2A:B6:BC"
#define DIALUP_NET_SVCLASS_ID           0x1103

static void device_name_callback(GnomebtController *bc,
        gchar*   device,
        gchar*   name,
        gpointer data) {

    printf("device %s is called %s\n", device, name);
}

int main(int argc, char **argv)
{
    GnomebtController *bc;
    GSList *services, *item, *chans, *devices;
    int num;
    gchar *devname;
    FILE *f;
    int complete = 0;

    g_type_init();

    bc=gnomebt_controller_new();

    /* these are our own signal handlers.  note that the
     * gnomebt_controller has some too for the same signals,
     * which update the gconf registry with cached information
     */
    
    g_signal_connect (G_OBJECT(bc), "device_name",
                G_CALLBACK(device_name_callback), NULL);

    printf("Removing entry 00:40:8C:5E:5D:A4\n");

    gnomebt_controller_remove_device (bc, "00:40:8C:5E:5D:A4");

	btctl_controller_list_rfcomm_connections(BTCTL_CONTROLLER(bc));

	printf("Connection to " TBDADDR " %d\n", 
		   btctl_controller_get_established_rfcomm_connection(BTCTL_CONTROLLER(bc), TBDADDR, 0));
    printf("Doing synchronous discovery\n");
    gnomebt_controller_discover_devices(bc);

    printf("Preferred name for " TBDADDR " is %s\n",
            gnomebt_controller_get_device_preferred_name(bc, TBDADDR));
    printf("Setting alias for " TBDADDR " to 'testalias'\n");
    gnomebt_controller_set_device_alias(bc, TBDADDR, "testalias");
    printf("Alias for " TBDADDR " is %s\n",
            gnomebt_controller_get_device_alias(bc, TBDADDR));
    printf("Preferred name for " TBDADDR " is %s\n",
            gnomebt_controller_get_device_preferred_name(bc, TBDADDR));
    printf("Removing alias\n");
    gnomebt_controller_remove_device_alias(bc, TBDADDR);
    printf("Preferred name for " TBDADDR " is %s\n",
            gnomebt_controller_get_device_preferred_name(bc, TBDADDR));

    printf("Permission for " TBDADDR " is %d\n", 
            gnomebt_controller_get_device_permission(bc, TBDADDR));
    printf("Setting permission to %d\n", GNOMEBT_PERM_ASK);
    gnomebt_controller_set_device_permission(bc, TBDADDR, GNOMEBT_PERM_ASK);
    printf("Permission for " TBDADDR " is %d\n", 
            gnomebt_controller_get_device_permission(bc, TBDADDR));
    printf("Setting permission to %d\n", GNOMEBT_PERM_ALWAYS);
    gnomebt_controller_set_device_permission(bc, TBDADDR, GNOMEBT_PERM_ALWAYS);
    printf("Setting permission to invalid value %d\n", 34);
    gnomebt_controller_set_device_permission(bc, TBDADDR, 34);

    printf("Looking to see what channel OPUSH is on " TBDADDR "\n");

    btctl_controller_scan_for_service(BTCTL_CONTROLLER(bc), TBDADDR, 0x1105, NULL);

    printf("Examining " TBDADDR ", name %s class %x\n",
            gnomebt_controller_get_device_name(bc, TBDADDR),
            gnomebt_controller_get_device_class(bc, TBDADDR));
    

    printf("Services supported:\n");

    services = gnomebt_controller_services_for_device (bc, TBDADDR);
    for (item=services; item != NULL; item = g_slist_next (item)) {
        GnomebtServiceDesc *desc = (GnomebtServiceDesc *)item->data;
        printf ("   Service ID: %x\n", desc->id);
        for (chans=desc->channels; chans!=NULL; chans = g_slist_next (chans)) {
            printf ("      Channel: %d\n", GPOINTER_TO_UINT(chans->data));
        }
    }
    gnomebt_service_list_free (services);

    chans = gnomebt_controller_channels_for_service (bc, TBDADDR, 0x1101);
    printf ("Channels for Service ID: %x\n", 0x1101);
    for ( ; chans!=NULL; chans = g_slist_next (chans)) {
        printf ("  Channel: %d\n", GPOINTER_TO_UINT(chans->data));
    }

    printf ("Known devices:\n");
    for (devices = gnomebt_controller_known_devices (bc); devices != NULL; devices = g_slist_next (devices)) {
        GnomebtDeviceDesc *dd = (GnomebtDeviceDesc *)devices->data;
        printf ("Name %s Address %s Class %x\n",
                dd->name, dd->bdaddr, dd->deviceclass);
    }
    gnomebt_device_desc_list_free (devices);

    printf ("Known devices that are phones:\n");
    for (devices = gnomebt_controller_known_devices_of_class (bc, GNOMEBT_MAJOR_PHONE ); devices != NULL; devices = g_slist_next (devices)) {
        GnomebtDeviceDesc *dd = (GnomebtDeviceDesc *)devices->data;
        printf ("Name %s Address %s Class %x\n",
                dd->name, dd->bdaddr, dd->deviceclass);
    }
    gnomebt_device_desc_list_free (devices);

    printf ("Testing RFCOMM\n");
    
    num = gnomebt_controller_connect_rfcomm_port_by_service (bc, TBDADDR,
            DIALUP_NET_SVCLASS_ID);

    printf ("Connected dialup on channel %d\n", num);
    printf ("Now sending data to /dev/rfcomm%d\n", num);
    devname=g_strdup_printf("/dev/rfcomm%d", num);
    f=fopen(devname, "wb");
    g_free(devname);
    if (f) {
        fprintf (f, "foo");
        fclose(f);
    } else {
        printf("open failed\n");
    }
    
    g_object_unref(bc);

    return 0;
}
