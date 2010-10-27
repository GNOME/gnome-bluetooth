CLEANFILES =
BUILT_SOURCES = marshal.h marshal.c

# This is where private libraries and typelibs for gnome-shell are installed
# (provided that gnome-bluetooth and gnome-shell are configured with the same
# libdir)
gnomeshelldir = $(libdir)/gnome-shell

gnomeshell_LTLIBRARIES = libgnome-bluetooth-applet.la

libgnome_bluetooth_applet_la_SOURCES = 	\
	bluetooth-applet.c		\
	bluetooth-applet.h		\
	marshal.c

libgnome_bluetooth_applet_la_LIBADD = $(LIBGNOMEBT_LIBS) $(top_builddir)/lib/libcommon.la

libgnome_bluetooth_applet_la_LDFLAGS = -no-undefined

AM_CFLAGS = -I$(srcdir) -I$(top_srcdir)/lib $(LIBGNOMEBT_CFLAGS) $(COMMON_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header > $@

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header --body > $@

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
GnomeBluetoothApplet-1.0.gir: libgnome-bluetooth-applet.la
GnomeBluetoothApplet_1_0_gir_SCANNERFLAGS = --warn-all --symbol-prefix=bluetooth_ --identifier-prefix=Bluetooth
GnomeBluetoothApplet_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 DBusGLib-1.0 GModule-2.0 GnomeBluetooth-1.0
GnomeBluetoothApplet_1_0_gir_PACKAGES = gobject-2.0 gtk+-3.0 dbus-glib-1 gmodule-2.0 glib-2.0
GnomeBluetoothApplet_1_0_gir_CFLAGS = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/lib
GnomeBluetoothApplet_1_0_gir_LIBS = libgnome-bluetooth-applet.la
GnomeBluetoothApplet_1_0_gir_FILES = $(libgnome_bluetooth_applet_la_SOURCES)
INTROSPECTION_GIRS += GnomeBluetoothApplet-1.0.gir

gnomeshell_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gnomeshell_DATA)
endif # HAVE_INTROSPECTION
