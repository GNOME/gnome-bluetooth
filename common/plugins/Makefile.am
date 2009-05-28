plugindir = $(libdir)/gnome-bluetooth/plugins/

INCLUDES =                                      \
        -DDATADIR=\"$(datadir)\"                \
        -DICONDIR=\"$(icondir)\"                \
	-DLOCALEDIR="\"$(datadir)/locale\""	\
        -I$(top_srcdir)/common			\
        -I$(top_builddir)			\
	$(PLUGINS_CFLAGS)			\
	$(WARN_CFLAGS)

plugin_LTLIBRARIES = libgbttest.la

libgbttest_la_SOURCES = test.c
libgbttest_la_LDFLAGS = -module -avoid-version
libgbttest_la_LIBADD = $(PLUGINS_LIBS)

# override to _not_ install the test plugins
install-pluginLTLIBRARIES:
