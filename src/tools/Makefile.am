AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/network \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusgatectl \
	       ebusgatefeed

ebusgatectl_SOURCES = ebusgatectl.cpp

ebusgatectl_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
		    $(top_srcdir)/src/lib/network/libnetwork.a \
		    $(top_srcdir)/src/lib/ebus/libebus.a

ebusgatefeed_SOURCES = ebusgatefeed.cpp

ebusgatefeed_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
		     $(top_srcdir)/src/lib/network/libnetwork.a \
		     $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

