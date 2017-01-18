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
		    Common.cpp \
		    Common.h \
		    Sequence.cpp \
		    Sequence.h \
		    EbusSequence.cpp \
		    EbusSequence.h \
		    fsm/EbusMessage.cpp \
		    fsm/EbusMessage.h \
		    fsm/State.cpp \
		    fsm/State.h \
		    fsm/OnError.cpp \
		    fsm/OnError.h \
		    fsm/Idle.cpp \
		    fsm/Idle.h \
		    fsm/Connect.cpp \
		    fsm/Connect.h \
		    fsm/Listen.cpp \
		    fsm/Listen.h \
		    fsm/LockBus.cpp \
		    fsm/LockBus.h \
		    fsm/FreeBus.cpp \
		    fsm/FreeBus.h \
		    fsm/EvalMessage.cpp \
		    fsm/EvalMessage.h \
		    fsm/SendMessage.cpp \
		    fsm/SendMessage.h \
		    fsm/RecvMessage.cpp \
		    fsm/RecvMessage.h \
		    fsm/SendResponse.cpp \
		    fsm/SendResponse.h \
		    fsm/RecvResponse.cpp \
		    fsm/RecvResponse.h \
		    fsm/IEbusProcess.cpp \
		    fsm/IEbusProcess.h \
		    fsm/IEbusLogger.h \
		    fsm/EbusFSM.cpp \
		    fsm/EbusFSM.h

distclean-local:
	-rm -f Makefile.in
