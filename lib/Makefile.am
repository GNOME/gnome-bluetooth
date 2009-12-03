SUBDIRS = plugins

EXTRA_DIST = 
BUILT_GIRSOURCES =

noinst_LTLIBRARIES = libcommon.la
lib_LTLIBRARIES = libgnome-bluetooth.la

libcommon_la_SOURCES =						\
		bluetooth-client.h bluetooth-client.c		\
		bluetooth-client-private.h			\
		bluetooth-agent.h bluetooth-agent.c		\
		bluetooth-killswitch.h bluetooth-killswitch.c	\
		obex-agent.h obex-agent.c			\
		bluetooth-plugin-manager.c			\
		bluetooth-plugin-manager.h			\
		bling-spinner.c bling-spinner.h			\
		bluetooth-enums.h				\
		rfkill.h

libcommon_la_LIBADD = $(COMMON_LIBS)
libcommon_la_LDFLAGS = -no-undefined $(AM_LDFLAGS)

libgnome_bluetooth_la_SOURCES =				\
	bluetooth-enums.h				\
	bluetooth-client.h bluetooth-client.c		\
	bluetooth-client-private.h			\
	marshal.h marshal.c				\
	gnome-bluetooth-enum-types.h			\
	gnome-bluetooth-enum-types.c			\
	bling-spinner.c bling-spinner.h			\
	bluetooth-chooser.c bluetooth-chooser.h		\
	bluetooth-chooser-private.h			\
	bluetooth-chooser-button.c bluetooth-chooser-button.h

libgnome_bluetooth_la_LIBADD = $(LIBGNOMEBT_LIBS)

libgnome_bluetooth_la_LDFLAGS =				\
	-version-info $(GNOMEBT_LT_VERSION)		\
	-no-undefined					\
	$(AM_LDFLAGS)					\
	-export-symbols $(srcdir)/gnome-bluetooth.symbols

gnomebluetoothdir = $(pkgincludedir)
gnomebluetooth_HEADERS =				\
	bluetooth-client.h				\
	bluetooth-chooser.h				\
	bluetooth-chooser-button.h			\
	bluetooth-enums.h				\
	bluetooth-plugin.h

AM_CFLAGS = -I$(srcdir) $(LIBGNOMEBT_CFLAGS) $(COMMON_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPLUGINDIR=\"$(libdir)/gnome-bluetooth/plugins\"

if HAVE_INTROSPECTION
BUILT_GIRSOURCES += GnomeBluetooth-1.0.gir

GnomeBluetooth-1.0.gir: $(libgnome_bluetooth_la_SOURCES)
GnomeBluetooth-1.0.gir: $(gnomebluetooth_HEADERS)
GnomeBluetooth-1.0.gir: libgnome-bluetooth.la
GnomeBluetooth-1.0.gir: $(INTROSPECTION_SCANNER)
	$(AM_V_GEN) $(INTROSPECTION_SCANNER) -v --namespace GnomeBluetooth --nsversion=1.0 \
		--add-include-path=$(srcdir) --add-include-path=. \
		--include=GObject-2.0 \
		--include=Gtk-2.0 \
		--include=DBusGLib-1.0 \
		--include=GModule-2.0 \
		--libtool="$(LIBTOOL)" \
		--library=gnome-bluetooth \
		--output $@ \
		--pkg gobject-2.0 \
		--pkg gtk+-2.0 \
		--pkg dbus-glib-1 \
		--pkg gmodule-2.0 \
		-I$(srcdir) \
		$(LIBGNOMEBT_CFLAGS) \
		$(gnomebluetooth_HEADERS)

girdir = $(INTROSPECTION_GIRDIR)
dist_gir_DATA = $(BUILT_GIRSOURCES)

EXTRA_DIST += $(BUILT_GIRSOURCES)

typelibsdir = $(INTROSPECTION_TYPELIBDIR)
typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

%.typelib: %.gir $(G_IR_COMPILER)
	$(AM_V_GEN) LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. $(INTROSPECTION_COMPILER) --includedir=$(srcdir) --includedir=. $(G_IR_COMPILER_OPTS) $< -o $@
	
endif # HAVE_INTROSPECTION

BUILT_SOURCES = marshal.h marshal.c \
		bluetooth-client-glue.h \
		bluetooth-agent-glue.h \
		obex-agent-glue.h \
		gnome-bluetooth-enum-types.h \
		gnome-bluetooth-enum-types.c

nodist_libcommon_la_SOURCES = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

noinst_PROGRAMS = test-client test-agent test-deviceselection test-plugins test-class

test_client_LDADD = libcommon.la $(GTK_LIBS) $(DBUS_LIBS)

test_agent_LDADD = libcommon.la $(DBUS_LIBS)

test_deviceselection_LDADD = libgnome-bluetooth.la

test_plugins_LDADD = libcommon.la

test_class_LDADD = libcommon.la

EXTRA_DIST += marshal.list \
		bluetooth-client.xml \
		bluetooth-agent.xml \
		obex-agent.xml \
		gnome-bluetooth.symbols

MAINTAINERCLEANFILES = Makefile.in

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header > $@

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header --body > $@

bluetooth-client-glue.h: bluetooth-client.xml bluetooth-agent.h bluetooth-agent.c
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --prefix=bluetooth_client --mode=glib-client --output=$@ $<

bluetooth-agent-glue.h: bluetooth-agent.xml bluetooth-agent.h bluetooth-agent.c
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --prefix=bluetooth_agent --mode=glib-server --output=$@ $<

obex-agent-glue.h: obex-agent.xml
	$(AM_V_GEN) $(DBUS_BINDING_TOOL) --prefix=obex_agent --mode=glib-server --output=$@ $<

gnome_bluetooth_headers_to_scan_for_enums = bluetooth-enums.h
# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
gnome_bluetooth_built_headers = gnome-bluetooth-enum-types.h
gnome_bluetooth_built_cfiles = gnome-bluetooth-enum-types.c

gnome-bluetooth-enum-types.h: $(gnome_bluetooth_headers_to_scan_for_enums) Makefile
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GNOME_BLUETOOTH_ENUM_TYPES_H__\n#define __GNOME_BLUETOOTH_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define BLUETOOTH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __GNOME_BLUETOOTH_ENUM_TYPES_H__ */" \
		$(gnome_bluetooth_headers_to_scan_for_enums)) > $@

gnome-bluetooth-enum-types.c: $(gnome_bluetooth_headers_to_scan_for_enums) Makefile gnome-bluetooth-enum-types.h
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
			--fhead "#include <bluetooth-enums.h>\n" \
			--fhead "#include \"gnome-bluetooth-enum-types.h\"\n" \
			--fhead "#include <glib-object.h>" \
		      	--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gnome_bluetooth_headers_to_scan_for_enums)) > $@
