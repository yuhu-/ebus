AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/include/ebusfsm

lib_LTLIBRARIES = libebusfsm.la

libebusfsm_la_LDFLAGS = -version-info $(EBUSFSM_SO_VERSION) \
			-lpthread

libebusfsm_la_SOURCES = Message.cpp \
			EbusCommon.cpp \
			Device.cpp \
			SerialDevice.cpp \
			NetworkDevice.cpp \
			EbusDevice.cpp  \
			Sequence.cpp \
			EbusSequence.cpp \
			State.cpp \
			OnError.cpp \
			Idle.cpp \
			Connect.cpp \
			Listen.cpp \
			LockBus.cpp \
			FreeBus.cpp \
			EvalMessage.cpp \
			SendMessage.cpp \
			RecvMessage.cpp \
			SendResponse.cpp \
			RecvResponse.cpp \
			EbusFSM.cpp

EXTRA_DIST = SerialDevice.h \
	     NetworkDevice.h \
	     State.h \
	     OnError.h \
	     Idle.h \
	     Connect.h \
	     Listen.h \
	     LockBus.h \
	     FreeBus.h \
	     EvalMessage.h \
	     SendMessage.h \
	     RecvMessage.h \
	     SendResponse.h \
	     RecvResponse.h


distclean-local:
	-rm -f Makefile.in
