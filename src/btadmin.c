#include <assert.h>
#include <stdio.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <glib.h>
#include "config.h"

#include <libbonobo.h>
#include <unistd.h>

#include "GNOME_Bluetooth_Manager.h"

#include "btadmin.h"
#include "bthelper.h"

#define GLADE_FILE DATA_DIR "/btdeview.glade"

static void populate_device_details(BTAdmin *app);
static void populate_device_list(BTAdmin *app);

void on_device_treeview_selection_changed(GtkTreeSelection *sel,
										  gpointer data);
void on_scanbutton_clicked(GtkButton *but, gpointer data);

// handler
void
on_device_treeview_selection_changed(GtkTreeSelection *sel, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BTAdmin *app=(BTAdmin*)data;

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		guint i;
		gtk_tree_model_get(model, &iter,
			DEV_NUM_COLUMN, &i,
			-1);
		app->cur_device=i;
		populate_device_details(app);
	}
	// TODO: else clear the dependent widgets
}

void
on_scanbutton_clicked(GtkButton *but, gpointer data)
{
  BTAdmin *app=(BTAdmin*)data;

  GNOME_Bluetooth_Manager_discoverDevices(app->btmanager, &app->ev);
  populate_device_list(app);
  app->cur_device=0;
  populate_device_details(app);

}

static void
populate_device_list(BTAdmin *app)
{
	guint i;
	GtkTreeIter iter;

	GNOME_Bluetooth_Manager_knownDevices(app->btmanager,
			&(app->list), &(app->ev));

	gtk_list_store_clear(app->devstore);

	for(i=0; i<app->list->_length; i++) {
		gtk_list_store_append(app->devstore, &iter);
		gtk_list_store_set(app->devstore, &iter,
				DEV_NAME_COLUMN, app->list->_buffer[i].name,
				DEV_NUM_COLUMN, i,
				-1);
	}
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(app->devstore),
			DEV_NAME_COLUMN, GTK_SORT_ASCENDING);
}

/*
 * Fill in details for the active device.
 */
