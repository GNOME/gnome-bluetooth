## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-g \
	-I$(top_srcdir)/intl \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(BLUETOOTH_CFLAGS) \
	$(SDP_CFLAGS) \
    -DDATA_DIR="\"$(GNOME_BLUETOOTH_DATA_DIR)\"" \
	$(GNOME_BLUETOOTH_PY_CFLAGS) \
	-I$(PYTHON_PREFIX)/include/python$(PYTHON_VERSION)/

noinst_PROGRAMS=gconftest gnomebt-controller-test \
				gnomebt-chooser-test

lib_LTLIBRARIES = libgnomebt.la
if HAVE_PYTHON
py_LTLIBRARIES = controller.la
else
py_LTLIBRARIES =
endif

libgnomebt_la_SOURCES = \
	gnomebt-spinner.c gnomebt-icons.c gnomebt-icons.h \
	gnomebt-chooser.c gnomebt-controller.c

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

gconftest_SOURCES = \
	gconftest.c

gconftest_LDFLAGS = \
	$(GNOME_BLUETOOTH_LIBS) $(BLUETOOTH_LIBS) $(SDP_LIBS)

EXTRA_DIST = \
	spinner.gob chooser.gob controller.gob

BUILT_SOURCES = \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h \
	gnomebt-controller.c gnomebt-controller.h gnomebt-controller-private.h

CLEANFILES =  \
	gnomebt-spinner.c gnomebt-spinner.h gnomebt-spinner-private.h \
	gnomebt-chooser.c gnomebt-chooser.h gnomebt-chooser-private.h \
	gnomebt-controller.c gnomebt-controller.h gnomebt-controller-private.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

gnomebt-%.c gnomebt-%.h gnomebt-%-private.h: %.gob
	@GOB2@ $<

# Python bindings
CLEANFILES += btctl-py.c
EXTRA_DIST += btctl.defs btctl.override
                                                                                
if HAVE_PYTHON
regenerate-defs: gnomebt-controller.h
	python /usr/share/pygtk/2.0/codegen/h2def.py -v gnomebt-controller.h > gnomebt-controller.defs
                                                                                
gnomebt-controller-py.c: gnomebt-controller.defs gnomebt-controller.override
	pygtk-codegen-2.0 --prefix controller \
		--register $(DEFSDIR)/gtk-types.defs \
		--override gnomebt-controller.override \
		gnomebt-controller.defs > $@
                                                                                
pydir = $(PYTHON_PREFIX)/lib/python$(PYTHON_VERSION)/site-packages/gtk-2.0/gnomebt
controller_la_SOURCES = gnomebt-controller-py.c gnomebt-controller-pymodule.c
controller_la_LIBADD =  libgnomebt.la
controller_la_LDFLAGS = $(GNOME_BLUETOOTH_PY_LIBS) \
						$(GNOME_BLUETOOTH_LIBS) \
						-module
endif

