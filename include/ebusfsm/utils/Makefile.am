libebusfsmincludedir = $(includedir)/ebusfsm/utils

libebusfsminclude_HEADERS = Color.h \
			    Notify.h \
			    NQueue.h

EXTRA_DIST = Color.h \
	     Notify.h \
	     NQueue.h

uninstall-hook:
	-rmdir $(libebusfsmincludedir)

distclean-local:
	-rm -f Makefile.in
