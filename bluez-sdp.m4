# Configure paths for libbluetooth
# Edd Dumbill <edd@usefulinc.com>
# Shamelessly stolen from Jack Moffitt, who stole
# it from Owen Taylor and Manish Singh

dnl BLUEZ_PATH_BLUETOOTH([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libbluetooth, and define BLUETOOTH_CFLAGS and BLUETOOTH_LIBS
dnl
AC_DEFUN(BLUEZ_PATH_SDP,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(sdp,[  --with-sdp=PFX   Prefix where libsdp is installed (optional)], sdp_prefix="$withval", sdp_prefix="")
AC_ARG_WITH(sdp-libraries,[  --with-sdp-libraries=DIR   Directory where libsdp library is installed (optional)], sdp_libraries="$withval", sdp_libraries="")
AC_ARG_WITH(sdp-includes,[  --with-sdp-includes=DIR   Directory where libsdp header files are installed (optional)], sdp_includes="$withval", sdp_includes="")
AC_ARG_ENABLE(sdptest, [  --disable-sdptest       Do not try to compile and run a test Sdp program],, enable_sdptest=yes)

  if test "x$sdp_libraries" != "x" ; then
    SDP_LIBS="-L$sdp_libraries"
  elif test "x$sdp_prefix" != "x" ; then
    SDP_LIBS="-L$sdp_prefix/lib"
  elif test "x$prefix" != "xNONE" ; then
    SDP_LIBS="-L$prefix/lib"
  fi

  SDP_LIBS="$SDP_LIBS -lsdp"

  if test "x$sdp_includes" != "x" ; then
    SDP_CFLAGS="-I$sdp_includes"
  elif test "x$sdp_prefix" != "x" ; then
    SDP_CFLAGS="-I$sdp_prefix/include"
  elif test "$prefix" != "xNONE"; then
    SDP_CFLAGS="-I$prefix/include"
  fi

  AC_MSG_CHECKING(for libsdp 1.0 or better)
  no_sdp=""


  if test "x$enable_sdptest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $SDP_CFLAGS $GLIB_CFLAGS"
    LIBS="$LIBS $SDP_LIBS $GLIB_LIBS"
dnl
dnl Now check if the installed Sdp is sufficiently new.
dnl
      rm -f conf.sdptest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

int main ()
{
  uuid_t group;
  sdp_uuid16_create(&group, PUBLIC_BROWSE_GROUP);
  system("touch conf.sdptest");
  return 0;
}

],, no_sdp=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_sdp" = "x" ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     if test -f conf.sdptest ; then
       :
     else
       echo "*** Could not run libsdp test program, checking why..."
       CFLAGS="$CFLAGS $SDP_CFLAGS $GLIB_CFLAGS"
       LIBS="$LIBS $SDP_LIBS $GLIB_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <glib.h>
#include <bluetooth/sdp.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding libsdp 1.0 or finding the wrong"
       echo "*** version of libsdp. If it is not finding libsdp, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occurred. This usually means libsdp was incorrectly installed"
       echo "*** or that you have moved libsdp since it was installed." ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     SDP_CFLAGS=""
     SDP_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(SDP_CFLAGS)
  AC_SUBST(SDP_LIBS)
  rm -f conf.sdptest
])
