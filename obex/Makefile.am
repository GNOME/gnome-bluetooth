#
#  $Id$
#

noinst_LIBRARIES = libmisc.a

libmisc_a_SOURCES = \
	obex_io.c obex_io.h \
	obex_put_common.c obex_put_common.h \
	util.c util.h

bin_PROGRAMS = gnome-obex-server gnome-obex-send

noinst_PROGRAMS =

gnome_obex_server_SOURCES = \
	servonly.c obex_test.h \
	obex_test_client.c obex_test_client.h \
	obex_test_server.c obex_test_server.h \
	obexsdp.c obexsdp.h

gnome_obex_send_SOURCES = \
	sender.c obex_test.h \
	obex_test_client.c obex_test_client.h \
	obex_test_server.c obex_test_server.h \
	obexsdp.c obexsdp.h chooser.c chooser.h

gnome_obex_server_LDADD = @OPENOBEX_LIBS@ @BLUETOOTH_LIBS@ @GOBEX_LIBS@ \
	@SDP_LIBS@ $(GNOME_BLUETOOTH_LIBS) \
	libmisc.a ../libegg/libegg/tray/libeggtray.la \
	../src/libgnomebt.la

gnome_obex_send_LDFLAGS = @OPENOBEX_LIBS@ @BLUETOOTH_LIBS@ @GOBEX_LIBS@ \
	@SDP_LIBS@ $(GNOME_BLUETOOTH_LIBS) \
	libmisc.a ../libegg/libegg/tray/libeggtray.la \
	../src/libgnomebt.la

INCLUDES = @OPENOBEX_CFLAGS@ @BLUETOOTH_CFLAGS@ @GOBEX_CFLAGS@ \
	@SDP_CFLAGS@ \
	-I ../libegg/libegg/tray -I ../src \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\""

EXTRA_DIST = gnome-obex-server.desktop.in

desktop_in_files = gnome-obex-server.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
