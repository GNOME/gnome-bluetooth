# $Id$

module_flags = -avoid-version -module

INCLUDES = \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)/gnome-bluetooth"\" \
	-D_LARGEFILE64_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	$(VFSMOD_CFLAGS) \
	-I../src

EXTRA_DIST = \
	bluetooth-modules.conf \
	README

modulesconfdir=$(sysconfdir)/gnome-vfs-2.0/modules
modulesconf_DATA = bluetooth-modules.conf

modulesdir = $(libdir)/gnome-vfs-2.0/modules

modules_LTLIBRARIES = \
    libbluetooth.la

libbluetooth_la_SOURCES = \
	bluetooth-method.c \
	bluetooth-method.h

libbluetooth_la_LDFLAGS = $(module_flags)

libbluetooth_la_LIBADD = $(VFSMOD_LIBS) ../src/libBtctl.a
