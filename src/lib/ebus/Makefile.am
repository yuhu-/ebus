AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -std=c++14

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = Device.cpp \
		    Device.h \
		    SerialDevice.cpp \
		    SerialDevice.h \
		    NetworkDevice.cpp \
		    NetworkDevice.h \
		    EbusDevice.cpp \
		    EbusDevice.h \
		    Common.cpp \
		    Common.h \
		    Sequence.cpp \
		    Sequence.h \
		    EbusSequence.cpp \
		    EbusSequence.h

distclean-local:
	-rm -f Makefile.in
