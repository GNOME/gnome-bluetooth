#!/usr/bin/python3
#
# Copyright (C) 2022 Bastien Nocera <hadess@hadess.net>
#
# SPDX-License-Identifier: LGPL-2.1-or-later

import gi
from gi.repository import GLib
from gi.repository import Gio
gi.require_version('GnomeBluetooth', '3.0')
from gi.repository import GnomeBluetooth

def notify_cb(client, pspec):
    value = client.get_property(pspec.name)
    print(f'{pspec.name} changed to {value}')

client = GnomeBluetooth.Client.new()
client.connect('notify', notify_cb)

ml = GLib.MainLoop()
ml.run()
