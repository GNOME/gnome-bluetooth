
bin_PROGRAMS = bluetooth-properties

bluetooth_properties_SOURCES = main.c general.h general.c adapter.h adapter.c

bluetooth_properties_LDADD = $(top_builddir)/common/libcommon.la $(PROPS_LIBS)

AM_CFLAGS = $(PROPS_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-properties.1

desktopdir = $(datadir)/applications

desktop_in_in_files = bluetooth-properties.desktop.in.in
desktop_in_files = bluetooth-properties.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

schemadir = $(GCONF_SCHEMA_FILE_DIR)

schema_in_files = bluetooth-manager.schemas.in

schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

CLEANFILES = $(desktop_DATA) $(schema_DATA)

EXTRA_DIST = $(man_MANS) $(desktop_in_in_files) $(schema_in_files)

MAINTAINERCLEANFILES = Makefile.in

if GCONF_SCHEMAS_INSTALL
install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(schema_DATA)

uninstall-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif
