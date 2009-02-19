## Process this file with automake to produce Makefile.in

SUBDIRS = po src pixmaps

EXTRA_DIST = \
	intltool-extract.in intltool-merge.in intltool-update.in \
	COPYING COPYING.gnomebt TODO MAINTAINERS $(PACKAGE).pc.in

DISTCLEANFILES = intltool-extract intltool-merge intltool-update

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(PACKAGE).pc

