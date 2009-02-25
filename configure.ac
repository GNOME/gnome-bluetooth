AC_PREREQ(2.50)
AC_INIT()

AM_INIT_AUTOMAKE(gnome-bluetooth, 2.27.0)
AM_CONFIG_HEADER(config.h)

# Before making a release, the GNOMEBT_LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
GNOMEBT_LT_VERSION=2:0:0
AC_SUBST(GNOMEBT_LT_VERSION)

AM_MAINTAINER_MODE

AC_LANG_C
AC_PROG_CC
AC_PROG_INSTALL

dnl Initialize libtool
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

GETTEXT_PACKAGE=gnome-bluetooth
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
					[The name of the gettext domain])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AC_PATH_PROG(GCONFTOOL, [gconftool-2])
AM_GCONF_SOURCE_2

AC_DEFINE_UNQUOTED(LOCALEDIR, "${prefix}/share/locale", [Directory for the localization files])

AC_ARG_ENABLE(desktop-update, AC_HELP_STRING([--disable-desktop-update],
					[Disable desktop database update]))
if (test "$enable_desktop_update" != no); then
	AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, [update-desktop-database])
fi
AM_CONDITIONAL([DESKTOP_UPDATE], [test -n "$UPDATE_DESKTOP_DATABASE"])

AC_ARG_ENABLE(mime-update, AC_HELP_STRING([--disable-mime-update],
					[Disable mime database update]))
if (test "$enable_mime_update" != no); then
	AC_PATH_PROG(UPDATE_MIME_DATABASE, [update-mime-database])
fi
AM_CONDITIONAL([MIME_UPDATE], [test -n "$UPDATE_MIME_DATABASE"])

AC_ARG_ENABLE(icon-update, AC_HELP_STRING([--disable-icon-update],
					[Disable icon cache update]))
if (test "$enable_icon_update" != no); then
	AC_PATH_PROG(UPDATE_ICON_CACHE, [gtk-update-icon-cache])
fi
AM_CONDITIONAL([ICON_UPDATE], [test -n "$UPDATE_ICON_CACHE"])

PKG_CHECK_MODULES(DBUS, dbus-glib-1 >= 0.73, dummy=yes,
				AC_MSG_ERROR(dbus-glib >= 0.73 is required))
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

DBUS_BINDING_TOOL="dbus-binding-tool"
AC_SUBST(DBUS_BINDING_TOOL)

PKG_CHECK_MODULES(HAL, hal >= 0.5.8, [
		AC_DEFINE(HAVE_HAL, 1, [Define to 1 if you have HAL support.])
], dummy=no)
AC_SUBST(HAL_CFLAGS)
AC_SUBST(HAL_LIBS)

PKG_CHECK_MODULES(GIO, gio-2.0 >= 2.16, dummy=yes,
				AC_MSG_ERROR(gio >= 2.16 is required))
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.10, dummy=yes,
				AC_MSG_ERROR(gtk+ >= 2.10 is required))
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.16, dummy=yes,
				AC_MSG_ERROR(gconf >= 2.16 is required))
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

PKG_CHECK_MODULES(NOTIFY, libnotify >= 0.4.3, dummy=yes,
				AC_MSG_ERROR(libnotify is required))
AC_SUBST(NOTIFY_CFLAGS)
AC_SUBST(NOTIFY_LIBS)

GNOME_COMMON_INIT
GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS
GNOME_MAINTAINER_MODE_DEFINES

AC_OUTPUT(Makefile
	  gnome-bluetooth-1.0.pc
	  icons/Makefile
	  common/Makefile
	  applet/Makefile
	  properties/Makefile
	  wizard/Makefile
	  sendto/Makefile
	  browse/Makefile
	  po/Makefile.in
)

