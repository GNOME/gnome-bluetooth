
PYDEFS = `pkg-config --variable=defsdir pygtk-2.0`

# sigh, some hackery around what automake normally provides is
# necessary, in order to install into a subdir of site-packages
# sorry to anyone this offends :-)

pythondir = $(PYTHON_PREFIX)/lib/python$(PYTHON_VERSION)/site-packages/gnomebt
pkgpythondir = $(pythondir)
libdir = $(pkgpythondir)

lib_LTLIBRARIES = iconlist.la

python_PYTHON = __init__.py manager.py defs.py

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
		--register $(PYDEFS)/gdk-types.defs \
		--register $(PYDEFS)/gtk-types.defs \
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