static void
populate_device_details(BTAdmin *app)
{
	GtkTreeIter iter;
	GNOME_Bluetooth_ServiceList *slist;
	guint i;

	if (app->list->_length>0) {
		gtk_list_store_clear(app->devinfstore);

		gtk_list_store_append(app->devinfstore, &iter);
		gtk_list_store_set(app->devinfstore, &iter,
				DEVINF_NAME_COLUMN, _("Name"),
				DEVINF_DESC_COLUMN, app->list->_buffer[app->cur_device].name,
				-1);

		gtk_list_store_append(app->devinfstore, &iter);
		gtk_list_store_set(app->devinfstore, &iter,
				DEVINF_NAME_COLUMN, _("Address"),
				DEVINF_DESC_COLUMN, app->list->_buffer[app->cur_device].bdaddr,
				-1);

		/* now get the services for each device */

		gtk_list_store_clear(app->servicestore);
		GNOME_Bluetooth_Manager_servicesForDevice(app->btmanager,
												  &slist,
												  app->list->_buffer[app->cur_device].bdaddr,
												  &app->ev);
		for(i=0; i<slist->_length; i++) {
		  gchar *sname,*sdesc;
		  sname=g_strdup_printf("0x%x", slist->_buffer[i].service);
		  sdesc=bthelper_svc_id_to_class(slist->_buffer[i].service);
		  gtk_list_store_append(app->servicestore, &iter);
		  gtk_list_store_set(app->servicestore, &iter,
							 SERVICE_NAME_COLUMN, sname,
							 SERVICE_DESC_COLUMN, sdesc,
							 -1);
		  g_free(sname);
		  g_free(sdesc);
		}
		CORBA_free(slist);
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

static void main2(GladeXML *xml, gchar *gladefile) {
	GtkTreeSelection *select;
	GtkButton *but;
    BTAdmin *app=g_new0(BTAdmin, 1);

    app->client=gconf_client_get_default();
    app->ui=xml;
    app->btmanager=bonobo_get_object("OAFIID:GNOME_Bluetooth_Manager",
					  "GNOME/Bluetooth/Manager", NULL);
    if (app->btmanager==CORBA_OBJECT_NIL) {
	g_warning("Couldn't get instance of BT Manager");
	bonobo_debug_shutdown();
	return;
    }

    CORBA_exception_init(&(app->ev));
    app->list=NULL;

    app->devstore=gtk_list_store_new(DEV_NUM_COLUMNS, G_TYPE_STRING,
			G_TYPE_UINT);
    app->devinfstore=gtk_list_store_new(DEVINF_NUM_COLUMNS, G_TYPE_STRING,
	    G_TYPE_STRING);
    app->servicestore=gtk_list_store_new(SERVICE_NUM_COLUMNS, G_TYPE_STRING,
	    G_TYPE_STRING);

    app->devtreeview=(GtkTreeView*)glade_xml_get_widget(xml,
	    "device_treeview");
    app->devinftreeview=(GtkTreeView*)glade_xml_get_widget(xml,
	    "details_treeview");
    app->servicetreeview=(GtkTreeView*)glade_xml_get_widget(xml,
	    "service_treeview");

	// TODO : remove leak
    gtk_tree_view_set_model(app->devtreeview,
	    gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(app->devstore)));

    gtk_tree_view_set_model(app->devinftreeview,
	    GTK_TREE_MODEL(app->devinfstore));
    gtk_tree_view_set_model(app->servicetreeview,
	    GTK_TREE_MODEL(app->servicestore));

    /* setup cell renderers for each column
     */

	treeview_add_text_renderer(GTK_TREE_VIEW(app->devtreeview),
			DEV_NAME_COLUMN, "Device name");

	treeview_add_text_renderer(GTK_TREE_VIEW(app->devinftreeview),
			DEVINF_NAME_COLUMN, "Attribute");
	treeview_add_text_renderer(GTK_TREE_VIEW(app->devinftreeview),
			DEVINF_DESC_COLUMN, "Value");

	treeview_add_text_renderer(GTK_TREE_VIEW(app->servicetreeview),
			SERVICE_NAME_COLUMN, "Service");
	treeview_add_text_renderer(GTK_TREE_VIEW(app->servicetreeview),
			SERVICE_DESC_COLUMN, "Description");

	/*
	 * connect treeview selection signal handler
	 */
	select = gtk_tree_view_get_selection(app->devtreeview);
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
			G_CALLBACK(on_device_treeview_selection_changed),
			(gpointer)app);

	select = gtk_tree_view_get_selection(app->devinftreeview);
	gtk_tree_selection_set_mode(select, GTK_SELECTION_NONE);

	/*
	 * connect scan button signal handler
	 */
	but = (GtkButton*)glade_xml_get_widget(xml, "scanbutton");
	g_signal_connect(G_OBJECT(but), "clicked",
					 G_CALLBACK(on_scanbutton_clicked),
					 (gpointer)app);

	app->cur_device=0;
	populate_device_list(app);
	populate_device_details(app);

    gtk_main ();

    CORBA_free(app->list);
    CORBA_exception_free(&(app->ev));
    bonobo_object_release_unref(app->btmanager, NULL);
    bonobo_debug_shutdown();
    g_object_unref(G_OBJECT(app->devstore));
    g_object_unref(G_OBJECT(app->devinfstore));
    g_object_unref(G_OBJECT(app->servicestore));
    g_object_unref(app->client);
    g_free(app);
}

static void cannot_find_ui(void) {
	GtkWidget *dialog;
	dialog=gnome_error_dialog(_("Failed to find user interface."));
	gtk_signal_connect_object (GTK_OBJECT (dialog),
			"close", GTK_SIGNAL_FUNC (gtk_main_quit),
			(gpointer) dialog);
	gtk_main();
}

int main( int   argc,
	  char *argv[] )
{
	GladeXML *xml;
    gchar *fname=NULL;;

	gnome_init(PACKAGE, VERSION, argc, argv);
	glade_gnome_init();
    if (!bonobo_init(&argc, argv)) {
	g_error("Couldn't initialize bonobo");
	exit(1);
    }
	if (g_file_exists(GLADE_FILE)) {
	fname=GLADE_FILE;
    } else if (g_file_exists("../ui/btdeview.glade")) {
	fname="../ui/btdeview.glade";
    }
    if (fname) {
		xml = glade_xml_new(fname, NULL, NULL);
		glade_xml_signal_autoconnect(xml);
	bonobo_activate();
		main2(xml, fname);
    } else {
		cannot_find_ui();
		exit(1);
	}
    return(0);
}
