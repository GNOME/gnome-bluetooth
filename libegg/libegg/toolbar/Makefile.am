
INCLUDES = \
  $(EGG_CFLAGS) \
  -DEGG_COMPILATION \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED \
  -I$(top_builddir)/libegg/util


noinst_LTLIBRARIES = libegg-toolbar.la

libegg_toolbar_la_LIBADD = $(EGG_LIBS) $(top_builddir)/libegg/util/libeggutil.la
libegg_toolbar_la_SOURCES = \
  eggtoolbar.c \
  eggtoolitem.c \
  eggseparatortoolitem.c \
  eggradiotoolbutton.c \
  eggtoolbutton.c \
  eggtoggletoolbutton.c

noinst_HEADERS = \
  eggtoolbar.h \
  eggtoolitem.h	 \
  eggseparatortoolitem.h \
  eggradiotoolbutton.h \
  eggtoolbutton.h \
  eggtoggletoolbutton.h

noinst_PROGRAMS = test-toolbar gtkexample test-drag

gtkexample_SOURCES = gtkexample.c

gtkexample_LDADD = $(EGG_LIBS) \
  $(top_builddir)/libegg/toolbar/libegg-toolbar.la

test_toolbar_SOURCES = test-toolbar.c

test_toolbar_LDADD = $(EGG_LIBS) \
  $(top_builddir)/libegg/toolbar/libegg-toolbar.la

test_drag_SOURCES = test-drag.c

test_drag_LDADD = $(EGG_LIBS) \
  $(top_builddir)/libegg/toolbar/libegg-toolbar.la
