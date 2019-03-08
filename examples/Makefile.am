AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wpedantic \
	      -std=c++17 \
	      -isystem$(top_srcdir)/src \
	      -isystem$(top_srcdir)/include/ebusfsm

noinst_PROGRAMS = demo_ebusdevice \
		  demo_ebussequence \
		  demo_ebusfsm

demo_ebusdevice_SOURCES = demo_ebusdevice.cpp
demo_ebusdevice_LDADD = ../src/libebusfsm.la
demo_ebusdevice_LDFLAGS = -no-install

demo_ebussequence_SOURCES = demo_ebussequence.cpp
demo_ebussequence_LDADD = ../src/libebusfsm.la
demo_ebussequence_LDFLAGS = -no-install

demo_ebusfsm_SOURCES = demo_ebusfsm.cpp
demo_ebusfsm_LDADD = ../src/libebusfsm.la \
		     -lpthread
demo_ebusfsm_LDFLAGS = -no-install

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs
