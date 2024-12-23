AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++14 \
	      -isystem$(top_srcdir)/include/ebus

lib_LTLIBRARIES = libebus.la

libebus_la_LDFLAGS = -version-info $(EBUS_SO_VERSION) \
		     -lpthread

libebus_la_SOURCES = Device.cpp \
		     Sequence.cpp \
		     Telegram.cpp \
		     Ebus.cpp

EXTRA_DIST = Device.h \
	     Sequence.h \
	     Telegram.h \
	     Notify.h \
	     NQueue.h \
	     runtime_warning.h

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
