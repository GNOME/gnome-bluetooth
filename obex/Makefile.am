#
#  $Id: Makefile.am,v 1.11 2006/12/29 21:30:42 hadess Exp $
#

bin_PROGRAMS = gnome-obex-server

noinst_PROGRAMS =

gnome_obex_server_SOURCES = \
	gnome-obex-server.c util.c util.h

gnome_obex_server_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) \
	../src/libgnomebt.la

INCLUDES = $(GNOME_BLUETOOTH_CFLAGS) \
	-I../src \
	-I$(top_srcdir)/src \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATA_DIR="\"$(pkgdatadir)\"" \
	-DICON_DIR="\"$(pkgdatadir)/pixmaps\""

EXTRA_DIST = gnome-obex-server.desktop.in

desktop_in_files = gnome-obex-server.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(desktop_DATA)
