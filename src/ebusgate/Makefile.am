AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/ebusgate/network \
	      -isystem$(top_srcdir)/src/ebusgate/ebus/fsm \
	      -isystem$(top_srcdir)/src/ebusgate/ebus \
	      -isystem$(top_srcdir)/src/ebusgate/process/forward \
	      -isystem$(top_srcdir)/src/ebusgate/process \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/logger \
	      -isystem$(top_srcdir)/src/lib/network \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusgate

ebusgate_SOURCES = network/NetMessage.cpp \
		   network/NetMessage.h \
		   network/TCPConnection.cpp \
		   network/TCPConnection.h \
		   network/TCPAcceptor.cpp \
		   network/TCPAcceptor.h \
		   network/UDPReceiver.cpp \
		   network/UDPReceiver.h \
		   network/NetworkHandler.cpp \
		   network/NetworkHandler.h \
		   ebus/fsm/State.cpp \
		   ebus/fsm/State.h \
		   ebus/fsm/OnError.cpp \
		   ebus/fsm/OnError.h \
		   ebus/fsm/Idle.cpp \
		   ebus/fsm/Idle.h \
		   ebus/fsm/Connect.cpp \
		   ebus/fsm/Connect.h \
		   ebus/fsm/Listen.cpp \
		   ebus/fsm/Listen.h \
		   ebus/fsm/LockBus.cpp \
		   ebus/fsm/LockBus.h \
		   ebus/fsm/FreeBus.cpp \
		   ebus/fsm/FreeBus.h \
		   ebus/fsm/Evaluate.cpp \
		   ebus/fsm/Evaluate.h \
		   ebus/fsm/SendMessage.cpp \
		   ebus/fsm/SendMessage.h \
		   ebus/fsm/RecvMessage.cpp \
		   ebus/fsm/RecvMessage.h \
		   ebus/fsm/SendResponse.cpp \
		   ebus/fsm/SendResponse.h \
		   ebus/fsm/RecvResponse.cpp \
		   ebus/fsm/RecvResponse.h \
		   ebus/fsm/EbusFSM.cpp \
		   ebus/fsm/EbusFSM.h \
		   ebus/EbusMessage.cpp \
		   ebus/EbusMessage.h \
		   ebus/EbusHandler.cpp \
		   ebus/EbusHandler.h \
		   process/forward/Relation.cpp \
		   process/forward/Relation.h \
		   process/forward/Filter.cpp \
		   process/forward/Filter.h \
		   process/forward/Host.cpp \
		   process/forward/Host.h \
		   process/forward/ForwardHandler.cpp \
		   process/forward/ForwardHandler.h \
		   process/Process.h \
		   process/ProcessHandler.cpp \
		   process/ProcessHandler.h \
		   BaseLoop.cpp \
		   BaseLoop.h \
		   EbusGate.cpp

ebusgate_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
		 $(top_srcdir)/src/lib/logger/liblogger.a \
		 $(top_srcdir)/src/lib/network/libnetwork.a \
		 $(top_srcdir)/src/lib/ebus/libebus.a \
		 -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

install-data-local:
	[ -d $(sysconfdir)/ebusgate ] || mkdir -p $(sysconfdir)/ebusgate
