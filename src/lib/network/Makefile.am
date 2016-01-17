AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14

noinst_LIBRARIES = libnetwork.a

libnetwork_a_SOURCES = TCPSocket.cpp \
		       TCPSocket.h \
		       TCPClient.cpp \
		       TCPClient.h \
		       TCPServer.cpp \
		       TCPServer.h

distclean-local:
	-rm -f Makefile.in
