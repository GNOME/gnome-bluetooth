
bin_PROGRAMS = bluetooth-sendto

bluetooth_sendto_SOURCES = main.c

bluetooth_sendto_LDADD = $(top_builddir)/lib/libcommon.la $(top_builddir)/lib/libgnome-bluetooth.la $(SENDTO_LIBS)

AM_CFLAGS = $(SENDTO_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/lib -I$(top_builddir)/lib

man_MANS = bluetooth-sendto.1

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in
