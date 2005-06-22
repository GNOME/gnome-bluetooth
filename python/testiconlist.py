import sys

import pygtk
pygtk.require('2.0')
import gtk

# these two lines for testing in the build directory
# when installed, instead just use "from gnomebt import iconlist"
sys.path.append(".libs/")
import iconlist

class HelloWorld:
    def __init__ (self):
        self.window = gtk.Window (gtk.WINDOW_TOPLEVEL)
        self.window.set_default_size (400, 200)
        self.window.connect ("delete_event", self.delete_event)
        self.window.connect ("destroy", self.destroy)

        self.iconlist = iconlist.IconList ()
        self.iconlist.set_selection_mode (gtk.SELECTION_MULTIPLE)
        self.iconlist.connect ("selection_changed", self.selection_changed)
        self.iconlist.connect ("item_added", self.item_added)
        self.iconlist.connect ("item_removed", self.item_removed)
        self.iconlist.set_sort_func (
                lambda a, b, iconlist, userdata = None:
                    cmp (a.get_data(), b.get_data()))
        self.iconlist.set_sorted (True)
        
        self.scrolly = gtk.ScrolledWindow ()
        self.window.add (self.scrolly)
        self.scrolly.set_policy (gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.scrolly.add (self.iconlist)
        self.window.show_all ()

        for i in range(0,15):
            ic = iconlist.IconListItem (
                    gtk.gdk.pixbuf_new_from_file (
                        "../pixmaps/blueradio-48.png"),
                    "Device %d" % i)
            ic.set_data ("User data %d" % i)
            self.iconlist.append_item (ic)

    def delete_event (self, widget, event, data = None):
        return False

    def destroy (self, widget, data = None):
        gtk.main_quit ()

    def selection_changed (self, widget, data = None):
        print "SELECTION CHANGED"
        for i in self.iconlist.get_selected ():
            s = i.get_data ()
            print s,
            s = s + "*"
            i.set_data (s)
        print

    def item_added (self, widget, item, data = None):
        print "ADDED",str(item)

    def item_removed (self, widget, item, data = None):
        print "REMOVED",str(item)

    def main (self):
        gtk.main ()

if __name__ == '__main__':
    HelloWorld ().main ()
