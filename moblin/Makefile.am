SUBDIRS = moblin-copy-n-paste theme

bin_PROGRAMS = bluetooth-panel

bluetooth_panel_SOURCES = main.c moblin-panel.c moblin-panel.h

bluetooth_panel_LDADD = $(top_builddir)/lib/libcommon.la $(top_builddir)/lib/libgnome-bluetooth.la \
			$(top_builddir)/moblin/moblin-copy-n-paste/libmoblin.la \
			$(top_builddir)/wizard/libwizard.la $(MOBLIN_LIBS)

AM_CFLAGS = $(MOBLIN_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

INCLUDES = -I$(top_srcdir)/lib -I$(top_srcdir)/wizard -I$(top_srcdir)/moblin/moblin-copy-n-paste

autostartdir = $(sysconfdir)/xdg/autostart

autostart_in_in_files = bluetooth-panel.desktop.in.in
autostart_in_files = bluetooth-panel.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(autostart_DATA)

EXTRA_DIST = $(autostart_in_in_files)

MAINTAINERCLEANFILES = Makefile.in
