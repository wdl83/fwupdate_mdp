all: fwupdate.Makefile
	make -f fwupdate.Makefile

clean:
	rm *.o -f
	rm *.elf -f
