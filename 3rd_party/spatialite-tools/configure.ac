#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(spatialite-tools, 4.4.0-RC0, a.furieri@lqt.it)
AC_LANG(C)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

# supporting SPATIALITE_AMALGAMATION
AH_TEMPLATE([SPATIALITE_AMALGAMATION],
	    [must be defined when using libspatialite-amalgamation])
AH_TEMPLATE([OMIT_FREEXL],
            [Should be defined in order to disable FREEXL support.])
AH_TEMPLATE([OMIT_READOSM],
            [Should be defined in order to disable ReadOSM support.])
AH_TEMPLATE([ENABLE_LIBXML2],
            [Should be defined in order to enable LIBXML2 support.])
AH_TEMPLATE([HAVE_DECL_SQLITE_CONFIG_URI],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_USED],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_HIT],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_MISS_SIZE],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_MISS_FULL],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_CACHE_USED],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_CACHE_HIT],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_CACHE_MISS],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_CACHE_WRITE],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_SCHEMA_USED],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_STMT_USED],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_STMTSTATUS_FULLSCAN_STEP],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_STMTSTATUS_SORT],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_STMTSTATUS_AUTOINDEX],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_DBSTATUS_STMT_USED],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_TESTCTRL_EXPLAIN_STMT],
            [depending on SQLite library version.])
AH_TEMPLATE([HAVE_DECL_SQLITE_FCNTL_VFSNAME],
            [depending on SQLite library version.])

