/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2002 James Willcox
 * Copyright (C) 2003 Edd Dumbill
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more av.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author:  James Willcox  <jwillcox@gnome.org>
 * Author:  Edd Dumbill  <edd@usefulinc.com>
 */

#include <config.h>

#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gtk/gtk.h>
#include <libbonobo.h>
#include "gnome-bluetooth-component.h"

#include <stdlib.h>


static char *
get_path_from_url (const char *url)
{
	GnomeVFSURI *uri     = NULL;
	char        *escaped = NULL;
	char        *path    = NULL;
	
	uri = gnome_vfs_uri_new (url);

	if (uri != NULL)
		escaped = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);

	if (escaped != NULL)
		path = gnome_vfs_unescape_string (escaped, NULL);
	
	if (uri != NULL)
		gnome_vfs_uri_unref (uri);
	g_free (escaped);
	
	return path;
}


static void
impl_Bonobo_Listener_event (PortableServer_Servant servant,
			    const CORBA_char *event_name,
			    const CORBA_any *args,
			    CORBA_Environment *ev)
{
	GNOMEBluetoothComponent *gbc;
	const CORBA_sequence_CORBA_string *list;
	char    *cmd, *current_dir, *first_path;
	char    *cmd_option = NULL;
	GString *str = NULL;
	int      i;

	gbc = GNOME_BLUETOOTH_COMPONENT (bonobo_object_from_servant (servant));

	if (!CORBA_TypeCode_equivalent (args->_type, TC_CORBA_sequence_CORBA_string, ev)) {
		return;
	}

	list = (CORBA_sequence_CORBA_string *)args->_value;

	g_return_if_fail (gbc != NULL);
	g_return_if_fail (list != NULL);

	/* they could in fact be in different directories, but we will
	 * only look at the first one
	 */

	first_path = get_path_from_url (list->_buffer[0]);
	current_dir = g_path_get_dirname (first_path);


	if (strcmp (event_name, "BTOBEXSend") == 0)  {
		cmd_option = NULL;
        str = g_string_new ("gnome-obex-send");
    }


	g_free (first_path);

    if (cmd_option)
        g_string_append_printf (str, " %s ", cmd_option);

	for (i = 0; i < list->_length; i++) {
		char *path = get_path_from_url (list->_buffer[i]);

		if (path == NULL) 
			continue;
		
		g_string_append_printf (str, " \"%s\"", path);
		g_free (path);
	}

	cmd = g_string_free (str, FALSE);

	g_spawn_command_line_async (cmd, NULL);

	g_free (cmd);
	g_free (cmd_option);
	g_free (current_dir);
}


/* initialize the class */
static void
gnome_bluetooth_component_class_init (GNOMEBluetoothComponentClass *class)
{
	POA_Bonobo_Listener__epv *epv = &class->epv;
	epv->event = impl_Bonobo_Listener_event;
}


static void
gnome_bluetooth_component_init (GNOMEBluetoothComponent *gbc)
{
}


BONOBO_TYPE_FUNC_FULL (GNOMEBluetoothComponent, 
		       Bonobo_Listener, 
		       BONOBO_TYPE_OBJECT,
		       gnome_bluetooth_component);
