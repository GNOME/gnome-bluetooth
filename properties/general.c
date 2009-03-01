/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <bluetooth-client.h>

#include "general.h"

typedef enum {
	ICON_POLICY_NEVER,
	ICON_POLICY_ALWAYS,
	ICON_POLICY_PRESENT,
} EnumIconPolicy;

static int icon_policy = ICON_POLICY_PRESENT;

#define PREF_DIR		"/apps/bluetooth-manager"
#define PREF_ICON_POLICY	PREF_DIR "/icon_policy"

static GConfEnumStringPair icon_policy_enum_map [] = {
	{ ICON_POLICY_NEVER,	"never"		},
	{ ICON_POLICY_ALWAYS,	"always"	},
	{ ICON_POLICY_PRESENT,	"present"	},
	{ ICON_POLICY_PRESENT,	NULL		},
};

static GConfClient* gconf;

static GtkWidget *button_never;
static GtkWidget *button_always;
static GtkWidget *button_present;

static void update_icon_policy(GtkWidget *button)
{
	GtkWidget *widget;
	const char *str;

	if (button == NULL) {
		switch (icon_policy) {
		case ICON_POLICY_NEVER:
			widget = button_never;
			break;
		case ICON_POLICY_ALWAYS:
			widget = button_always;
			break;
		case ICON_POLICY_PRESENT:
		default:
			widget = button_present;
			break;
		}

		if (!widget)
			return;

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

		return;
	}

	if (button == button_always)
		icon_policy = ICON_POLICY_ALWAYS;
	else if (button == button_never)
		icon_policy = ICON_POLICY_NEVER;
	else if (button == button_present)
		icon_policy = ICON_POLICY_PRESENT;

	str = gconf_enum_to_string(icon_policy_enum_map, icon_policy);

	gconf_client_set_string(gconf, PREF_ICON_POLICY, str, NULL);
}

static void policy_callback(GtkWidget *button, gpointer user_data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE)
		update_icon_policy(button);
}

GtkWidget *create_label(const gchar *str)
{
	GtkWidget *label;
	gchar *tmp;

	label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

	tmp = g_strdup_printf("<b>%s</b>", str);
	gtk_label_set_markup(GTK_LABEL(label), tmp);
	g_free(tmp);

	return label;
}

GtkWidget *create_general(void)
{
	GtkWidget *mainbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GSList *group = NULL;

	mainbox = gtk_vbox_new(FALSE, 24);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 12);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, FALSE, 0);

	label = create_label(_("Notification area"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	button_never = gtk_radio_button_new_with_label(group,
						_("Never display icon"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button_never));
	gtk_box_pack_start(GTK_BOX(vbox), button_never, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button_never), "toggled",
					G_CALLBACK(policy_callback), NULL);

	button_present = gtk_radio_button_new_with_label(group,
				_("Only display when adapter present"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button_present));
	gtk_box_pack_start(GTK_BOX(vbox), button_present, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button_present), "toggled",
					G_CALLBACK(policy_callback), NULL);

	button_always = gtk_radio_button_new_with_label(group,
						_("Always display icon"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button_always));
	gtk_box_pack_start(GTK_BOX(vbox), button_always, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button_always), "toggled",
					G_CALLBACK(policy_callback), NULL);

	update_icon_policy(NULL);

	return mainbox;
}

static void gconf_callback(GConfClient *client, guint cnxn_id,
					GConfEntry *entry, gpointer user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value(entry);
	if (value == NULL)
		return;

	if (g_str_equal(entry->key, PREF_ICON_POLICY) == TRUE) {
		const char *str;

		str = gconf_value_get_string(value);
		if (str)
			gconf_string_to_enum(icon_policy_enum_map,
							str, &icon_policy);

		update_icon_policy(NULL);
	}
}

void setup_general(void)
{
	char *str;

	gconf = gconf_client_get_default();

	str = gconf_client_get_string(gconf, PREF_ICON_POLICY, NULL);
	if (str) {
		gconf_string_to_enum(icon_policy_enum_map, str, &icon_policy);
		g_free(str);
	}

	gconf_client_add_dir(gconf, PREF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);

	gconf_client_notify_add(gconf, PREF_DIR,
					gconf_callback, NULL, NULL, NULL);
}

void cleanup_general(void)
{
	g_object_unref(gconf);
}
