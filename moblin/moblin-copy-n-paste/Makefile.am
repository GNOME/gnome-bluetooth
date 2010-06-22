noinst_LTLIBRARIES = libmoblin.la
libmoblin_la_CFLAGS = $(MEEGO_CFLAGS) \
	              -DPKG_DATADIR=\"$(pkgdatadir)\" -Wall
libmoblin_la_LIBADD = $(MEEGO_LIBS)
libmoblin_la_SOURCES = koto-cell-renderer-pixbuf.c \
		       koto-cell-renderer-pixbuf.h \
		       mux-cell-renderer-text.c \
		       mux-cell-renderer-text.h \
		       mux-banner.c \
		       mux-banner.h
