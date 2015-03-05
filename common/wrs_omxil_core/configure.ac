#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([omxil-core], [0.5.0], [daniel.charles at intel.com])
AC_CONFIG_SRCDIR([ilcore/inc/khronos/openmax/include/OMX_Index.h])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

AC_ARG_ENABLE(omx_debug,
    [AC_HELP_STRING([--enable-omx-debug],
                    [build with extra debug symbols @<:@default=no@:>@])],
    [], [enable_omx_debug="no"])

if test "$enable_omx_debug" = "yes"; then
AC_DEFINE([__ENABLE_DEBUG__], [1], [Defined to 1 if extra debug symbols are compiled])
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_DISABLE_STATIC

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup])

AC_CONFIG_FILES([Makefile
                 ilcore/src/Makefile
                 base/src/Makefile
		 utils/src/Makefile
                 pkgconfig/Makefile])
AC_OUTPUT([pkgconfig/libomxil_base.pc
	   pkgconfig/libomxil_utils.pc])
