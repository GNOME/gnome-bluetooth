#ifndef MANAGER_H_
#define MANAGER_H_

#include <glib.h>

#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-object.h>

#include <gconf/gconf-client.h>

#include "GNOME_Bluetooth_Manager.h"
#include "btctl.h"

G_BEGIN_DECLS

#define BTMANAGER_TYPE   (btmanager_get_type())
#define BTMANAGER(o)     G_TYPE_CHECK_INSTANCE_CAST (o, btmanager_get_type (), BTManager)
#define BTMANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, btmanager_get_type (), BTManagerClass)
#define BTMANAGER_IS_OBJECT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, btmanager_get_type ())
#define BTMANAGER_IS_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASSE ((obj), BTMANAGER_TYPE, BTManagerClass))

typedef struct {
  BonoboObject parent;
  GConfClient  *client;
  BtctlController        *bc;
} BTManager;

typedef struct {
  BonoboObjectClass parent_class;

  POA_GNOME_Bluetooth_Manager__epv epv;
} BTManagerClass;

typedef struct {
  gchar *name;
  gchar *bdaddr;
  guint deviceclass;
} BTDeviceDesc;

typedef struct {
	guint id;
	GSList *channels;
} BTServiceDesc;

/* this define tests the class long against the
   enums from the idl
*/
#define IS_MAJOR_CLASS(t, cls) (((t>>8)&0x1f)==cls)

GType btmanager_get_type(void);
BTManager *btmanager_new(void);

typedef void (*BTDeviceCallback)(BTManager *m, gchar *bdaddr, gpointer data);
typedef int (*BTDeviceFilter)(BTManager *m, gchar *bdaddr, gpointer data);

void btmanager_impl_for_each_known_device(BTManager *mgr,
										  BTDeviceCallback cb,
										  gpointer data);

void btmanager_impl_for_each_known_device_filtered(BTManager *mgr,
												   BTDeviceCallback cb,
												   BTDeviceFilter filt,
												   gpointer data,
												   gpointer filterdata);
void btmanager_impl_init(BTManager *mgr);
void btmanager_impl_shutdown(BTManager *mgr);

void btmanager_impl_remove_all_devices(BTManager *mgr);
void btmanager_impl_discover_devices(BTManager *mgr);

guint btmanager_impl_num_known_devices(BTManager *mgr);
guint btmanager_impl_num_known_devices_filtered(BTManager *mgr,
												BTDeviceFilter filt,
												gpointer data);

BTDeviceDesc *btmanager_impl_new_device_desc(BTManager *mgr,
											 gchar     *bdaddr);
void btmanager_impl_free_device_desc(BTManager *mgr, BTDeviceDesc *dd);

gint btmanager_impl_get_rfcomm_port(BTManager *mgr, gchar* bdaddr, guint chan);
gint btmanager_impl_connect_rfcomm_port(BTManager *mgr, gchar* bdaddr,
										 guint chan);

GSList * btmanager_impl_channels_for_service(BTManager *mgr, const gchar *bdaddr,
											 guint svcid);

void btmanager_free_service_list(GSList *list);
GSList * btmanager_impl_services_for_device(BTManager *mgr, const gchar *bdaddr);


gchar * btmanager_impl_get_device_name(BTManager *mgr, const gchar *bdaddr);
guint btmanager_impl_get_device_class(BTManager *mgr, const gchar *bdaddr);

G_END_DECLS

#endif /* MANAGER_H_ */
