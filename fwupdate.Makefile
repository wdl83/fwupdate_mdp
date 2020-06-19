include Makefile.defs

CFLAGS += $(DEFS)
CXXFLAGS += $(DEFS) 

TARGET = fwupdate

# CSRCS =

CXXSRCS = \
	../mdp/Client.cpp \
	../mdp/MutualHeartbeatMonitor.cpp \
	../mdp/ZMQClientContext.cpp \
	../mdp/ZMQIdentity.cpp \
	fwupdate.cpp \
	ihex.cpp

include Makefile.rules
