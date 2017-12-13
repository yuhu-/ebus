AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14 \
	      -isystem$(top_srcdir)/src \
	      -isystem$(top_srcdir)/include/ebusfsm

noinst_PROGRAMS = demo_ebusdevice \
		  demo_ebussequence \
		  demo_ebusfsm \
		  demo_ebusdata

demo_ebusdevice_SOURCES = demo_ebusdevice.cpp
demo_ebusdevice_LDADD = ../src/libebusfsm.la

demo_ebussequence_SOURCES = demo_ebussequence.cpp
demo_ebussequence_LDADD = ../src/libebusfsm.la

demo_ebusfsm_SOURCES = demo_ebusfsm.cpp
demo_ebusfsm_LDADD = ../src/libebusfsm.la \
		     -lpthread

demo_ebusdata_SOURCES = demo_ebusdata.cpp
demo_ebusdata_LDADD = ../src/libebusfsm.la

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
