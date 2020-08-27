# GNOME Bluetooth


gnome-bluetooth is collection of widgets for applications that want
to select Bluetooth devices. It is also used by GNOME session
components such as the Settings and gnome-shell.

Requirements
------------

- glib
- bluez 5.x
- rfkill sub-system enable in the kernel[1]
- gnome-settings-daemon

[1]: Note that read/write access to the /dev/rfkill device is
required and should be provided by the distribution

Copyright
---------

A long time ago, gnome-bluetooth was a fork of bluez-gnome,
which was:

`Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>`