#include "config.h"

#include <libbonobo.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

                                                                                
#include "GNOME_Bluetooth_Manager.h"

typedef struct {
    GConfClient *client;
    GladeXML *ui;
    GNOME_Bluetooth_Manager btmanager;
    CORBA_Environment ev;
    GNOME_Bluetooth_DeviceList *list;
    GtkListStore *devstore;
    GtkListStore *devinfstore;
    GtkListStore *servicestore;
    GtkTreeView *devtreeview;
    GtkTreeView *devinftreeview;
    GtkTreeView *servicetreeview;
	guint cur_device;
} BTAdmin;

enum 
{
    DEV_NAME_COLUMN,
	DEV_NUM_COLUMN,
    DEV_NUM_COLUMNS
};

enum
{
    DEVINF_NAME_COLUMN,
    DEVINF_DESC_COLUMN,
    DEVINF_NUM_COLUMNS
};

enum
{
    SERVICE_NAME_COLUMN,
    SERVICE_DESC_COLUMN,
    SERVICE_NUM_COLUMNS
};
