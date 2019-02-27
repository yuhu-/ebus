SUBDIRS = utils

libebusfsmincludedir = $(includedir)/ebusfsm

libebusfsminclude_HEADERS = Device.h \
			    EbusDevice.h \
			    EbusCommon.h \
			    Sequence.h \
			    EbusSequence.h \
			    EbusMessage.h \
			    EbusFSM.h \
			    IEbusLogger.h

uninstall-hook:
	-rmdir $(libebusfsmincludedir)

distclean-local:
	-rm -f Makefile.in
