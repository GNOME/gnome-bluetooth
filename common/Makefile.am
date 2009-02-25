
noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = helper.h helper.c \
		bluetooth-instance.h bluetooth-instance.c \
		bluetooth-client.h bluetooth-client.c \
		bluetooth-agent.h bluetooth-agent.c \
		obex-agent.h obex-agent.c

AM_CFLAGS = @DBUS_CFLAGS@ @GTK_CFLAGS@

BUILT_SOURCES = marshal.h marshal.c \
		bluetooth-instance-glue.h \
		bluetooth-client-glue.h \
		bluetooth-agent-glue.h \
		obex-agent-glue.h

nodist_libcommon_a_SOURCES = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

noinst_PROGRAMS = test-client test-agent

test_client_LDADD = libcommon.a @GTK_LIBS@ @DBUS_LIBS@

test_agent_LDADD = libcommon.a @DBUS_LIBS@

EXTRA_DIST = marshal.list \
		bluetooth-instance.xml \
		bluetooth-client.xml \
		bluetooth-agent.xml \
		obex-agent.xml

MAINTAINERCLEANFILES = Makefile.in

marshal.h: marshal.list
	$(GLIB_GENMARSHAL) --prefix=marshal $< --header > $@

marshal.c: marshal.list
	$(GLIB_GENMARSHAL) --prefix=marshal $< --body > $@

bluetooth-instance-glue.h: bluetooth-instance.xml
	$(DBUS_BINDING_TOOL) --prefix=bluetooth_instance --mode=glib-server --output=$@ $<

bluetooth-client-glue.h: bluetooth-client.xml
	$(DBUS_BINDING_TOOL) --prefix=bluetooth_client --mode=glib-client --output=$@ $<

bluetooth-agent-glue.h: bluetooth-agent.xml
	$(DBUS_BINDING_TOOL) --prefix=bluetooth_agent --mode=glib-server --output=$@ $<

obex-agent-glue.h: obex-agent.xml
	$(DBUS_BINDING_TOOL) --prefix=obex_agent --mode=glib-server --output=$@ $<
