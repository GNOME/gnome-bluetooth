
INCLUDES = \
  $(EGG_TRAY_CFLAGS) \
  -I$(top_srcdir)/libegg/util \
  -DEGG_COMPILATION \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED


noinst_LTLIBRARIES = libeggtray.la

libeggtray_la_LIBADD = \
	$(EGG_TRAY_LIBS) \
	$(top_builddir)/libegg/util/libeggutil.la

libeggtray_la_SOURCES = \
  eggtrayicon.c		\
  eggstatusicon.c	\
  eggtraymanager.c

noinst_HEADERS = \
  eggtrayicon.h	 \
  eggstatusicon.h\
  eggtraymanager.h

noinst_PROGRAMS = testtray testtrayicon teststatusicon

testtray_SOURCES = testtray.c
testtray_LDFLAGS = libeggtray.la $(EGG_TRAY_LIBS)
testtray_DEPENDENCIES = libeggtray.la 

testtrayicon_SOURCES = testtrayicon.c
testtrayicon_LDFLAGS = libeggtray.la $(EGG_TRAY_LIBS)
testtrayicon_DEPENDENCIES = libeggtray.la 

teststatusicon_SOURCES = teststatusicon.c
teststatusicon_LDFLAGS = libeggtray.la $(EGG_TRAY_LIBS)
teststatusicon_DEPENDENCIES = libeggtray.la 
