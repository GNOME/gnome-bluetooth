AM_CFLAGS =                                     \
        -DDATADIR=\"$(datadir)\"                \
        -DICONDIR=\"$(icondir)\"                \
	-DLOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/lib                     \
	-I$(top_builddir)/lib                   \
	$(SENDTO_CFLAGS)                        \
        $(NST_CFLAGS)                           \
	$(BLUETOOTH_CFLAGS)			\
	$(DBUS_CFLAGS)				\
	$(DISABLE_DEPRECATED)			\
	$(WARN_CFLAGS)

bin_PROGRAMS = bluetooth-sendto

bluetooth_sendto_SOURCES = main.c

bluetooth_sendto_LDADD = $(top_builddir)/lib/libcommon.la $(top_builddir)/lib/libgnome-bluetooth.la $(SENDTO_LIBS)

man_MANS = bluetooth-sendto.1

if HAVE_NST
plugindir = $(libdir)/nautilus-sendto/plugins

plugin_LTLIBRARIES = libnstbluetooth.la

libnstbluetooth_la_SOURCES = nautilus-sendto-plugin.c
libnstbluetooth_la_LDFLAGS = -module -avoid-version
libnstbluetooth_la_LIBADD = $(BLUETOOTH_LIBS) ../lib/libgnome-bluetooth.la

convertdir=$(datadir)/GConf/gsettings
convert_DATA=gnome-bluetooth-nst

gsettingsschema_in_files = org.gnome.Bluetooth.nst.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

endif

EXTRA_DIST = $(man_MANS) gnome-bluetooth-nst org.gnome.Bluetooth.nst.gschema.xml.in nautilus-sendto-plugin.c

MAINTAINERCLEANFILES = Makefile.in
