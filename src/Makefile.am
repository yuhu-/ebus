AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/include/ebusfsm

noinst_LIBRARIES = libebusfsm.a

libebusfsm_a_SOURCES = Device.cpp \
		       SerialDevice.cpp \
		       NetworkDevice.cpp \
		       EbusDevice.cpp \
		       EbusCommon.cpp \
		       Sequence.cpp \
		       EbusSequence.cpp \
		       Message.cpp \
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

libebusfsm_a_INCLUDES = utils/Notify.h \
			utils/Color.h \
			utils/NQueue.h \
			Device.h \
		        SerialDevice.h \
		        NetworkDevice.h \
		        EbusDevice.h \
		        Sequence.h \
		        EbusSequence.h \
		        Message.h \
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
		        RecvResponse.h \
		        IEbusLogger.h \
		        EbusFSM.h

distclean-local:
	-rm -f Makefile.in
