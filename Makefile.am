## Process this file with automake to produce Makefile.in

SUBDIRS = po libegg src obex python nautilus

EXTRA_DIST = \
	autogen.sh \
    intltool-extract.in intltool-merge.in intltool-update.in

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*.png; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(datadir)/$(PACKAGE)/pixmaps; \
	    fi \
	  done \
	fi

	if test -d $(srcdir)/ui; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE); \
	  for uifile in $(srcdir)/ui/*.glade; do \
	    if test -f $$uifile; then \
	      $(INSTALL_DATA) $$uifile $(DESTDIR)$(datadir)/$(PACKAGE); \
	    fi \
	  done \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi
	if test -d ui; then \
	  mkdir $(distdir)/ui; \
	  for uifile in ui/*; do \
	    if test -f $$uifile; then \
	      cp -p $$uifile $(distdir)/ui; \
	    fi \
	  done \
	fi
