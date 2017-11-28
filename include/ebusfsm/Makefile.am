SUBDIRS = base utils

libebusfsmincludedir = $(includedir)/ebusfsm

libebusfsminclude_HEADERS = EbusCommon.h \
			    EbusDevice.h \
			    EbusSequence.h \
			    EbusFSM.h \
			    IEbusLogger.h

EXTRA_DIST = EbusCommon.h \
	     EbusDevice.h \
	     EbusSequence.h \
	     EbusFSM.h \
	     IEbusLogger.h

uninstall-hook:
	-rmdir $(libebusfsmincludedir)

distclean-local:
	-rm -f Makefile.in
