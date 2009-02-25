
bin_PROGRAMS = bluetooth-sendto

bluetooth_sendto_SOURCES = main.c

bluetooth_sendto_LDADD = $(top_builddir)/common/libcommon.la $(top_builddir)/common/libgnome-bluetooth.la $(SENDTO_LIBS)

AM_CFLAGS = $(SENDTO_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/common -I$(top_builddir)/common

man_MANS = bluetooth-sendto.1

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in
