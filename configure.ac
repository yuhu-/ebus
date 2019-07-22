AC_PREREQ([2.69])
AC_INIT([ebus],[0.1.0],[roland.jax@liwest.at],[ebus],[https://github.com/yuhu-/ebus])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

AC_SUBST([EBUS_SO_VERSION], [0:1:0])
AC_SUBST([EBUS_API_VERSION], [0.1])

if test -z $CXXFLAGS;
	then CXXFLAGS='-g -O2'
fi

AC_PROG_CXX
AM_PROG_AR

LT_INIT([disable-static])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_SRCDIR([src/Ebus.cpp])

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 examples/Makefile
		 ebus.pc:ebus.pc.in])

AC_OUTPUT
