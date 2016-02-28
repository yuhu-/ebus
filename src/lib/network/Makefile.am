AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14

noinst_LIBRARIES = libnetwork.a

libnetwork_a_SOURCES = Socket.cpp \
		       Socket.h \
		       Client.cpp \
		       Client.h \
		       Server.cpp \
		       Server.h

distclean-local:
	-rm -f Makefile.in
