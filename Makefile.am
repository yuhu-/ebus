SUBDIRS = src examples

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ebus.pc

distclean-local:
	-rm -rf autom4te.cache
	-rm -f aclocal.m4
	-rm -f config.h.in*
	-rm -f configure
	-rm -f Makefile.in
	-rm -rf build
	-rm -rf src/.deps
	-rm -rf m4
	-rm -rf *.tar.gz
