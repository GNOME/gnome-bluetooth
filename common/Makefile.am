
noinst_LTLIBRARIES = libcommon.la
lib_LTLIBRARIES = libgnome-bluetooth.la

libcommon_la_SOURCES = helper.h helper.c \
		bluetooth-client.h bluetooth-client.c \
		bluetooth-agent.h bluetooth-agent.c \
		bluetooth-killswitch.h bluetooth-killswitch.c \
		obex-agent.h obex-agent.c \
		helper.c helper.h \
		bluetooth-enums.h
libcommon_la_LIBADD = $(COMMON_LIBS)
libcommon_la_LDFLAGS = -no-undefined $(AM_LDFLAGS)

libgnome_bluetooth_la_SOURCES =				\
	bluetooth-enums.h				\
	bluetooth-client.h bluetooth-client.c		\
	marshal.h marshal.c				\
	bluetooth-chooser.c bluetooth-chooser.h		\
	bluetooth-chooser-button.c bluetooth-chooser-button.h

libgnome_bluetooth_la_LIBADD = $(LIBGNOMEBT_LIBS)

libgnome_bluetooth_la_LDFLAGS =				\
	-version-info $(GNOMEBT_LT_VERSION)		\
	-no-undefined					\
	$(AM_LDFLAGS)

# FIXME
#	-export-symbols $(srcdir)/gnome-bluetooth.symbols

gnomebluetoothdir = $(pkgincludedir)
gnomebluetooth_HEADERS =				\
	bluetooth-chooser.h				\
	bluetooth-chooser-button.h			\
	bluetooth-enums.h

AM_CFLAGS = -I$(srcdir) $(LIBGNOMEBT_CFLAGS) $(COMMON_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

BUILT_SOURCES = marshal.h marshal.c \
		bluetooth-client-glue.h \
		bluetooth-agent-glue.h \
		obex-agent-glue.h

nodist_libcommon_la_SOURCES = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

noinst_PROGRAMS = test-client test-agent test-deviceselection

test_client_LDADD = libcommon.la $(GTK_LIBS) $(DBUS_LIBS)

test_agent_LDADD = libcommon.la $(DBUS_LIBS)

test_deviceselection_LDADD = libgnome-bluetooth.la

EXTRA_DIST = marshal.list \
		bluetooth-client.xml \
		bluetooth-agent.xml \
		obex-agent.xml

MAINTAINERCLEANFILES = Makefile.in

marshal.h: marshal.list
	$(GLIB_GENMARSHAL) --prefix=marshal $< --header > $@

marshal.c: marshal.list
	$(GLIB_GENMARSHAL) --prefix=marshal $< --header --body > $@

bluetooth-client-glue.h: bluetooth-client.xml
	$(DBUS_BINDING_TOOL) --prefix=bluetooth_client --mode=glib-client --output=$@ $<

bluetooth-agent-glue.h: bluetooth-agent.xml
	$(DBUS_BINDING_TOOL) --prefix=bluetooth_agent --mode=glib-server --output=$@ $<

obex-agent-glue.h: obex-agent.xml
	$(DBUS_BINDING_TOOL) --prefix=obex_agent --mode=glib-server --output=$@ $<
