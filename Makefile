ifndef OBJ_DIR
OBJ_DIR = ${PWD}/obj
export OBJ_DIR
endif

ifndef DST_DIR
DST_DIR = ${PWD}/dst
export DST_DIR
endif

all: \
	fwchecksum.Makefile \
	fwupdate.Makefile \
	zmqpp/Makefile
	make -C zmqpp
	mkdir -p ${OBJ_DIR}/zmqpp
	make PREFIX=${OBJ_DIR}/zmqpp install -C zmqpp
	make -f fwupdate.Makefile
	make -f fwchecksum.Makefile

install: \
	fwchecksum.Makefile \
	fwupdate.Makefile \
	zmqpp/Makefile
	make -C zmqpp
	mkdir -p ${OBJ_DIR}/zmqpp
	make PREFIX=${OBJ_DIR}/zmqpp install -C zmqpp
	make -f fwupdate.Makefile install
	make -f fwchecksum.Makefile install

clean:
	make -f fwupdate.Makefile clean

purge:
	rm $(OBJ_DIR) -rf
