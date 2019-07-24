AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++17

lib_LTLIBRARIES = libebus.la

libebus_la_LDFLAGS = -version-info $(EBUS_SO_VERSION) \
			-lpthread

libebus_la_SOURCES = Device.cpp \
		     Sequence.cpp \
		     Telegram.cpp \
		     Message.cpp \
		     Ebus.cpp

EXTRA_DIST = Notify.h \
	     NQueue.h \
	     Average.h \
	     runtime_warning.h \
	     ILogger.h \
	     Device.h \
	     Sequence.h \
	     Telegram.h \
	     Message.h \
	     Ebus.h
	     	     

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
