AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/ebusgate/network \
	      -isystem$(top_srcdir)/src/ebusgate/ebus \
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
		   ebus/Action.cpp \
		   ebus/Action.h \
		   ebus/Relation.cpp \
		   ebus/Relation.h \
		   ebus/Filter.cpp \
		   ebus/Filter.h \
		   ebus/Host.cpp \
		   ebus/Host.h \
		   ebus/DataHandler.cpp \
		   ebus/DataHandler.h \
		   ebus/EbusMessage.cpp \
		   ebus/EbusMessage.h \
		   ebus/State.cpp \
		   ebus/State.h \
		   ebus/OnError.cpp \
		   ebus/OnError.h \
		   ebus/Idle.cpp \
		   ebus/Idle.h \
		   ebus/Connect.cpp \
		   ebus/Connect.h \
		   ebus/Listen.cpp \
		   ebus/Listen.h \
		   ebus/LockBus.cpp \
		   ebus/LockBus.h \
		   ebus/FreeBus.cpp \
		   ebus/FreeBus.h \
		   ebus/Evaluate.cpp \
		   ebus/Evaluate.h \
		   ebus/SendMessage.cpp \
		   ebus/SendMessage.h \
		   ebus/RecvMessage.cpp \
		   ebus/RecvMessage.h \
		   ebus/SendResponse.cpp \
		   ebus/SendResponse.h \
		   ebus/RecvResponse.cpp \
		   ebus/RecvResponse.h \
		   ebus/EbusHandler.cpp \
		   ebus/EbusHandler.h \
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
