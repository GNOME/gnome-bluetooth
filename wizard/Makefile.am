
bin_PROGRAMS = bluetooth-wizard

bluetooth_wizard_SOURCES = main.c pin.c pin.h

bluetooth_wizard_LDADD = $(top_builddir)/common/libgnome-bluetooth.la $(top_builddir)/common/libcommon.la $(WIZARD_LIBS)

AM_CFLAGS = $(WIZARD_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

pin_DATA = pin-code-database.xml
pindir = $(pkgdatadir)

ui_DATA = wizard.ui
uidir = $(pkgdatadir)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-wizard.1

all: check
	@if test -n $(XMLLINT) ; then \
		xmllint --noout --valid $(srcdir)/pin-code-database.xml ;	\
	fi

EXTRA_DIST = $(man_MANS) $(pin_DATA)

MAINTAINERCLEANFILES = Makefile.in
