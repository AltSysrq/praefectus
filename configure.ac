#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([praefectus], [0.0.0], [jason@lin.gl])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT([dlopen])
AM_CONFIG_HEADER([config.h])
PKG_PROG_PKG_CONFIG()

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_CHECK_PROG([ASN1C], [asn1c], [asn1c], [])
AS_IF([test "x$ASN1C" = "x"],
      [AC_MSG_ERROR([cannot find "asn1c" in the path])])

# Checks for libraries.
AC_SEARCH_LIBS([errc], [c bsd])
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([dlopen], [dl])
PKG_CHECK_MODULES([CHECK], [check], [], [:])
# In theory, this should work, but maybe it won't.
# Leave it here for now, and cross that bridge when we eventually port to this
# platform.
AC_SEARCH_LIBS([CryptAcquireContextW], [advapi32])

# Headers
AC_CHECK_HEADERS([sys/queue.h bsd/sys/queue.h sys/tree.h bsd/sys/tree.h dnl
                  err.h bsd/err.h sysexits.h bsd/sysexits.h bsd/string.h])
# For Windows-specific crypto functions, since that platform doesn't have
# /dev/urandom and such.
AC_CHECK_HEADERS([windows.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT
AC_C_FLEXIBLE_ARRAY_MEMBER
AC_TYPE_SIZE_T

AC_CONFIG_FILES([Makefile src/Makefile src/asn1-support/Makefile
                 test/Makefile])
AC_OUTPUT
