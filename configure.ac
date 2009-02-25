AC_PREREQ(2.50)
AC_INIT()

AM_INIT_AUTOMAKE(bluez-gnome, 1.8)
AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AC_INIT_BLUEZ

AC_LANG_C

AC_PROG_CC
AC_PROG_CC_PIE
AC_PROG_INSTALL
AC_PROG_RANLIB

GETTEXT_PACKAGE=bluetooth-manager
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
					[The name of the gettext domain])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AC_PATH_PROG(GCONFTOOL, [gconftool-2])
AM_GCONF_SOURCE_2

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

dnl PKG_CHECK_MODULES(OPENOBEX, libopenobex-glib >= 1.4, dummy=yes, dummy=no)
dnl AC_SUBST(OPENOBEX_CFLAGS)
dnl AC_SUBST(OPENOBEX_LIBS)

AC_ARG_BLUEZ

AC_OUTPUT(Makefile icons/Makefile common/Makefile
			applet/Makefile properties/Makefile wizard/Makefile
			sendto/Makefile browse/Makefile analyzer/Makefile
			explorer/Makefile proximity/Makefile po/Makefile.in)
