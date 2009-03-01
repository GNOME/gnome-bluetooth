
SUBDIRS = icons common applet properties wizard sendto docs po

EXTRA_DIST = intltool-extract.in intltool-update.in intltool-merge.in ChangeLog.pre-2.27 gtk-doc.make

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install \
	--disable-desktop-update --disable-mime-update --disable-icon-update \
	--enable-gtk-doc

DISTCLEANFILES = intltool-extract intltool-update intltool-merge

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in config.sub config.guess \
	ltmain.sh depcomp missing install-sh mkinstalldirs \
	intltool-extract.in intltool-update.in intltool-merge.in \
	gnome-bluetooth-1.0.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-bluetooth-1.0.pc

install-data-hook:
if DESKTOP_UPDATE
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
endif

uninstall-hook:
if DESKTOP_UPDATE
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
endif

# Build ChangeLog from GIT or SVN history
# (SVN r304 was the first ChangeLog-less checkin).
# Only build this when in a git or svn checkout.
#
SVN_ROOT=svn+ssh://svn.gnome.org/svn/
SVN_MODULE=gnome-bluetooth
SVN_BRANCH=trunk

ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat > $@; \
	elif test -f $(top_srcdir)/.svn/entries; then \
		svn log -v --xml -r HEAD:304 $(SVN_ROOT)/$(SVN_MODULE)/$(SVN_BRANCH) | \
			xsltproc --stringparam strip-prefix "$(SVN_MODULE)/$(SVN_BRANCH)" \
				 --stringparam include-rev "yes" $(top_srcdir)/svn2cl.xsl - > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

