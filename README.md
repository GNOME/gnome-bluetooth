# GNOME Bluetooth


gnome-bluetooth is a helper library on top of the bluez daemon's D-Bus API. It used
to contain widgets for application developers but is now home to everything Bluetooth
related for the code GNOME desktop, and nothing pertinent to application developers.

Requirements
------------

- GTK
- bluez 5.51 or newer
- rfkill sub-system enabled in the kernel, and [accessible](https://github.com/systemd/systemd/pull/21605)
- the latest [git version of python-dbusmock](https://github.com/martinpitt/python-dbusmock) to run tests.

Multiple Bluetooth adapters
---------------------------

The gnome-bluetooth user interface and API have no support for handling
multiple Bluetooth adapters. Earlier versions of the bluez backend software
had support for setting a "default adapter" but that is not the case
any more.

Since GNOME 42, the default adapter is the "highest numbered" one, so
removable/external Bluetooth adapters are likely going to be preferred
to internal ones.

As the goal for multiple adapters usually is to disable an internal
Bluetooth adapter in favour of a more featureful removable one, there are
a couple of possibilities to do this, depending on the hardware:

- Disable the internal Bluetooth adapter in the system's BIOS or firmware

- Disable the internal adapter through a mechanical "RF kill" switch
  available on some laptops

- Unplug the USB cable from the wireless card in the case of combo Bluetooth/Wi-Fi
  desktop cards

- Enable the hardware-specific software kill switch on laptops. First find out
  whether your hardware has one:

```sh
rfkill | grep bluetooth | grep -v hci
5 bluetooth hp-bluetooth  unblocked unblocked
```

  Then block it with `rfkill block <ID>` where `<ID>` is the identifier in the
  command above. systemd will remember this across reboots.

- Disable a specific USB adapter through udev by creating a
  `/etc/udev/rules.d/81-bluetooth-hci.rules` device containing:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="0a5c", ATTRS{idProduct}=="21b4", ATTR{authorized}="0"
```

- If the adapter still needs to be plugged in so it can be used as a passthrough,
for virtualisation or gaming, we ship [a small script that makes unbinding the Bluetooth
driver easier](contrib/unbind-bluetooth-driver.sh)

Copyright
---------

A long time ago, gnome-bluetooth was a fork of bluez-gnome,
which was:

`Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>`