# gnome-bluetooth-manager
# (C) 2003-4 Edd Dumbill <edd@usefulinc.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import gtk

from gettext import gettext as _

def reportError(parentWindow, primaryText, secondaryText):
    alert = HIGAlert(primaryText, secondaryText,
                     buttons = (gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    if parentWindow != None:
        alert.set_transient_for (parentWindow)
    alert.run()
    alert.hide()

class HIGAlert(gtk.Dialog):
    def __init__(self, primaryText, secondaryText, parent=None, flags=0, buttons=None):
        gtk.Dialog.__init__(self, primaryText, parent, flags, buttons)

        self.set_border_width(6)
        self.set_has_separator(False)
        self.set_resizable(False)

        self.vbox.set_spacing(12)

        hbox = gtk.HBox()
        hbox.set_spacing(12)
        hbox.set_border_width(6)
        
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_DIALOG_ERROR, gtk.ICON_SIZE_DIALOG)
        alignment = gtk.Alignment(yalign=0.0)
        alignment.add(image)
        hbox.pack_start(alignment, expand=False)

        
        vbox = gtk.VBox()

        primaryLabel = gtk.Label("")
        primaryLabel.set_use_markup(True)
        primaryLabel.set_markup('<span weight="bold" size="larger">%s</span>\n' % (primaryText))
        primaryLabel.set_line_wrap(True)
        primaryLabel.set_alignment(0.0, 0.0)
        primaryLabel.set_selectable(True)
        
        secondaryLabel = gtk.Label("")
        secondaryLabel.set_use_markup(True)
        secondaryLabel.set_markup(secondaryText)
        secondaryLabel.set_line_wrap(True)
        secondaryLabel.set_alignment(0.0, 0.0)
        secondaryLabel.set_selectable(True)
        
        vbox.pack_start(primaryLabel)
        vbox.pack_end(secondaryLabel)
        hbox.pack_end(vbox)

        hbox.show_all()

        self.vbox.add(hbox)
        
