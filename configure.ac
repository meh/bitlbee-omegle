AC_INIT([bitlbee-omegle], [0.1.0],
        [https://github.com/meh/bitlbee-omegle/issues])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_PREFIX_DEFAULT([/usr])

AM_INIT_AUTOMAKE()

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_ARG_WITH([plugindir],
            [AS_HELP_STRING([--with-plugindir],
                            [BitlBee plugin directory])],
            [plugindir="$with_plugindir"],
            [plugindir="$libdir/bitlbee"])

##########################################################################
# debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [enable debug data generation (def=no)])],
    [debugit="$enableval"],
    [debugit=no])
AC_MSG_RESULT([$debugit])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG],[],[Debug Mode])
    AM_CFLAGS="$AM_CFLAGS -g3 -Wall -Wno-uninitialized -O0"
else
    AC_DEFINE([NDEBUG],[],[No-debug Mode])
    AM_CFLAGS="$AM_CFLAGS -Os"
fi

##########################################################################

PKG_CHECK_MODULES([GLIB],    [glib-2.0])
PKG_CHECK_MODULES([BITLBEE], [bitlbee])
PKG_CHECK_MODULES([JANSSON], [jansson])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

LT_INIT([disable-static])

AC_SUBST([plugindir])
AC_SUBST([AM_CFLAGS])
AC_SUBST([LIBTOOL_DEPS])

AC_CONFIG_FILES([Makefile sources/Makefile])
AC_OUTPUT
