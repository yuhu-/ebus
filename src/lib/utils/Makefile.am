AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = Daemon.cpp \
		     Daemon.h \
		     Options.cpp \
		     Options.h \
		     NQueue.h \
		     PipeNotify.h \
		     Notify.h \
		     Color.h

distclean-local:
	-rm -f Makefile.in
