## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-g \
	-I$(top_srcdir)/intl \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(BLUETOOTH_CFLAGS) \
	$(SDP_CFLAGS) \
    -DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\""

noinst_PROGRAMS=manager-testclient gconftest

bin_PROGRAMS = \
	gnome-bluetooth-admin

libexec_PROGRAMS = \
	gnome-bluetooth-control

## lib_LTLIBRARIES = libgnomebt.la

noinst_LIBRARIES = libBtctl.a 

noinst_LTLIBRARIES = libgnomebt.la

libgnomebt_la_SOURCES = \
	gnomebt-spinner.c gnomebt-icons.c gnomebt-icons.h \
	gnomebt-chooser.c

libgnomebt_la_LDFLAGS = \
	-version-info 0:0:0

gnome_bluetooth_admin_SOURCES = \
	btadmin.c btadmin.h

gnome_bluetooth_admin_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) libBtctl.a

manager_testclient_SOURCES = \
	manager-client.c

manager_testclient_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) libBtctl.a

gconftest_SOURCES = \
	gconftest.c

gconftest_LDFLAGS = \
	$(GNOME_BLUETOOTH_LIBS) $(BLUETOOTH_LIBS) $(SDP_LIBS)

gnome_bluetooth_control_SOURCES = \
	main.c manager.c manager.h manager-impl.c

gnome_bluetooth_control_LDADD = \
	libBtctl.a \
	$(GNOME_BLUETOOTH_LIBS) $(BLUETOOTH_LIBS) $(SDP_LIBS)

BTCTL_CORBA_GENERATED = \
	GNOME_Bluetooth_Manager-common.c \
	GNOME_Bluetooth_Manager-skels.c  \
	GNOME_Bluetooth_Manager-stubs.c  \
	GNOME_Bluetooth_Manager.h

libBtctl_a_SOURCES = $(BTCTL_CORBA_GENERATED)

$(BTCTL_CORBA_GENERATED): GNOME_Bluetooth_Manager.idl
	$(ORBIT_IDL) -I`$(PKG_CONFIG) --variable=idldir libbonobo-2.0` \
		-I`$(PKG_CONFIG) --variable=idldir bonobo-activation-2.0` \
		$(srcdir)/GNOME_Bluetooth_Manager.idl

idldir = $(datadir)/idl
idl_DATA = GNOME_Bluetooth_Manager.idl

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_Bluetooth_Manager.server.in
server_DATA     = $(server_in_files:.server.in=.server)

EXTRA_DIST = $(idl_DATA) GNOME_Bluetooth_Manager.server.in.in \
	spinner.gob chooser.gob


@INTLTOOL_SERVER_RULE@

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXEC\@|$(libexecdir)|" $< > $@


BUILT_SOURCES = $(BTCTL_CORBA_GENERATED) \
	GNOME_Bluetooth_Manager.server.in GNOME_Bluetooth_Manager.server \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h

CLEANFILES = $(BTCTL_CORBA_GENERATED) \
	GNOME_Bluetooth_Manager.server.in GNOME_Bluetooth_Manager.server \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

gnomebt-%.c gnomebt-%.h gnomebt-%-private.h: %.gob
	@GOB2@ $<
