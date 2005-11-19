
pythondir = @pythondir@/gnomebt
pkgpythondir = $(pythondir)
libdir = $(pkgpythondir)

python_LTLIBRARIES = iconlist.la

python_DATA = __init__.py manager.py hig_alert.py defs.py

CTAGS_ARGS = $(python_DATA)

noinst_PYTHON = testiconlist.py

INCLUDES = \
	-I$(GNOME_BLUETOOTH_PYTHON_CFLAGS) \
	$(PYTHON_INCLUDES) \
	-I$(top_srcdir)/libegg/libegg/iconlist

iconlist_la_SOURCES = iconlist.c \
					  iconlistmodule.c \
					  gtkobject-support.c \
					  pyegg-private.h

iconlist_la_LDFLAGS = -module -avoid-version

iconlist_la_LIBADD = \
	$(GNOME_BLUETOOTH_PYTHON_LIBS) \
	../libegg/libegg/iconlist/libeggiconlist.la

EXTRA_DIST = iconlist.defs iconlist.override \
			 gnome-bluetooth-manager.desktop.in \
			 $(python_DATA) gnome-bluetooth-manager
			 $(noinst_PYTHON)

iconlist.c: iconlist.defs iconlist.override
	pygtk-codegen-2.0 --prefix iconlist \
		--register $(DEFSDIR)/gdk-types.defs \
		--register $(DEFSDIR)/gtk-types.defs \
		--override $(srcdir)/iconlist.override \
		$(srcdir)/iconlist.defs > $@

defs.py: Makefile
	echo "version = '$(PACKAGE_VERSION)'" >defs.py
	echo "datadir = '$(pkgdatadir)'" >>defs.py

desktop_in_files = gnome-bluetooth-manager.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-exec-hook:
	$(INSTALL_SCRIPT) $(srcdir)/gnome-bluetooth-manager $(DESTDIR)$(bindir)/gnome-bluetooth-manager
