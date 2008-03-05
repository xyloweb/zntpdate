#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(zntpdate, 0.4.1, public.jmm@free.fr)
AM_INIT_AUTOMAKE([ -Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS(socket, socket)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# check settimeofday number of arguments 
AC_CACHE_CHECK(number of arguments to settimeofday(), ac_cv_func_settimeofday_nargs,
[AC_TRY_COMPILE([#include <sys/time.h>],[
settimeofday((struct timeval*)0,(struct timezone*)0);
],
	ac_cv_func_settimeofday_nargs=2, ac_cv_func_settimeofday_nargs=1)
])
if test $ac_cv_func_settimeofday_nargs = 1; then
	AC_DEFINE(SYSV_TIMEOFDAY, 1, [Does settimeofday take 1 arg?])
fi

# Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gethostbyname inet_ntoa memset socket strchr strerror])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	])
AC_OUTPUT
