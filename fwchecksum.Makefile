include Makefile.defs

CFLAGS += $(DEFS)
CXXFLAGS += $(DEFS) 

TARGET = fwchecksum

# CSRCS =

CXXSRCS = \
	../modbus_mdp/crc.cpp \
	fwchecksum.cpp \
	ihex.cpp

include Makefile.rules
