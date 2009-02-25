
bin_PROGRAMS = bluetooth-wizard

bluetooth_wizard_SOURCES = main.c

bluetooth_wizard_LDADD = $(top_builddir)/common/libcommon.a \
						@GTK_LIBS@ @DBUS_LIBS@

AM_CFLAGS = @DBUS_CFLAGS@ @GTK_CFLAGS@ $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-wizard.1

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in
