AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/lib/ebus

noinst_PROGRAMS = test_ebusdevice \
		  test_ebussequence

test_ebusdevice_SOURCES = test_ebusdevice.cpp
test_ebusdevice_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_ebussequence_SOURCES = test_ebussequence.cpp
test_ebussequence_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
