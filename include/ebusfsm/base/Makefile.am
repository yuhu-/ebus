libebusfsmincludedir = $(includedir)/ebusfsm/base

libebusfsminclude_HEADERS = Message.h \
			    Device.h \
			    Sequence.h

EXTRA_DIST = Message.h \
	     Device.h \
	     Sequence.h

uninstall-hook:
	-rmdir $(libebusfsmincludedir)

distclean-local:
	-rm -f Makefile.in
