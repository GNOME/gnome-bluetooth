dnl Configure paths for libsdp
dnl Edd Dumbill <edd@usefulinc.com>
dnl Shamelessly stolen from Jack Moffitt, who stole
dnl it from Owen Taylor and Manish Singh

dnl BLUEZ_PATH_SDP([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
AC_DEFUN(BLUEZ_PATH_SDP,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_ENABLE(sdptest, [  --disable-sdptest       Do not try to compile and run a test SDP program],, enable_sdptest=yes)

  SDP_LIBS="-lsdp"
  SDP_CFLAGS=""

  AC_MSG_CHECKING(for SDP support)
  no_sdp=""

  if test "x$enable_sdptest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $SDP_CFLAGS $GLIB_CFLAGS $BLUETOOTH_CFLAGS"
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
#include <bluetooth/sdp.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding libsdp 1.0 or finding the wrong"
       echo "*** version of libsdp. "
       echo "***"
       echo "*** You can ignore this if your bluez-libs is version 2.6 or better."
       echo "*** If not, you won't be able to compile this code until you install"
       echo "*** bluez-sdp."
       ],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occurred. This usually means libsdp was incorrectly"
       echo "*** installed or that you have moved libsdp since it was installed."
       echo "***"
       echo "*** You can ignore this if your bluez-libs is version 2.7 or better."
       echo "*** If not, you won't be able to compile this code until you install"
       echo "*** bluez-sdp."

       ])
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
