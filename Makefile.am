
SUBDIRS = po icons common applet properties wizard \
				sendto browse analyzer explorer proximity

EXTRA_DIST = intltool-extract.in intltool-update.in intltool-merge.in

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install \
	--disable-desktop-update --disable-mime-update --disable-icon-update

DISTCLEANFILES = intltool-extract intltool-update intltool-merge

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in config.sub config.guess \
	ltmain.sh depcomp missing install-sh mkinstalldirs \
	intltool-extract.in intltool-update.in intltool-merge.in

install-data-hook:
if ICON_UPDATE
	$(UPDATE_ICON_CACHE) -f -t $(DESTDIR)$(datadir)/icons/hicolor
endif
if MIME_UPDATE
	$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime
endif
if DESKTOP_UPDATE
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
endif

uninstall-hook:
if ICON_UPDATE
	$(UPDATE_ICON_CACHE) $(DESTDIR)$(datadir)/icons/hicolor
endif
if MIME_UPDATE
	$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime
endif
if DESKTOP_UPDATE
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
endif
