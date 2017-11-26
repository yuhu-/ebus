AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14

lib_LTLIBRARIES = libebusfsm.la

libebusfsm_la_LDFLAGS = -version-info $(EBUSFSM_SO_VERSION) \
			-lpthread

libebusfsm_la_SOURCES = Notify.h \
			Color.h \
			NQueue.h \
			Message.cpp Message.h \
			EbusCommon.cpp EbusCommon.h \
			Device.cpp Device.h \
			SerialDevice.cpp SerialDevice.h \
			NetworkDevice.cpp NetworkDevice.h \
			EbusDevice.cpp EbusDevice.h \
			Sequence.cpp Sequence.h \
			EbusSequence.cpp EbusSequence.h \
			State.cpp State.h \
			OnError.cpp OnError.h \
			Idle.cpp Idle.h \
			Connect.cpp Connect.h \
			Listen.cpp Listen.h \
			LockBus.cpp LockBus.h \
			FreeBus.cpp FreeBus.h \
			EvalMessage.cpp EvalMessage.h \
			SendMessage.cpp SendMessage.h \
			RecvMessage.cpp RecvMessage.h \
			SendResponse.cpp SendResponse.h \
			RecvResponse.cpp RecvResponse.h \
			EbusFSM.cpp EbusFSM.h \
			IEbusLogger.h

libebusfsm_includedir = $(includedir)/ebusfsm-$(EBUSFSM_API_VERSION)

libebusfsm_include_HEADERS = \
			Notify.h \
			Color.h \
			NQueue.h \
			Device.h \
			Sequence.h \
			Message.h \
			EbusDevice.h \
			EbusCommon.h \
			EbusSequence.h \
			EbusFSM.h \
			IEbusLogger.h

uninstall-hook:
	-rmdir $(libebusfsm_includedir)

distclean-local:
	-rm -f Makefile.in
