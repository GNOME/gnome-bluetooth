
INCLUDES = \
  $(EGG_CFLAGS) \
  -I$(top_srcdir)/libegg/util \
  -DEGG_COMPILATION \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED


noinst_LTLIBRARIES = libeggiconlist.la

libeggiconlist_la_LIBADD = \
	$(EGG_LIBS) \
	$(top_builddir)/libegg/util/libeggutil.la

libeggiconlist_la_SOURCES = \
  eggiconlist.c

noinst_HEADERS = \
  eggiconlist.h

noinst_PROGRAMS = testiconlist

testiconlist_SOURCES = testiconlist.c
testiconlist_LDFLAGS = libeggiconlist.la $(EGG_LIBS)
testiconlist_DEPENDENCIES = libeggiconlist.la 
