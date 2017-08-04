AC_PREREQ([2.68])
AC_INIT([ebusfsm], [0.1.0], [roland.jax@liwest.at], [ebusfsm], [https://github.com/yuhu-/ebusfsm])

if test -z $CXXFLAGS;
	then CXXFLAGS='-g -O2'
fi
AC_PROG_CXX

AC_CONFIG_AUX_DIR([build])

AC_CONFIG_SRCDIR([src/EbusFSM.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
		 src/Makefile
		 example/Makefile])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_RANLIB

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_OUTPUT
