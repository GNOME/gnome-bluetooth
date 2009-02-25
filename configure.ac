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

DBUS_GLIB_REQUIRED=0.73
HAL_REQUIRED=0.5.8
NOTIFY_REQUIRED=0.4.3

dnl Requires for the properties window
PKG_CHECK_MODULES(PROPS,
		  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
		  hal >= $HAL_REQUIRED
		  gconf-2.0
		  gtk+-2.0)

dnl Requires for the applet
PKG_CHECK_MODULES(APPLET,
		  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
		  gconf-2.0
		  gtk+-2.0
		  libnotify >= $NOTIFY_REQUIRED)

dnl Requires for the sendto app
PKG_CHECK_MODULES(SENDTO,
		  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
		  gtk+-2.0
		  gio-2.0)

dnl Requires for the wizard dialogue
PKG_CHECK_MODULES(WIZARD,
		  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
		  gtk+-2.0)

dnl Requires for the libraries
PKG_CHECK_MODULES(LIBGNOMEBT,
		  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
		  gtk+-2.0)

DBUS_BINDING_TOOL="dbus-binding-tool"
AC_SUBST(DBUS_BINDING_TOOL)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

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
	  po/Makefile.in
)

