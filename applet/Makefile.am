
bin_PROGRAMS = bluetooth-applet

bluetooth_applet_SOURCES = main.c notify.h notify.c agent.h agent.c

bluetooth_applet_LDADD = $(top_builddir)/common/libcommon.a \
			@NOTIFY_LIBS@ @GCONF_LIBS@ @GTK_LIBS@ @DBUS_LIBS@

noinst_PROGRAMS = test-agentdialog

test_agentdialog_SOURCES = test-agentdialog.c notify.h notify.c

test_agentdialog_LDADD = $(top_builddir)/common/libcommon.a \
				@NOTIFY_LIBS@ @GTK_LIBS@ @DBUS_LIBS@

AM_CFLAGS = @DBUS_CFLAGS@ @GTK_CFLAGS@ @GCONF_CFLAGS@ @NOTIFY_CFLAGS@

INCLUDES = -I$(top_srcdir)/common

man_MANS = bluetooth-applet.1

autostartdir = $(sysconfdir)/xdg/autostart

autostart_in_files = bluetooth-applet.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(autostart_DATA)

EXTRA_DIST = $(man_MANS) $(autostart_in_files)

MAINTAINERCLEANFILES = Makefile.in
