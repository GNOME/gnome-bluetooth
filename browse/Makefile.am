
bin_PROGRAMS = bluetooth-browse

bluetooth_browse_SOURCES = main.c

bluetooth_browse_LDADD = $(top_builddir)/common/libcommon.a \
						@GTK_LIBS@ @DBUS_LIBS@

AM_CFLAGS = @DBUS_CFLAGS@ @GTK_CFLAGS@ $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-browse.1

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in
