/*
  Copyright (c) 2003 Edd Dumbill.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome.h>

#include "gnomebt-controller.h"
#include "chooser.h"

#define GLADE_FILE DATA_DIR "/gnome-obex-send.glade"


typedef struct _BTChooser {
    GnomebtController *btmanager;
    GSList *list;
    GtkListStore *devstore;
    GtkTreeView *devtreeview;
    gint cur_device;
    GladeXML *xml;
    gint response;
} BTChooser;

enum 
{
    DEV_NAME_COLUMN,
	DEV_NUM_COLUMN,
    DEV_NUM_COLUMNS
};

static void
populate_device_list(BTChooser *app)
{
	guint i=0;
	GtkTreeIter iter;
    GSList *item;

    if (app->list)
        gnomebt_device_desc_list_free (app->list);
    app->list = gnomebt_controller_known_devices (app->btmanager);

	gtk_list_store_clear(app->devstore);

    for(item=app->list, i=0; item!=NULL; item=g_slist_next(item), i++) {
        GnomebtDeviceDesc *dd=(GnomebtDeviceDesc*)item->data;
		gtk_list_store_append(app->devstore, &iter);
		gtk_list_store_set(app->devstore, &iter,
				DEV_NAME_COLUMN, dd->name,
				DEV_NUM_COLUMN, i,
				-1);
	}
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(app->devstore),
			DEV_NAME_COLUMN, GTK_SORT_ASCENDING);
}

static void
response_cb (GtkDialog *d, gint arg, gpointer userdata)
{
    BTChooser *m = (BTChooser *)userdata;
    switch (arg) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_DELETE_EVENT:
            m->response = arg;
            gtk_widget_destroy (GTK_WIDGET (d));
            gtk_main_quit ();
            break;
        case GTK_RESPONSE_HELP:
            /* TODO -- do help*/
            break;
    }
}

static void treeview_add_text_renderer(
		GtkTreeView *tv,
		int colno,
		gchar *label)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				label,
				renderer,
				"text",
				colno,
				NULL);
	gtk_tree_view_append_column(tv, column);
}

static void
treeview_cb (GtkTreeSelection *sel, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BTChooser *app=(BTChooser *)data;

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		guint i;
		gtk_tree_model_get(model, &iter,
			DEV_NUM_COLUMN, &i,
			-1);
        
        app->cur_device = i;
        if (i >= 0) {
            GtkWidget *button = glade_xml_get_widget (app->xml, "okbutton1");
            gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
        }
	}
}

gchar *
choose_bdaddr ()
{
    GtkWidget *dialog, *button;
    gchar *fname = NULL;
    GladeXML *xml;
    gchar *ret = NULL;
    BTChooser *app;
    GtkTreeSelection *select;

    if (g_file_exists("../ui/gnome-obex-send.glade")) {
	  fname="../ui/gnome-obex-send.glade";
    } else if (g_file_exists(GLADE_FILE)) {
	  fname=GLADE_FILE;
    }

    if (fname) {
        xml = glade_xml_new(fname, NULL, NULL);
        glade_xml_signal_autoconnect(xml);
    } else {
        g_error("Can't find user interface file");
        return NULL;
	}

    dialog = glade_xml_get_widget (xml, "choose_dialog");
    button = glade_xml_get_widget (xml, "okbutton1");
    gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

    app = g_new0 (BTChooser, 1);

    app->xml = xml;
    app->btmanager=gnomebt_controller_new();

    app->list = NULL;
    app->cur_device = -1;
    app->devstore=gtk_list_store_new(DEV_NUM_COLUMNS, G_TYPE_STRING,
			G_TYPE_UINT);
    app->devtreeview = (GtkTreeView *) glade_xml_get_widget (xml, "devicetree");

    gtk_tree_view_set_model(app->devtreeview,
	    gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(app->devstore)));
    
	treeview_add_text_renderer(GTK_TREE_VIEW(app->devtreeview),
			DEV_NAME_COLUMN, "Device name");

  	select = gtk_tree_view_get_selection(app->devtreeview);
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
			G_CALLBACK(treeview_cb),
			(gpointer)app);

    populate_device_list (app);

    g_signal_connect (GTK_OBJECT (dialog), 
                             "response", 
                             G_CALLBACK (response_cb),
                             (gpointer) app);

    g_message("showing dialog");
    
    gtk_widget_show_all (dialog);
    gtk_main ();

    if (app->response == GTK_RESPONSE_OK && 
            app->cur_device >= 0) {
        GnomebtDeviceDesc *dd=(GnomebtDeviceDesc*)
            g_slist_nth(app->list, app->cur_device)->data;
        ret = g_strdup (dd->bdaddr);
    }

    if (app->list)
        gnomebt_device_desc_list_free (app->list);
    g_object_unref(app->btmanager);
    g_object_unref(G_OBJECT(app->devstore));
 
    g_free (app);

    return ret;
}
