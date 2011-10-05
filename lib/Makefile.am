SUBDIRS = plugins

EXTRA_DIST = 
CLEANFILES =

noinst_LTLIBRARIES = libcommon.la
lib_LTLIBRARIES = libgnome-bluetooth.la

libcommon_la_SOURCES =						\
		bluetooth-client.h bluetooth-client.c		\
		bluetooth-utils.h bluetooth-utils.c		\
		bluetooth-client-private.h			\
		bluetooth-agent.h bluetooth-agent.c		\
		bluetooth-killswitch.h bluetooth-killswitch.c	\
		obex-agent.h obex-agent.c			\
		bluetooth-plugin-manager.c			\
		bluetooth-plugin-manager.h			\
		bluetooth-enums.h				\
		rfkill.h

libcommon_la_LIBADD = $(COMMON_LIBS)
libcommon_la_LDFLAGS = -no-undefined $(AM_LDFLAGS)

# will be scanned for introspection annotation, but won't be installed
libgnome_bluetooth_c_sources =		\
	bluetooth-client.c		\
	bluetooth-utils.c		\
	bluetooth-agent.c		\
	bluetooth-killswitch.c		\
	marshal.c			\
	gnome-bluetooth-enum-types.c	\
	bluetooth-chooser.c		\
	bluetooth-filter-widget.c	\
	bluetooth-chooser-button.c	\
	bluetooth-chooser-combo.c	\
	bluetooth-client-glue.c

libgnome_bluetooth_private_headers =	\
	bluetooth-client-private.h	\
	bluetooth-agent.h		\
	bluetooth-killswitch.h		\
	marshal.h			\
	gnome-bluetooth-enum-types.h	\
	bluetooth-chooser-private.h	\
	bluetooth-client-glue.h

# public headers don't need to be listed, are handled by _HEADERS
libgnome_bluetooth_la_SOURCES =			\
	$(libgnome_bluetooth_private_headers)	\
	$(libgnome_bluetooth_c_sources)

libgnome_bluetooth_la_LIBADD = $(LIBGNOMEBT_LIBS)

libgnome_bluetooth_la_LDFLAGS =				\
	-version-info $(GNOMEBT_LT_VERSION)		\
	-no-undefined					\
	$(AM_LDFLAGS)					\
	-export-symbols $(srcdir)/gnome-bluetooth.symbols

libgnome_bluetooth_introspect_headers =			\
	bluetooth-client.h				\
	bluetooth-chooser.h				\
	bluetooth-chooser-button.h			\
	bluetooth-chooser-combo.h			\
	bluetooth-filter-widget.h			\
	bluetooth-enums.h				\
	bluetooth-utils.h

gnomebluetoothdir = $(pkgincludedir)
gnomebluetooth_HEADERS =				\
	$(libgnome_bluetooth_introspect_headers)	\
	bluetooth-plugin.h

AM_CFLAGS =						\
	-I$(srcdir)					\
	$(LIBGNOMEBT_CFLAGS)				\
	$(COMMON_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	-DPLUGINDIR=\"$(libdir)/gnome-bluetooth/plugins\" \
	-DG_LOG_DOMAIN=\"Bluetooth\"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_files =				 \
	$(libgnome_bluetooth_introspect_headers) \
	$(libgnome_bluetooth_c_sources)

GnomeBluetooth-1.0.gir: libgnome-bluetooth.la
GnomeBluetooth_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 DBusGLib-1.0 GModule-2.0
GnomeBluetooth_1_0_gir_PACKAGES = gobject-2.0 gtk+-3.0 dbus-glib-1 gmodule-2.0 glib-2.0
GnomeBluetooth_1_0_gir_CFLAGS = -I$(srcdir)
GnomeBluetooth_1_0_gir_LIBS = libgnome-bluetooth.la
GnomeBluetooth_1_0_gir_SCANNERFLAGS = --symbol-prefix=bluetooth_ --identifier-prefix=Bluetooth --pkg-export=gnome-bluetooth-1.0
GnomeBluetooth_1_0_gir_FILES = $(introspection_files)
INTROSPECTION_GIRS += GnomeBluetooth-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif # HAVE_INTROSPECTION

BUILT_SOURCES = marshal.h marshal.c \
		bluetooth-client-glue.h \
		bluetooth-client-glue.c \
		obex-agent-glue.h \
		gnome-bluetooth-enum-types.h \
		gnome-bluetooth-enum-types.c

nodist_libcommon_la_SOURCES = $(BUILT_SOURCES)

CLEANFILES += $(BUILT_SOURCES)

noinst_PROGRAMS = test-client test-agent test-deviceselection test-plugins test-class

test_client_LDADD = libcommon.la $(GTK_LIBS) $(DBUS_LIBS)

test_agent_LDADD = libcommon.la $(DBUS_LIBS)

test_deviceselection_LDADD = libcommon.la libgnome-bluetooth.la

test_plugins_LDADD = libcommon.la

test_class_LDADD = libcommon.la

EXTRA_DIST += marshal.list \
		bluetooth-client.xml \
		obex-agent.xml \
		gnome-bluetooth.symbols

MAINTAINERCLEANFILES = Makefile.in

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header > $@

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=marshal $< --header --body > $@

bluetooth-client-glue.c: bluetooth-client-glue.h

bluetooth-client-glue.h: bluetooth-client.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace= --generate-c-code=bluetooth-client-glue --interface-prefix=org.bluez $<

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
