AC_PREREQ([2.69])
AC_INIT([ebusfsm],[0.0.0],[roland.jax@liwest.at],[ebusfsm],[https://github.com/yuhu-/ebusfsm])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

AC_SUBST([EBUSFSM_SO_VERSION], [0:0:0])
AC_SUBST([EBUSFSM_API_VERSION], [0.0])

if test -z $CXXFLAGS;
	then CXXFLAGS='-g -O2'
fi

AC_PROG_CXX
AM_PROG_AR

LT_INIT([disable-static])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_SRCDIR([src/EbusFSM.cpp])

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 include/Makefile
		 include/ebusfsm/Makefile
		 include/ebusfsm/utils/Makefile
		 examples/Makefile
		 ebusfsm.pc:ebusfsm.pc.in])

AC_OUTPUT
