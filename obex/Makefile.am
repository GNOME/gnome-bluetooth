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

gnome_obex_server_LDADD = @OPENOBEX_LIBS@ @BLUETOOTH_LIBS@ \
	$(GNOME_BLUETOOTH_LIBS) \
	../libegg/libegg/tray/libeggtray.la \
	../src/libgnomebt.la

gnome_obex_send_LDFLAGS = @OPENOBEX_LIBS@ @BLUETOOTH_LIBS@ \
	$(GNOME_BLUETOOTH_LIBS) \
	../src/libgnomebt.la

INCLUDES = @GNOME_BLUETOOTH_CFLAGS@ @BLUETOOTH_CFLAGS@ \
	-I ../libegg/libegg/tray -I ../src \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\""

EXTRA_DIST = gnome-obex-server.desktop.in

desktop_in_files = gnome-obex-server.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
