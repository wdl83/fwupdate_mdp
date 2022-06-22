all: \
	fwupdate.Makefile \
	zmqpp/Makefile
	make -C zmqpp
	make PREFIX=${PWD}/zmqpp-root install -C zmqpp
	make -f fwupdate.Makefile

clean:
	make -f fwupdate.Makefile clean
