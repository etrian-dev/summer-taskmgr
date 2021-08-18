#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([summer-taskmgr], [1.0], [nicola.vetrini@gmail.com])
AC_CONFIG_SRCDIR([src/main.c])

# Use automake
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC

# glib check using pkg-config
PKG_CHECK_MODULES([DEPS], [glib-2.0])

# Checks for libraries.
# JSON parsing library libjansson
AC_CHECK_LIB([jansson], [json_object_get])
# Standard math library <math.h>
AC_CHECK_LIB([m], [round])
# ncurses library
AC_CHECK_LIB([ncurses], [initscr])
# POSIX threads library
AC_CHECK_LIB([pthread], [pthread_create])
# POSIX timers library
AC_CHECK_LIB([rt], [timer_create])

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h ctype.h math.h unistd.h signal.h glib.h jansson.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strcasecmp strdup strstr strtol])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
