AC_PREREQ([2.69])

# Given a version number MAJOR.MINOR.PATCH, increment the:
# MAJOR version when you make incompatible API changes,
# MINOR version when you add functionality in a backwards-compatible manner, and
# PATCH version when you make backwards-compatible bug fixes.
AC_INIT([ebus],[0.0.0],[roland.jax@liwest.at],[ebus],[https://github.com/yuhu-/ebus])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

# Before making a release, the VERSION string should be modified. The
# string is of the form c:r:a. Follow these instructions sequentially:
#   1. If the library source code has changed at all since the last update, then
#      increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
#   2. If any interfaces have been added, removed, or changed since the last
#      update, increment current, and set revision to 0.
#   3. If any interfaces have been added since the last public release, then
#      increment age.
#   4. If any interfaces have been removed or changed since the last public
#      release, then set age to 0.
AC_SUBST([EBUS_SO_VERSION], [0:0:0])

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
