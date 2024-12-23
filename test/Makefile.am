AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src \
	      -isystem$(top_srcdir)/include/ebus

noinst_PROGRAMS = test_telegram

test_telegram_SOURCES = test_telegram.cpp
test_telegram_LDADD = ../src/libebus.la
test_telegram_LDFLAGS = -no-install

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
	-rm -rf .deps
