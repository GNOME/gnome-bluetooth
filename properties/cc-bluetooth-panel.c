/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2010  Bastien Nocera <hadess@hadess.net>
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

#include <glib/gi18n-lib.h>
#include <libgnome-control-center/cc-shell.h>

#include "cc-bluetooth-panel.h"

#include <bluetooth-client.h>
#include <bluetooth-client-private.h>
#include <bluetooth-killswitch.h>
#include <bluetooth-chooser.h>
#include <bluetooth-chooser-private.h>
#include <bluetooth-plugin-manager.h>

G_DEFINE_DYNAMIC_TYPE (CcBluetoothPanel, cc_bluetooth_panel, CC_TYPE_PANEL)

#define BLUETOOTH_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct CcBluetoothPanelPrivate {
	GtkBuilder          *builder;
	GtkWidget           *chooser;
	BluetoothClient     *client;
	BluetoothKillswitch *killswitch;
};

static void cc_bluetooth_panel_finalize (GObject *object);

static void
cc_bluetooth_panel_class_init (CcBluetoothPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cc_bluetooth_panel_finalize;

	g_type_class_add_private (klass, sizeof (CcBluetoothPanelPrivate));
}

static void
cc_bluetooth_panel_class_finalize (CcBluetoothPanelClass *klass)
{
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	CcBluetoothPanel *self;

	bluetooth_plugin_manager_cleanup ();

	self = CC_BLUETOOTH_PANEL (object);
	if (self->priv->builder) {
		g_object_unref (self->priv->builder);
		self->priv->builder = NULL;
	}
	if (self->priv->killswitch) {
		g_object_unref (self->priv->killswitch);
		self->priv->killswitch = NULL;
	}
	if (self->priv->client) {
		g_object_unref (self->priv->client);
		self->priv->client = NULL;
	}

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	GtkWidget *widget;
	GError *error = NULL;
	GtkTreeViewColumn *column;
	GtkStyleContext *context;

	self->priv = BLUETOOTH_PANEL_PRIVATE (self);

	bluetooth_plugin_manager_init ();
	self->priv->killswitch = bluetooth_killswitch_new ();
	self->priv->client = bluetooth_client_new ();

	self->priv->builder = gtk_builder_new ();
	gtk_builder_add_from_file (self->priv->builder,
				   PKGDATADIR "/bluetooth.ui",
				   &error);
	if (error != NULL) {
		g_warning ("Failed to load '%s': %s", PKGDATADIR "/bluetooth.ui", error->message);
		g_error_free (error);
		return;
	}

	widget = WID ("vbox");
	gtk_widget_reparent (widget, GTK_WIDGET (self));

	/* The known devices */
	widget = WID ("devices_table");

	/* Note that this will only ever show the devices on the default
	 * adapter, this is on purpose */
	self->priv->chooser = bluetooth_chooser_new (NULL);
	gtk_table_attach (GTK_TABLE (widget), self->priv->chooser, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	g_object_set (self->priv->chooser,
		      "show-searching", FALSE,
		      "show-device-type", FALSE,
		      "show-device-category", FALSE,
		      "show-pairing", FALSE,
		      "show-connected", TRUE,
		      "device-category-filter", BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED,
		      NULL);
	column = bluetooth_chooser_get_type_column (BLUETOOTH_CHOOSER (self->priv->chooser));
	gtk_tree_view_column_set_visible (column, FALSE);
	widget = bluetooth_chooser_get_treeview (BLUETOOTH_CHOOSER (self->priv->chooser));
	g_object_set (G_OBJECT (widget), "headers-visible", FALSE, NULL);

	/* Join treeview and buttons */
	widget = bluetooth_chooser_get_scrolled_window (BLUETOOTH_CHOOSER (self->priv->chooser));;
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (widget), 250);
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
	widget = WID ("toolbar1");
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

	gtk_widget_show_all (GTK_WIDGET (self));
}

void
cc_bluetooth_panel_register (GIOModule *module)
{
	cc_bluetooth_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_BLUETOOTH_PANEL,
					"bluetooth", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_bluetooth_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

