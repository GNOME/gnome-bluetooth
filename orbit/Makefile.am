
libdir = $(prefix)/lib/orbit-2.0

AM_CPPFLAGS = \
	-g \
	$(GNOME_BLUETOOTH_CFLAGS)

lib_LTLIBRARIES = GNOME_Bluetooth_Manager_module.la

GNOME_Bluetooth_Manager_module_la_SOURCES = \
	GNOME_Bluetooth_Manager-imodule.c

GNOME_Bluetooth_Manager_module_la_LDFLAGS = \
	-shared -module

GNOME_Bluetooth_Manager-imodule.c: ../src/GNOME_Bluetooth_Manager.idl
	$(ORBIT_IDL) -I`$(PKG_CONFIG) --variable=idldir libbonobo-2.0` \
        -I`$(PKG_CONFIG) --variable=idldir bonobo-activation-2.0` \
		--imodule ../src/GNOME_Bluetooth_Manager.idl
