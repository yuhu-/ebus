AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/include/ebusfsm

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

distclean-local:
	-rm -f Makefile.in
