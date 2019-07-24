libebusincludedir = $(includedir)/ebus

libebusinclude_HEADERS = Ebus.h \
			 ILogger.h

EXTRA_DIST = Ebus.h \
	     ILogger.h

uninstall-hook:
	-rmdir $(libebusincludedir)

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
