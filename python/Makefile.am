
pythondir = $(PYTHON_PREFIX)/lib/python$(PYTHON_VERSION)/site-packages/gtk-2.0/gnomebt
pkgpythondir = $(pythondir)
libdir = $(pkgpythondir)

python_LTLIBRARIES = iconlist.la

python_DATA = __init__.py manager.py defs.py

noinst_PYTHON = testiconlist.py

CFLAGS = \
	-I$(GNOME_BLUETOOTH_PYTHON_CFLAGS) \
	-I$(PYTHON_PREFIX)/include/python$(PYTHON_VERSION)/ \
	-I$(PYTHON_PREFIX)/include/python$(PYTHON_VERSION) \
	-I ../libegg/libegg/iconlist

iconlist_la_SOURCES = iconlist.c \
					  iconlistmodule.c \
					  gtkobject-support.c \
					  pyegg-private.h

iconlist_la_LDFLAGS = -module

iconlist_la_LIBADD = \
	$(GNOME_BLUETOOTH_PYTHON_LIBS) \
	../libegg/libegg/iconlist/libeggiconlist.la

EXTRA_DIST = iconlist.defs iconlist.override \
			 gnome-bluetooth-manager.desktop.in

iconlist.c: iconlist.defs iconlist.override            
	pygtk-codegen-2.0 --prefix iconlist \
		--register $(DEFSDIR)/gdk-types.defs \
		--register $(DEFSDIR)/gtk-types.defs \
		--override iconlist.override \
		iconlist.defs > $@

defs.py: Makefile
	echo "version = '$(PACKAGE_VERSION)'" >defs.py
	echo "datadir = '$(pkgdatadir)'" >>defs.py

desktop_in_files = gnome-bluetooth-manager.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-exec-hook:
	$(INSTALL_SCRIPT) gnome-bluetooth-manager $(bindir)/gnome-bluetooth-manager
