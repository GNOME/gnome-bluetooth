# Configure paths for libbluetooth
# Edd Dumbill <edd@usefulinc.com>
# Shamelessly stolen from Jack Moffitt, who stole
# it from Owen Taylor and Manish Singh

dnl BLUEZ_PATH_BLUETOOTH([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libbluetooth, and define BLUETOOTH_CFLAGS and BLUETOOTH_LIBS
dnl
AC_DEFUN(BLUEZ_PATH_BLUETOOTH,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(bluetooth,[  --with-bluetooth=PFX   Prefix where libbluetooth is installed (optional)], bluetooth_prefix="$withval", bluetooth_prefix="")
AC_ARG_WITH(bluetooth-libraries,[  --with-bluetooth-libraries=DIR   Directory where libbluetooth library is installed (optional)], bluetooth_libraries="$withval", bluetooth_libraries="")
AC_ARG_WITH(bluetooth-includes,[  --with-bluetooth-includes=DIR   Directory where libbluetooth header files are installed (optional)], bluetooth_includes="$withval", bluetooth_includes="")
AC_ARG_ENABLE(bluetoothtest, [  --disable-bluetoothtest       Do not try to compile and run a test Bluetooth program],, enable_bluetoothtest=yes)

  if test "x$bluetooth_libraries" != "x" ; then
    BLUETOOTH_LIBS="-L$bluetooth_libraries"
  elif test "x$bluetooth_prefix" != "x" ; then
    BLUETOOTH_LIBS="-L$bluetooth_prefix/lib"
  elif test "x$prefix" != "xNONE" ; then
    BLUETOOTH_LIBS="-L$prefix/lib"
  fi

  BLUETOOTH_LIBS="$BLUETOOTH_LIBS -lbluetooth"

  if test "x$bluetooth_includes" != "x" ; then
    BLUETOOTH_CFLAGS="-I$bluetooth_includes"
  elif test "x$bluetooth_prefix" != "x" ; then
    BLUETOOTH_CFLAGS="-I$bluetooth_prefix/include"
  elif test "$prefix" != "xNONE"; then
    BLUETOOTH_CFLAGS="-I$prefix/include"
  fi

  AC_MSG_CHECKING(for libbluetooth)
  no_bluetooth=""


  if test "x$enable_bluetoothtest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $BLUETOOTH_CFLAGS"
    LIBS="$LIBS $BLUETOOTH_LIBS"
dnl
dnl Now check if the installed Bluetooth is sufficiently new.
dnl
      rm -f conf.bluetoothtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bluetooth/bluetooth.h>

int main ()
{
  bdaddr_t bdaddr;

  str2ba("00:00:00:11:11:11", &bdaddr);
  system("touch conf.bluetoothtest");
  return 0;
}

],, no_bluetooth=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_bluetooth" = "x" ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     if test -f conf.bluetoothtest ; then
       :
     else
       echo "*** Could not run libluetooth test program, checking why..."
       CFLAGS="$CFLAGS $BLUETOOTH_CFLAGS"
       LIBS="$LIBS $BLUETOOTH_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <bluetooth/bluetooth.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding libbluetooth or finding the wrong"
       echo "*** version of libbluetooth. If it is not finding libbluetooth, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occurred. This usually means libbluetooth was incorrectly installed"
       echo "*** or that you have moved libbluetooth since it was installed." ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     BLUETOOTH_CFLAGS=""
     BLUETOOTH_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(BLUETOOTH_CFLAGS)
  AC_SUBST(BLUETOOTH_LIBS)
  rm -f conf.bluetoothtest
])
