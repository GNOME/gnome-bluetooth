## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-g \
	-I$(top_srcdir)/intl \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(BLUETOOTH_CFLAGS) \
	-DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\"" \
	$(GNOME_BLUETOOTH_PY_CFLAGS) \
	$(PYTHON_INCLUDES)

noinst_PROGRAMS=gconftest gnomebt-controller-test \
	gnomebt-chooser-test gnomebt-permissiondialog-test

lib_LTLIBRARIES = libgnomebt.la
if HAVE_PYTHON
py_LTLIBRARIES = controller.la chooser.la
else
py_LTLIBRARIES =
endif

libgnomebt_la_SOURCES = \
	gnomebt-spinner.c gnomebt-icons.c gnomebt-icons.h \
	gnomebt-chooser.c gnomebt-controller.c gnomebt-permissiondialog.c

libgnomebt_la_LDFLAGS = \
	-version-info 0:0:0

gnomebt_chooser_test_SOURCES = \
	gnomebt-chooser-test.c

gnomebt_chooser_test_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) libgnomebt.la

gnomebt_controller_test_SOURCES = \
	gnomebt-controller-test.c

gnomebt_controller_test_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) libgnomebt.la

gnomebt_permissiondialog_test_SOURCES = \
	gnomebt-permissiondialog-test.c

gnomebt_permissiondialog_test_LDADD = \
	$(GNOME_BLUETOOTH_LIBS) libgnomebt.la

gconftest_SOURCES = \
	gconftest.c

gconftest_LDFLAGS = \
	$(GNOME_BLUETOOTH_LIBS) $(BLUETOOTH_LIBS)

GOB_DIST = \
	spinner.gob chooser.gob controller.gob permissiondialog.gob

CTAGS_ARGS = --langmap=c++:.gob \
	$(GOB_DIST)

BUILT_SOURCES = \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h \
	gnomebt-controller.c gnomebt-controller.h gnomebt-controller-private.h \
	gnomebt-permissiondialog.c gnomebt-permissiondialog.h gnomebt-permissiondialog-private.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

gnomebt-%.c gnomebt-%.h gnomebt-%-private.h: %.gob
	@GOB2@ $<

# Python bindings
PYTHON_CLEANFILES = btctl-py.c
PYTHON_DIST = btctl.defs btctl.override

EXTRA_DIST = $(GOB_DIST) $(PYTHON_DIST)
CLEANFILES = $(PYTHON_CLEANFILES)

if HAVE_PYTHON
regenerate-defs: gnomebt-controller.defs gnomebt-chooser.defs

gnomebt-controller.defs: gnomebt-controller.h
	python /usr/share/pygtk/2.0/codegen/h2def.py -v gnomebt-controller.h > gnomebt-controller.defs

gnomebt-chooser.defs: gnomebt-chooser.h
	python /usr/share/pygtk/2.0/codegen/h2def.py -v gnomebt-chooser.h > gnomebt-chooser.defs

gnomebt-controller-py.c: gnomebt-controller.defs gnomebt-controller.override
	pygtk-codegen-2.0 --prefix controller \
		--register $(DEFSDIR)/gtk-types.defs \
		--override gnomebt-controller.override \
		gnomebt-controller.defs > $@

gnomebt-chooser-py.c: gnomebt-chooser.defs gnomebt-controller.defs gnomebt-chooser.override
	pygtk-codegen-2.0 --prefix chooser \
		--register $(DEFSDIR)/gtk-types.defs \
		--register gnomebt-controller.defs \
		--override gnomebt-chooser.override \
		gnomebt-chooser.defs > $@

pydir = $(PYTHON_PREFIX)/lib/python$(PYTHON_VERSION)/site-packages/gnomebt/

controller_la_SOURCES = gnomebt-controller-py.c gnomebt-controller-pymodule.c
controller_la_LIBADD =  libgnomebt.la
controller_la_LDFLAGS = $(GNOME_BLUETOOTH_PY_LIBS) \
			$(GNOME_BLUETOOTH_LIBS) \
			-module

chooser_la_SOURCES = gnomebt-chooser-py.c gnomebt-chooser-pymodule.c
chooser_la_LIBADD =  libgnomebt.la
chooser_la_LDFLAGS = 	$(GNOME_BLUETOOTH_PY_LIBS) \
			$(GNOME_BLUETOOTH_LIBS) \
			-module

endif

