
ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libbluetooth.la

libbluetooth_la_SOURCES =			\
	cc-bluetooth-panel.c			\
	cc-bluetooth-panel.h			\
	general.h				\
	general.c				\
	adapter.h				\
	adapter.c

libbluetooth_la_LIBADD = $(top_builddir)/lib/libgnome-bluetooth.la $(top_builddir)/lib/libcommon.la $(PROPS_LIBS)
libbluetooth_la_LDFLAGS = -avoid-version -module

AM_CFLAGS = $(PROPS_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

INCLUDES = -I$(top_srcdir)/lib

desktopdir = $(datadir)/applications
desktop_in_in_files = bluetooth-properties.desktop.in.in
desktop_in_files = bluetooth-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

ui_DATA = bluetooth.ui
uidir = $(pkgdatadir)

CLEANFILES = $(desktop_DATA) $(schema_DATA)

EXTRA_DIST = $(man_MANS) $(desktop_in_in_files) $(schema_in_files) $(ui_DATA) $(convert_DATA) $(gsettingsschema_in_files)

MAINTAINERCLEANFILES = Makefile.in


