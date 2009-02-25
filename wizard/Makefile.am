
bin_PROGRAMS = bluetooth-wizard

bluetooth_wizard_SOURCES = main.c

bluetooth_wizard_LDADD = $(top_builddir)/common/libcommon.la \
						@GTK_LIBS@ @DBUS_LIBS@

AM_CFLAGS = @DBUS_CFLAGS@ @GTK_CFLAGS@ $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

pin_DATA = pin-code-database.txt
pindir = $(pkgdatadir)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-wizard.1

EXTRA_DIST = $(man_MANS) $(pin_DATA)

MAINTAINERCLEANFILES = Makefile.in
