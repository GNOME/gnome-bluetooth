INCLUDES = \
  $(EGG_CFLAGS) \
  $(EGG_VFS_FS_CFLAGS) \
  -I$(top_srcdir)/libegg/util \
  -I$(top_srcdir)/libegg/filesystem \
  -I$(top_srcdir)/libegg/combo \
  -DEGG_COMPILATION \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libeggfileselector.la

libeggfileselector_la_LIBADD = \
	$(EGG_LIBS) \
	$(EGG_VFS_FS_LIBS) \
	$(top_builddir)/libegg/util/libeggutil.la \
	$(top_builddir)/libegg/filesystem/libeggfilesystem.la \
	$(top_builddir)/libegg/combo/eggentry.lo \
	$(top_builddir)/libegg/treeviewutils/eggtreemodelfilter.lo

libeggfileselector_la_SOURCES =	\
	eggfileselector.c		

noinst_HEADERS =		\
	eggfileselector.h		

noinst_PROGRAMS = testfileselector

testfileselector_SOURCES = testfileselector.c
testfileselector_LDFLAGS = libeggfileselector.la $(EGG_LIBS) $(EGG_VFS_FS_LIBS)
testfileselector_DEPENDENCIES = libeggfileselector.la 
