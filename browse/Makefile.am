
bin_PROGRAMS = bluetooth-browse

bluetooth_browse_SOURCES = main.c

bluetooth_browse_LDADD = $(top_builddir)/common/libcommon.la $(BROWSE_LIBS)

AM_CFLAGS = $(BROWSE_CFLAGS) $(LIBGNOMEBT_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-browse.1

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in
