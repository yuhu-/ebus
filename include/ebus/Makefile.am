libebusincludedir = $(includedir)/ebus

libebusinclude_HEADERS = Ebus.h

EXTRA_DIST = Ebus.h

uninstall-hook:
	-rmdir $(libebusincludedir)

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
