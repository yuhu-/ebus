AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/ebuscpp/network \
	      -isystem$(top_srcdir)/src/ebuscpp/forward \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/logger \
	      -isystem$(top_srcdir)/src/lib/network \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusproxy

ebusproxy_SOURCES = network/NetMessage.cpp \
		    network/NetMessage.h \
		    network/TCPConnection.cpp \
		    network/TCPConnection.h \
		    network/TCPAcceptor.cpp \
		    network/TCPAcceptor.h \
		    network/UDPReceiver.cpp \
		    network/UDPReceiver.h \
		    network/Network.cpp \
		    network/Network.h \
		    forward/Relation.cpp \
		    forward/Relation.h \
		    forward/Filter.cpp \
		    forward/Filter.h \
		    forward/Host.cpp \
		    forward/Host.h \
		    forward/Forward.cpp \
		    forward/Forward.h \
		    EbusLogger.cpp \
		    EbusLogger.h \
		    BaseLoop.cpp \
		    BaseLoop.h \
		    EbusProxy.cpp

ebusproxy_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
		  $(top_srcdir)/src/lib/logger/liblogger.a \
		  $(top_srcdir)/src/lib/network/libnetwork.a \
		  $(top_srcdir)/src/lib/ebus/libebus.a \
		  -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

install-data-local:
	[ -d $(sysconfdir)/ebuscpp ] || mkdir -p $(sysconfdir)/ebuscpp
