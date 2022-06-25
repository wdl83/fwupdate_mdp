include Makefile.defs

TARGET = fwchecksum

CXXSRCS = \
	fwchecksum.cpp \
	ihex.cpp \
	modbus_tools/crc.cpp

include Makefile.rules
