AC_PREREQ([2.65])
AC_INIT([ebusgate], [0.0.1], [roland.jax@liwest.at], [ebusgate], [https://github.com/yuhu-/ebusgate])

case "$@" in
	*"--sysconfdir="*) ;;
	*) sysconfdir=/etc ;;
esac

if test -z $CXXFLAGS;
	then CXXFLAGS='-g -O2'
fi
AC_PROG_CXX

AC_CONFIG_AUX_DIR([build])

AC_CHECK_HEADERS([arpa/inet.h \
		  dirent.h \
		  fcntl.h \
		  netdb.h \
		  poll.h \
		  sys/ioctl.h \
		  sys/time.h \
		  sys/socket.h \
		  termios.h])

AC_CONFIG_SRCDIR([src/ebusgate/EbusGate.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
		 src/lib/utils/Makefile
		 src/lib/logger/Makefile
		 src/lib/network/Makefile
		 src/lib/ebus/Makefile
		 src/lib/ebus/test/Makefile
		 src/ebusgate/Makefile
		 src/tools/Makefile])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_RANLIB

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_OUTPUT
