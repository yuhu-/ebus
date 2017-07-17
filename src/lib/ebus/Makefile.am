AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/lib/utils

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = Device.cpp \
		    Device.h \
		    SerialDevice.cpp \
		    SerialDevice.h \
		    NetworkDevice.cpp \
		    NetworkDevice.h \
		    EbusDevice.cpp \
		    EbusDevice.h \
		    EbusCommon.cpp \
		    EbusCommon.h \
		    Sequence.cpp \
		    Sequence.h \
		    EbusSequence.cpp \
		    EbusSequence.h \
		    Message.cpp \
		    Message.h \
		    State.cpp \
		    State.h \
		    OnError.cpp \
		    OnError.h \
		    Idle.cpp \
		    Idle.h \
		    Connect.cpp \
		    Connect.h \
		    Listen.cpp \
		    Listen.h \
		    LockBus.cpp \
		    LockBus.h \
		    FreeBus.cpp \
		    FreeBus.h \
		    EvalMessage.cpp \
		    EvalMessage.h \
		    SendMessage.cpp \
		    SendMessage.h \
		    RecvMessage.cpp \
		    RecvMessage.h \
		    SendResponse.cpp \
		    SendResponse.h \
		    RecvResponse.cpp \
		    RecvResponse.h \
		    IEbusLogger.h \
		    EbusFSM.cpp \
		    EbusFSM.h

distclean-local:
	-rm -f Makefile.in
