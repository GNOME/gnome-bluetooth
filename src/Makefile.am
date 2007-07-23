## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-g \
	-I$(top_srcdir)/intl \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(BLUETOOTH_CFLAGS) \
	-DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\"" \
	$(GNOME_BLUETOOTH_PYTHON_CFLAGS)  $(PYTHON_INCLUDES) \
	-I$(srcdir) -I../src

noinst_PROGRAMS=gconftest gnomebt-controller-test \
	gnomebt-chooser-test gnomebt-permissiondialog-test \
	gnomebt-fileactiondialog-test spinner-test

lib_LTLIBRARIES = libgnomebt.la
py_LTLIBRARIES = controller.la chooser.la

libgnomebt_la_SOURCES = \
	gnomebt-controller.c gnomebt-controller.h \
	gnomebt-permissiondialog.c gnomebt-permissiondialog.h \
	gnomebt-spinner.c gnomebt-spinner.h \
	gnomebt-chooser.c gnomebt-chooser.h \
	gnomebt-icons.c gnomebt-icons.h \
	gnomebt-fileactiondialog.c gnomebt-fileactiondialog.h

pkginclude_HEADERS = \
	gnomebt-icons.h gnomebt-chooser.h gnomebt-spinner.h \
	gnomebt-controller.h gnomebt-permissiondialog.h \
	gnomebt-fileactiondialog.h

gnomebt-chooser.lo: gnomebt-controller.h gnomebt-spinner.h

libgnomebt_la_LDFLAGS = \
	-version-info $(GNOMEBT_LT_VERSION)
libgnomebt_la_LIBADD = $(GNOME_BLUETOOTH_LIBS)

gnomebt_chooser_test_SOURCES = \
	gnomebt-chooser-test.c

gnomebt_chooser_test_LDADD = \
	libgnomebt.la $(GNOME_BLUETOOTH_LIBS)

gnomebt_controller_test_SOURCES = \
	gnomebt-controller-test.c

gnomebt_controller_test_LDADD = \
	libgnomebt.la $(GNOME_BLUETOOTH_LIBS)

gnomebt_fileactiondialog_test_SOURCES = \
	gnomebt-fileactiondialog-test.c

gnomebt_fileactiondialog_test_LDADD = \
	libgnomebt.la $(GNOME_BLUETOOTH_LIBS)

gnomebt_permissiondialog_test_SOURCES = \
	gnomebt-permissiondialog-test.c

gnomebt_permissiondialog_test_LDADD = \
	libgnomebt.la $(GNOME_BLUETOOTH_LIBS)

spinner_test_SOURCES = \
	spinner-test.c

spinner_test_LDADD = \
	libgnomebt.la $(GNOME_BLUETOOTH_LIBS)

gconftest_SOURCES = \
	gconftest.c

gconftest_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) $(BLUETOOTH_LIBS)

GOB_DIST = \
	spinner.gob chooser.gob controller.gob permissiondialog.gob \
	fileactiondialog.gob

CTAGS_ARGS = --langmap=c++:.gob \
	$(GOB_DIST)

BUILT_SOURCES = \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h \
	gnomebt-controller.c gnomebt-controller.h gnomebt-controller-private.h \
	gnomebt-permissiondialog.c gnomebt-permissiondialog.h gnomebt-permissiondialog-private.h \
	gnomebt-fileactiondialog.c gnomebt-fileactiondialog.h gnomebt-fileactiondialog-private.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

gnomebt-%.c gnomebt-%.h gnomebt-%-private.h: %.gob
	@GOB2@ $<

# Python bindings
PYTHON_CLEANFILES = btctl-py.c
PYTHON_DIST = \
	gnomebt-controller.defs gnomebt-controller.override \
	gnomebt-chooser.defs gnomebt-chooser.override

EXTRA_DIST = $(GOB_DIST) $(PYTHON_DIST)
CLEANFILES = $(PYTHON_CLEANFILES) *.defs *-py.c						\
	     gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h		\
	     gnomebt-controller.c gnomebt-controller.h gnomebt-controller-private.h	\
	     gnomebt-fileactiondialog.c gnomebt-fileactiondialog.h gnomebt-fileactiondialog-private.h	\
	     gnomebt-permissiondialog.c gnomebt-permissiondialog.h gnomebt-permissiondialog-private.h	\
	     gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h

gnomebt-controller.defs: gnomebt-controller.h
	python $(CODEGENDIR)/h2def.py -v gnomebt-controller.h > gnomebt-controller.defs

gnomebt-chooser.defs: gnomebt-chooser.h
	python $(CODEGENDIR)/h2def.py -v gnomebt-chooser.h > gnomebt-chooser.defs

gnomebt-controller-py.c: gnomebt-controller.defs gnomebt-controller.override
	pygtk-codegen-2.0 --prefix controller \
		--register $(DEFSDIR)/gtk-types.defs \
		--override $(srcdir)/gnomebt-controller.override \
		$(srcdir)/gnomebt-controller.defs > $@

gnomebt-chooser-py.c: gnomebt-chooser.defs gnomebt-controller.defs gnomebt-chooser.override gnomebt-spinner.h gnomebt-controller.h
	pygtk-codegen-2.0 --prefix chooser \
		--register $(DEFSDIR)/gtk-types.defs \
		--register $(srcdir)/gnomebt-controller.defs \
		--override $(srcdir)/gnomebt-chooser.override \
		$(srcdir)/gnomebt-chooser.defs > $@

pydir = @pythondir@/gnomebt/

controller_la_SOURCES = gnomebt-controller-py.c gnomebt-controller-pymodule.c
controller_la_LIBADD =  libgnomebt.la \
			$(GNOME_BLUETOOTH_PYTHON_LIBS) \
			$(GNOME_BLUETOOTH_LIBS)
controller_la_LDFLAGS =	-module -avoid-version

chooser_la_SOURCES = gnomebt-chooser-py.c gnomebt-chooser-pymodule.c
chooser_la_LIBADD =  libgnomebt.la \
			$(GNOME_BLUETOOTH_PYTHON_LIBS) \
			$(GNOME_BLUETOOTH_LIBS)
chooser_la_LDFLAGS = -module -avoid-version
