#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import os.path

import pygtk
import gtk
import gtk.glade

import gnome
import gnome.ui

import locale, gettext

from gnomebt import iconlist, controller, hig_alert

try:
    from gnomebt import defs
    __version__ = defs.version
    __datadir__ = defs.datadir
except:
    __version__ = "0.0"
    __datadir__ = ".."


APP='gnome-bluetooth'
DIR='../po'
gettext.bindtextdomain (APP, DIR)
gettext.textdomain (APP)
gettext.install (APP, DIR, unicode = 1)


__appname__ = _("Bluetooth Device Manager")


gladefile = "../ui/gnome-bluetooth-manager/gnome-bluetooth-manager.glade"

icon_names = {
  0: "btdevice-misc.png",
  1: "btdevice-computer.png",
  2: "btdevice-phone.png",
  3: "btdevice-lan.png",
  4: "btdevice-audiovideo.png",
  5: "btdevice-peripheral.png",
  6: "btdevice-imaging.png"
}

class BTMenu (gtk.Menu):
    def add (self, item, handler = None):
        if handler is not None:
            item.connect ("activate", handler)
        gtk.Menu.add (self, item)

class BTManager (object):
    def __init__ (self):
        self.btctl = controller.Controller()
        self.setup_gui ()
        self.items_found = 0
        self.names_found = 0
        self.btctl.connect ("device_name", self.on_device_name)
        self.btctl.connect ("status_change", self.on_status_change)
        self.btctl.connect ("add_device", self.on_add_device)

	if self.btctl.is_initialised() == gtk.FALSE:
	    hig_alert.reportError (_("Could not find Bluetooth devices on the system"), _("Please make sure that your bluetooth adapter is correctly plugged in your machine."))
            sys.exit (1)

        self.read_devices ()

    def setup_gui (self):
        self.program = gnome.program_init (
                app_id = __appname__,
                app_version = __version__)
                
        self.window = gnome.ui.App ('gnome-bluetooth', 
                (_("Bluetooth devices")))

        self.window.set_default_size (400, 200)
        self.window.connect ("delete_event", self.delete_event)
        self.window.connect ("destroy", self.destroy)

        vbox = gtk.VBox ()

        self.window.set_contents (vbox)

        menubar = gtk.MenuBar ()

        menuitem = gtk.MenuItem (_("_Devices"))
        menubar.add (menuitem)

        menu = BTMenu ()
        scanitem = gtk.ImageMenuItem (_("_Scan"))
        scanitemimg = gtk.Image ()
        scanitemimg.set_from_stock (gtk.STOCK_REFRESH, gtk.ICON_SIZE_MENU)
        scanitem.set_image (scanitemimg)
        menu.add (scanitem, self.on_scan)
        menu.add (gtk.SeparatorMenuItem ())
        menu.add (gtk.ImageMenuItem (gtk.STOCK_QUIT), self.destroy)

        self.scanitem = scanitem

        menuitem.set_submenu (menu)

        menuitem = gtk.MenuItem (_("_Edit"))
        menubar.add (menuitem)
        menu = BTMenu ()
        menu.add (gtk.ImageMenuItem (gtk.STOCK_DELETE))
        menu.add (gtk.SeparatorMenuItem ())
        menu.add (gtk.ImageMenuItem (gtk.STOCK_PROPERTIES))
        menuitem.set_submenu (menu)

        menuitem = gtk.MenuItem (_("_Help"))
        menubar.add (menuitem)
        menu = BTMenu ()
        menu.add (gtk.ImageMenuItem ('gnome-stock-about'), self.on_about)
        menuitem.set_submenu (menu)
        
        self.statusbar = gnome.ui.AppBar (gtk.TRUE, gtk.TRUE, 
                gnome.ui.PREFERENCES_NEVER)
        self.window.set_statusbar (self.statusbar)
        self.window.set_menus (menubar)
        menubar.show_all ()

        self.iconlist = iconlist.IconList ()
        self.iconlist.set_selection_mode (gtk.SELECTION_SINGLE)
        self.iconlist.connect ("selection_changed", self.selection_changed)
        self.iconlist.set_sort_func (
                lambda a, b, iconlist, userdata = None:
                    cmp (a.get_data(), b.get_data()))
        self.iconlist.set_sorted (gtk.TRUE)

        self.iconlist.drag_dest_set (gtk.DEST_DEFAULT_ALL, 
                [("text/uri-list", 0, 0)],
                gtk.gdk.ACTION_COPY)
        self.iconlist.connect ("drag_data_received", self.drag_data_received)
        
        self.scrolly = gtk.ScrolledWindow ()
        self.scrolly.set_shadow_type (gtk.SHADOW_IN)
        vbox.pack_end (self.scrolly, expand = gtk.TRUE)
        self.scrolly.set_policy (gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.scrolly.add (self.iconlist)

        self.window.show_all ()

        logoname = self.image_file ("btdevice-misc.png")

        self.logo = gtk.gdk.pixbuf_new_from_file (logoname)


    def drag_data_received (self, widget, dc, x, y, selection_data,
            info, t, data = None):
        print "Drag data",widget, dc, x, y, selection_data, info, t, data

    def read_devices (self):
        devs = self.btctl.known_devices ()
        for d in devs:
            ic = iconlist.IconListItem (
                    gtk.gdk.pixbuf_new_from_file (
                        self.icon_name (d['deviceclass'])),
                        d['name'])
            ic.set_data (d['bdaddr'])
            self.iconlist.append_item (ic)

    def delete_event (self, widget, event, data = None):
        return gtk.FALSE

    def destroy (self, widget, data = None):
        gtk.main_quit ()

    def main (self):
        gtk.main ()

    def selection_changed (self, widget, data = None):
        pass

    def icon_name (self, deviceclass = 0):
        cls = (deviceclass >> 8) & 0x1f
        if cls > 6:
            cls = 0
        return self.image_file (icon_names[cls])

    def on_add_device (self, controller, name, data = None):
        self.items_found = self.items_found + 1
        self.statusbar.pop ()
        self.statusbar.push (_("Found device %s.") % (name))
    
    def on_device_name (self, controller, device, name, data = None):
        self.names_found = self.names_found + 1
        self.statusbar.pop ()
        self.statusbar.push (_("Device %s is called '%s'.") % (device, name))
        self.statusbar.set_progress_percentage (0.50 + float (self.names_found) / float (self.items_found) * 0.50)

    def on_status_change (self, controller, status, data = None):
        self.statusbar.pop ()
        if status == 2:
            self.statusbar.push (_("Scanning for devices..."))
            self.items_found = 0
            self.statusbar.set_progress_percentage (0.05)
        elif status == 3:
            self.statusbar.push (_("Retrieving device names..."))
            self.statusbar.set_progress_percentage (0.50)
            self.names_found = 0
        elif status == 4:
            self.statusbar.push (_("Scan complete."))
            self.scanitem.set_sensitive (gtk.TRUE)
            self.statusbar.set_progress_percentage (0.0)
            self.iconlist.clear ()
            self.read_devices ()
        elif status == 1:
            self.statusbar.push (_("Error during scan."))
            self.statusbar.set_progress_percentage (0.0)
            self.scanitem.set_sensitive (gtk.TRUE)

    def on_scan (self, widget, data = None):
        self.scanitem.set_sensitive (gtk.FALSE)
        self.btctl.discover_async ()

    def on_about (self, widget, data = None):
        gnome.ui.About (
                __appname__,
                __version__,
                _(u"Copyright \xc2\xa9 Edd Dumbill 2003"),
                _(u"Manage remote bluetooth devices."),
                [ "Edd Dumbill <edd@usefulinc.com>" ],
                [],
                "",
                self.logo).show ()

    def image_file (self, fname):
        for d in [os.path.join (__datadir__, "pixmaps"), "../pixmaps"]:
            if os.path.isfile (os.path.join (d, fname)):
                return os.path.join (d, fname)
        return None

if __name__ == '__main__':
    BTManager ().main ()

