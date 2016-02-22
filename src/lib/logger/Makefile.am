AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/lib/utils

noinst_LIBRARIES = liblogger.a

liblogger_a_SOURCES = LogMessage.cpp \
		      LogMessage.h \
		      LogSink.h \
		      LogConsole.cpp \
		      LogConsole.h \
		      LogFile.cpp \
		      LogFile.h \
		      LogHandler.cpp \
		      LogHandler.h \
		      Logger.cpp \
		      Logger.h

distclean-local:
	-rm -f Makefile.in
