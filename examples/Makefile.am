AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++17 \
	      -isystem$(top_srcdir)/src

noinst_PROGRAMS = demo_telegram \
		  demo_ebus

demo_telegram_SOURCES = demo_telegram.cpp
demo_telegram_LDADD = ../src/libebus.la
demo_telegram_LDFLAGS = -no-install

demo_ebus_SOURCES = demo_ebus.cpp
demo_ebus_LDADD = ../src/libebus.la \
		     -lpthread
demo_ebus_LDFLAGS = -no-install

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
