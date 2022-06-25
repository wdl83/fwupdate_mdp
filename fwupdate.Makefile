include Makefile.defs

TARGET = fwupdate

CXXSRCS = \
	fwupdate.cpp \
	ihex.cpp \
	mdp/Client.cpp \
	mdp/MutualHeartbeatMonitor.cpp \
	mdp/ZMQClientContext.cpp \
	mdp/ZMQIdentity.cpp

include Makefile.rules