# Checks for header files.
AC_CHECK_HEADERS(stdlib.h,, [AC_MSG_ERROR([cannot find stdlib.h, bailing out])])
AC_CHECK_HEADERS(stdio.h,, [AC_MSG_ERROR([cannot find stdio.h, bailing out])])
AC_CHECK_HEADERS(string.h,, [AC_MSG_ERROR([cannot find string.h, bailing out])])
AC_CHECK_HEADERS(memory.h,, [AC_MSG_ERROR([cannot find memory.h, bailing out])])
AC_CHECK_HEADERS(math.h,, [AC_MSG_ERROR([cannot find math.h, bailing out])])
AC_CHECK_HEADERS(float.h,, [AC_MSG_ERROR([cannot find float.h, bailing out])])
AC_CHECK_HEADERS(fcntl.h,, [AC_MSG_ERROR([cannot find fcntl.h, bailing out])])
AC_CHECK_HEADERS(inttypes.h,, [AC_MSG_ERROR([cannot find inttypes.h, bailing out])])
AC_CHECK_HEADERS(stddef.h,, [AC_MSG_ERROR([cannot find stddef.h, bailing out])])
AC_CHECK_HEADERS(stdint.h,, [AC_MSG_ERROR([cannot find stdint.h, bailing out])])
AC_CHECK_HEADERS(sys/time.h,, [AC_MSG_ERROR([cannot find sys/time.h, bailing out])])
AC_CHECK_HEADERS(unistd.h,, [AC_MSG_ERROR([cannot find unistd.h, bailing out])])
AC_CHECK_HEADERS(sqlite3.h,, [AC_MSG_ERROR([cannot find sqlite3.h, bailing out])])
AC_CHECK_HEADERS(sqlite3ext.h,, [AC_MSG_ERROR([cannot find sqlite3ext.h, bailing out])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([memset sqrt strcasecmp strerror strncasecmp strstr fdatasync ftruncate getcwd gettimeofday localtime_r memmove strerror])
 
# Checks for installed libraries
AC_CHECK_LIB(sqlite3,sqlite3_prepare_v2,,AC_MSG_ERROR(['libsqlite3' is required but it doesn't seem to be installed on this system.]),-lm)

AC_CONFIG_FILES([Makefile])

#-----------------------------------------------------------------------
#   --enable-readline
#
AC_ARG_ENABLE(readline, [AS_HELP_STRING(
  [--enable-readline],
  [use readline in shell tool (yes, no) [default=yes]])],
  [], [enable_readline=yes])
if test x"$enable_readline" != xno ; then
  sLIBS=$LIBS
  LIBS=""
  AC_SEARCH_LIBS(tgetent, curses, [], [])
  AC_SEARCH_LIBS(readline, readline, [], [enable_readline=no])
  AC_CHECK_FUNCS(readline, [], [])
  READLINE_LIBS=$LIBS
  LIBS=$sLIBS
fi
AC_SUBST(READLINE_LIBS)

AC_CHECK_HEADERS(expat.h,, [AC_MSG_ERROR([cannot find expat.h, bailing out])])
AC_CHECK_LIB(expat,XML_ParserCreate,,AC_MSG_ERROR(['expat' is required but it doesn't seem to be installed on this system.]))
AC_CHECK_LIB(proj,pj_init_plus,,AC_MSG_ERROR(['libproj' is required but it doesn't seem to be installed on this system.]),-lm)

#-----------------------------------------------------------------------
#   --with-geosconfig
#
AC_ARG_WITH([geosconfig],
      [AS_HELP_STRING([--with-geosconfig=FILE], [specify an alternative geos-config file])],
	[GEOSCONFIG="$withval"], [GEOSCONFIG=""])
if test "x$GEOSCONFIG" = "x"; then
      # GEOSCONFIG was not specified, so search within the current path
      AC_PATH_PROG([GEOSCONFIG], [geos-config])	
      # If we couldn't find geos-config, display an error
      if test "x$GEOSCONFIG" = "x"; then
              AC_MSG_ERROR([could not find geos-config within the current path. You may need to try re-running configure with a --with-geosconfig parameter.])
      fi
else
      # GEOSCONFIG was specified; display a message to the user
      if test "x$GEOSCONFIG" = "xyes"; then
              AC_MSG_ERROR([you must specify a parameter to --with-geosconfig, e.g. --with-geosconfig=/path/to/geos-config])
      else
              if test -f $GEOSCONFIG; then
                      AC_MSG_RESULT([Using user-specified geos-config file: $GEOSCONFIG])
              else
                      AC_MSG_ERROR([the user-specified geos-config file $GEOSCONFIG does not exist])
              fi     
      fi
fi
# Extract the linker and include flags
GEOS_LDFLAGS=`$GEOSCONFIG --ldflags`
GEOS_CPPFLAGS=-I`$GEOSCONFIG --includes`
AC_SUBST([GEOS_LDFLAGS])
AC_SUBST([GEOS_CPPFLAGS])	
# Ensure that we can parse geos_c.h
CPPFLAGS_SAVE="$CPPFLAGS"
CPPFLAGS="$GEOS_CPPFLAGS"
AC_CHECK_HEADERS([geos_c.h],, [AC_MSG_ERROR([could not find geos_c.h - you may need to specify the directory of a geos-config file using --with-geosconfig])])
CPPFLAGS="$CPPFLAGS_SAVE"	
# Ensure we can link against libgeos_c
LIBS_SAVE="$LIBS"
LIBS="$GEOS_LDFLAGS"
AC_SEARCH_LIBS(GEOSTopologyPreserveSimplify,geos_c,,AC_MSG_ERROR([could not find libgeos_c - you may need to specify the directory of a geos-config file using --with-geosconfig]))
LIBS="$LIBS_SAVE"
LIBS=$LIBS$GEOS_LDFLAGS' -lgeos_c'

PKG_CHECK_MODULES([LIBSPATIALITE], [spatialite], , AC_MSG_ERROR(['libspatialite' is required but it doesn't seem to be installed on this system.]))
AC_SUBST(LIBSPATIALITE_CFLAGS)
# testing for libspatialite-amalgamation
if test "x$(pkg-config --cflags spatialite|grep "DSPATIALITE_AMALGAMATION=1")" != "x"; then
  AC_DEFINE(SPATIALITE_AMALGAMATION, 1)
fi
AC_SUBST(LIBSPATIALITE_LIBS)

#-----------------------------------------------------------------------
#   --enable-freexl
#
AC_ARG_ENABLE(freexl, [AS_HELP_STRING(
  [--enable-freexl], [enables FreeXL inclusion [default=yes]])],
  [], [enable_freexl=yes])
if test x"$enable_freexl" != "xno"; then
  PKG_CHECK_MODULES([LIBFREEXL], [freexl], , AC_MSG_ERROR(['libfreexl' is required but it doesn't seem to be installed on this system.]))
  AC_SUBST(LIBFREEXL_CFLAGS)
  AC_SUBST(LIBFREEXL_LIBS)
else
  AC_DEFINE(OMIT_FREEXL)
fi

#-----------------------------------------------------------------------
#   --enable-readosm
#
AC_ARG_ENABLE(readosm, [AS_HELP_STRING(
  [--enable-readosm], [enables ReadOSM inclusion [default=yes]])],
  [], [enable_readosm=yes])
if test x"$enable_readosm" != "xno"; then
  AC_SUBST(LIBREADOSM_LIBS)PKG_CHECK_MODULES([LIBREADOSM], [readosm], , AC_MSG_ERROR(['libreadosm' is required but it doesn't seem to be installed on this system.]))
  AC_SUBST(LIBREADOSM_CFLAGS)
  AC_SUBST(LIBREADOSM_LIBS)
else
  AC_DEFINE(OMIT_READOSM)
  NOREADOSM="true"
fi

AM_CONDITIONAL([NO_READOSM], [test "$NOREADOSM" = "true"])

#-----------------------------------------------------------------------
#   --enable-libxml2
#
AC_ARG_ENABLE(libxml2, [AS_HELP_STRING(
  [--enable-libxml2], [enables libxml2 inclusion [default=yes]])],
  [], [enable_libxml2=yes])
if test x"$enable_libxml2" != "xno"; then
  PKG_CHECK_MODULES([LIBXML2], [libxml-2.0], , AC_MSG_ERROR(['libxml2' is required but it doesn't seem to be installed on this system.]))
  AC_SUBST(LIBXML2_CFLAGS)
  AC_SUBST(LIBXML2_LIBS)
  AC_DEFINE(ENABLE_LIBXML2)
fi

# checks for SQLite version-depending constants
AC_CHECK_DECL([SQLITE_CONFIG_URI],
  [AC_DEFINE(HAVE_DECL_SQLITE_CONFIG_URI)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_LOOKASIDE_USED],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_USED)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_LOOKASIDE_HIT],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_HIT)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_LOOKASIDE_MISS_SIZE],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_MISS_SIZE)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_LOOKASIDE_MISS_FULL],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_LOOKASIDE_MISS_FULL)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_CACHE_USED],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_CACHE_USED)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_CACHE_HIT],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_CACHE_HIT)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_CACHE_MISS],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_CACHE_MISS)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_CACHE_WRITE],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_CACHE_WRITE)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_SCHEMA_USED],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_SCHEMA_USED)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_DBSTATUS_STMT_USED],
  [AC_DEFINE(HAVE_DECL_SQLITE_DBSTATUS_STMT_USED)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_STMTSTATUS_FULLSCAN_STEP],
  [AC_DEFINE(HAVE_DECL_SQLITE_STMTSTATUS_FULLSCAN_STEP)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_STMTSTATUS_SORT],
  [AC_DEFINE(HAVE_DECL_SQLITE_STMTSTATUS_SORT)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_STMTSTATUS_AUTOINDEX],
  [AC_DEFINE(HAVE_DECL_SQLITE_STMTSTATUS_AUTOINDEX)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_TESTCTRL_EXPLAIN_STMT],
  [AC_DEFINE(HAVE_DECL_SQLITE_TESTCTRL_EXPLAIN_STMT)],[],[[#include <sqlite3.h>]])
AC_CHECK_DECL([SQLITE_FCNTL_VFSNAME],
  [AC_DEFINE(HAVE_DECL_SQLITE_FCNTL_VFSNAME)],[],[[#include <sqlite3.h>]])

AC_OUTPUT
