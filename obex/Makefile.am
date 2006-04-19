#
#  $Id$
#

bin_PROGRAMS = gnome-obex-send gnome-obex-server

noinst_PROGRAMS =

gnome_obex_server_SOURCES = \
	gnome-obex-server.c util.c util.h

gnome_obex_send_SOURCES = \
	chooser.c chooser.h \
	gnome-obex-send.c

gnome_obex_server_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) \
	../libegg/libegg/tray/libeggtray.la \
	../src/libgnomebt.la

gnome_obex_send_LDFLAGS = \
	$(GNOME_BLUETOOTH_LIBS) \
	../src/libgnomebt.la

INCLUDES = $(GNOME_BLUETOOTH_CFLAGS) \
	-I$(top_srcdir)/libegg/libegg/tray -I../src \
	-I$(top_srcdir)/src \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\"" \
	-DICON_DIR="\"$(datadir)/pixmaps\""

EXTRA_DIST = gnome-obex-server.desktop.in

desktop_in_files = gnome-obex-server.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
