#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(messasy, 3.03, github-oss@designet.co.jp)
AC_CONFIG_AUX_DIR(./shtool)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([client_side.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_RANLIB

SO_LDFLAGS=$LDFLAGS
SO_CFLAGS=$CFLAGS

BUILD_LIBDG=""
BUILD_MYSO=""

# get system architecture
ARCH=`uname -m`
if test $ARCH = "x86_64" ; then
  if test -d $prefix/lib64;
  then
    LIB_DIR="lib64" 
  else
    LIB_DIR="lib"
  fi
else
  LIB_DIR="lib"
fi

#get path of libmilter.so
if  test -e /usr/$LIB_DIR/libmilter.so;
then
  MIL_PATH="/usr/$LIB_DIR/"
else
  if test -e /usr/local/$LIB_DIR/libmilter.so;
  then
    MIL_PATH="/usr/local/$LIB_DIR/"
  else
    if test -e /usr/$LIB_DIR/libmilter/libmilter.so
    then
      MIL_PATH="/usr/$LIB_DIR/libmilter/"
    else
      if  test -e /usr/local/$LIB_DIR/libmilter/libmilter.so
      then
        MIL_PATH="/usr/local/$LIB_DIR/libmilter/"
      else
        MIL_PATH="/usr/$LIB_DIR/"
      fi
    fi
  fi
fi

# Checks for libraries.
AC_ARG_WITH([libdgstr],
        [AS_HELP_STRING([--with-libdgstr=DIR], [libdgstr path])],
        [LDFLAGS="$LDFLAGS -ldgstr"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([dgstr], [str_replace_tag], [],
                      [AC_MSG_ERROR([libdgstr was not found!])])],
        [LDFLAGS="$LDFLAGS -ldgstr"
         SO_LDFLAGS="$SO_LDFLAGS -ldgstr"
         CFLAGS="$CFLAGS -I./lib/libdgstr"
#         BUILD_LIBDG="$BUILD_LIBDG libdgstr"
         SO_CFLAGS="$SO_CFLAGS -I../../lib/libdgstr"])
AC_ARG_WITH([libdgconfig],
        [AS_HELP_STRING([--with-libdgconfig=DIR], [libdgconfig path])],
        [LDFLAGS="$LDFLAGS -ldgconfig"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([dgconfig], [read_config], [],
                      [AC_MSG_ERROR([libdgconfig was not found!])])],
        [LDFLAGS="$LDFLAGS -ldgconfig"
         SO_LDFLAGS="$SO_LDFLAGS -ldgconfig"
         CFLAGS="$CFLAGS -I./lib/libdgconfig"
#         BUILD_LIBDG="$BUILD_LIBDG libdgconfig"
         SO_CFLAGS="$SO_CFLAGS -I../../lib/libdgconfig"])
AC_ARG_WITH([libdgmail],
        [AS_HELP_STRING([--with-libdgmail=DIR], [libdgmail path])],
        [LDFLAGS="$LDFLAGS -ldgmail"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([dgmail], [decode_mime], [],
                      [AC_MSG_ERROR([libdgmail was not found!])])],
        [LDFLAGS="$LDFLAGS -ldgmail"
         SO_LDFLAGS="$SO_LDFLAGS -ldgmail"
         CFLAGS="$CFLAGS -I./lib/libdgmail"
#         BUILD_LIBDG="$BUILD_LIBDG libdgmail"
         SO_CFLAGS="$SO_CFLAGS -I../../lib/libdgmail"])
AC_ARG_WITH([libmilter],
        [AS_HELP_STRING([--with-libmilter=DIR], [libmilter path])],
        [LDFLAGS="$LDFLAGS -L$withval/$LIB_DIR -lmilter"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([milter], [smfi_main], [],
                      [AC_MSG_ERROR([libmilter was not found!])])],
        [LDFLAGS="$LDFLAGS -L$MIL_PATH -lmilter"])
AC_ARG_WITH([libpthread],
        [AS_HELP_STRING([--with-libpthread=DIR], [libpthread path])],
        [LDFLAGS="$LDFLAGS -L$withval/$LIB_DIR -lpthread"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([pthread], [pthread_create], [],
                      [AC_MSG_ERROR([libpthread was not found!])])],
        [LDFLAGS="$LDFLAGS -lpthread"])
AC_ARG_WITH([libldap],
        [AS_HELP_STRING([--with-libldap=DIR], [libldap path])],
        [LDFLAGS="$LDFLAGS -L$withval/$LIB_DIR -lldap"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([ldap], [ldap_initialize], [],
                      [AC_MSG_ERROR([libldap was not found!])])],
        [LDFLAGS="$LDFLAGS -lldap"])

AC_ARG_ENABLE([libwrap],
        [AS_HELP_STRING([--disable-libwrap], [disable to build libwrap [default=no]])])
if test "$enable_libwrap" = "yes" ; then
    AC_SUBST(DEF_LIBWRAP, "-D__HAVE_LIBWRAP")
    AC_SUBST(LIB_WRAP, "-lwrap")
else
    AC_SUBST(DEF_LIBWRAP, "")
    AC_SUBST(LIB_WRAP, "")
fi

if test "$enable_libwrap" = "yes" ; then
    AC_ARG_WITH([libwrap],
        [AS_HELP_STRING([--with-libwrap=DIR], [libwrap path])],
        [LDFLAGS="$LDFLAGS -L$withval/$LIB_DIR -lwrap"
         CFLAGS="$CFLAGS -I$withval/include"
         AC_CHECK_LIB([wrap], [hosts_ctl], [],
                      [AC_MSG_ERROR([libwrap was not found!])])],
        [LDFLAGS="$LDFLAGS -lwrap"])
fi

## Checks for libtthdrop libraries
#AC_ARG_WITH([libtokyotyrant],
#	[AS_HELP_STRING([--with-libtokyotyrant=DIR], [libtokyotyrant path])],
#	[SO_LDFLAGS="$SO_LDFLAGS -L$withval/$LIB_DIR"
#	 SO_CFLAGS="$SO_CFLAGS -I$withval/include"
#	 AC_CHECK_LIB([tokyotyrant], [tcrdbnew], [],
#			[AC_MSG_ERROR(libtokyotyrant.so is not found.)])],
#	[LDFLAGS="$LDFLAGS"])
#AC_ARG_WITH([libtokyocabinet],
#	[AS_HELP_STRING([--with-libtokyocabinet=DIR], [libtokyocabinet path])],
#	[SO_LDFLAGS="$SO_LDFLAGS -L$withval/$LIB_DIR"
#	 SO_CFLAGS="$SO_CFLAGS -I$withval/include"
#	 AC_CHECK_LIB([tokyocabinet], [tctdbnew], [],
#			[AC_MSG_ERROR(libtokyocabinet is not found.)])],
#	[LDFLAGS="$LDFLAGS"])

# Checks for libmaildrop libraries
AC_ARG_ENABLE([libmaildrop],
        [AS_HELP_STRING([--disable-libmaildrop], [disable to build libmaildrop [default=yes]])])

if test "$enable_libmaildrop" = "no" ; then
        MAKE_SO="$MAKE_SO"
else
        BUILD_MYSO="$BUILD_MYSO libmaildrop"
fi
AC_ARG_ENABLE([libgzip],
        [AS_HELP_STRING([--disable-libgzip], [disable to build libgzip [default=yes]])])
if test "$enable_libgzip" = "no" ; then
        MAKE_SO="$MAKE_SO"
else
        BUILD_MYSO="$BUILD_MYSO libgzip"
fi
AC_ARG_ENABLE([libenczip],
        [AS_HELP_STRING([--disable-libenczip], [disable to build libenczip [default=yes]])])
if test "$enable_libenczip" = "no" ; then
        MAKE_SO="$MAKE_SO"
else
        BUILD_MYSO="$BUILD_MYSO libenczip"
fi
AC_ARG_ENABLE([liblda],
        [AS_HELP_STRING([--disable-liblda], [disable to build liblda [default=yes]])])
if test "$enable_liblda" = "no" ; then
        MAKE_SO="$MAKE_SO"
else
        BUILD_MYSO="$BUILD_MYSO liblda"
fi

AC_SUBST(SO_LDFLAGS, $SO_LDFLAGS)
AC_SUBST(SO_CFLAGS, $SO_CFLAGS)
AC_SUBST(MAKE_SO, $MAKE_SO)
AC_SUBST(MYSODIR, ${prefix}/messasy/lib)
AC_SUBST(BUILD_LIBDG, $BUILD_LIBDG)
AC_SUBST(BUILD_MYSO, $BUILD_MYSO)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_FORK
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([inet_ntoa localtime_r memset mkdir regcomp socket strcasecmp strchr strdup strerror strncasecmp strndup strstr strtol strtoul uname setenv])

AC_CONFIG_FILES([lib/Makefile
                 lib/libdgconfig/Makefile
                 lib/libdgmail/Makefile
                 lib/libdgstr/Makefile
                 Makefile
                 so/Makefile
                 so/libmaildrop/Makefile
                 so/libgzip/Makefile
                 so/liblda/Makefile
                 so/libenczip/Makefile])
AC_OUTPUT
