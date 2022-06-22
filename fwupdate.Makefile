include Makefile.defs

ZMQ = zmqpp-root

CXXFLAGS += \
	-I $(ZMQ)/include

LDFLAGS += \
	-L $(ZMQ)/lib \
	-lpthread \
	-lzmq \
	-lzmqpp

TARGET = fwupdate

# CSRCS =

CXXSRCS = \
	fwupdate.cpp \
	ihex.cpp \
	mdp/Client.cpp \
	mdp/MutualHeartbeatMonitor.cpp \
	mdp/ZMQClientContext.cpp \
	mdp/ZMQIdentity.cpp

include Makefile.rules
