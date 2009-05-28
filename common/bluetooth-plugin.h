#ifndef _GNOME_BLUETOOTH_PLUGIN_H_
#define _GNOME_BLUETOOTH_PLUGIN_H_

#include <gmodule.h>
#include <gtk/gtk.h>

typedef struct _GbtPluginInfo GbtPluginInfo;
typedef struct _GbtPlugin GbtPlugin;

struct _GbtPluginInfo 
{
	const char *id;
	gboolean (* has_config_widget) (const char *bdaddr, const char **uuids);
	GtkWidget * (* get_config_widgets) (const char *bdaddr, const char **uuids);
};

struct _GbtPlugin
{
	GModule *module;
	GbtPluginInfo *info;
};

#define GBT_INIT_PLUGIN(plugininfo)					\
	gboolean gbt_init_plugin (GbtPlugin *plugin);			\
	G_MODULE_EXPORT gboolean					\
	gbt_init_plugin (GbtPlugin *plugin) {				\
		plugin->info = &(plugininfo);				\
		return TRUE;						\
	}

#define SOEXT           ("." G_MODULE_SUFFIX)
#define SOEXT_LEN       (strlen (SOEXT))

#endif /* _GNOME_BLUETOOTH_PLUGIN_H_ */

