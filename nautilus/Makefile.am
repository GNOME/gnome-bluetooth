INCLUDES =						\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED			\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(GNOME_BLUETOOTH_CFLAGS)

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libgnome-bluetooth-component.la

libgnome_bluetooth_component_la_SOURCES =			\
	gnome-bluetooth-component.c				\
	gnome-bluetooth-component.h				\
	libmain.c

libgnome_bluetooth_component_la_LIBADD = $(FR_LIBS)
libgnome_bluetooth_component_la_LDFLAGS = -module -avoid-version

server_in_files = GNOME_Bluetooth_Component.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
