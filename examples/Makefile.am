AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++17 \
	      -isystem$(top_srcdir)/src \
	      -isystem$(top_srcdir)/include/ebus

noinst_PROGRAMS = demo_ebus

demo_ebus_SOURCES = demo_ebus.cpp
demo_ebus_LDADD = ../src/libebus.la \
		  -lpthread
demo_ebus_LDFLAGS = -no-install

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
