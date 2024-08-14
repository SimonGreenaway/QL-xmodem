# Compile with 'make TARGET=qdos' to make qdos version

TARGET=linux
ARCH := $(shell arch)

COPT=-O3 -fomit-frame-pointer -Wall

ifeq ($(TARGET),qdos)
CC=qgcc qdos-gcc
ARCH=qdos
else ifeq ($(ARCH),aarch64)
CC=gcc
COPT=-O3 -fomit-frame-pointer -Wall -mtune=cortex-a72
LOPT=-Wl,-gc-sections
else
CC=gcc
LOPT=
endif

xmodem:	xmodem.o crc16.o os.o makefile os.h
	$(CC) $(LOPT) -o xmodem xmodem.o crc16.o os.o
	mv xmodem xmodem-$(ARCH)

lz:	lz.o
	$(CC) $(LOPT) -lm -o lz lz.o
	mv lz lz-$(ARCH)

xmodem.o:	xmodem.h xmodem.c crc16.h makefile
	$(CC) $(COPT) -c xmodem.c

crc16.o:	crc16.c crc16.h makefile
	$(CC) $(COPT) -c crc16.c

os.o:	os.c os.h makefile
	$(CC) $(COPT) -c os.c -D$(TARGET)

lz.o:	lz.c makefile
	$(CC) $(COPT) -c lz.c -D$(TARGET)

clean:
	rm -f xmodem.o os.o crc16.o lz.o

cleaner:	clean
	rm -f xmodem-* lz-*

git:	clean
	git add .
	git commit
	git push

dist:	xmodem
	cp xmodem-qdos /home/simon/emulators/ql/emulators/sQLux/flp1/xmodem
	cp ../QL-sprites/env.bin /home/simon/emulators/ql/emulators/sQLux/flp1/env_bin
	cp LICENSE /home/simon/emulators/ql/emulators/sQLux/flp1/LICENSE
	cd /home/simon/emulators/ql/emulators/sQLux && ./sqlux --SPEED=0 -b "LRESPR flp1_sigext_rext:EW flp1_zip,#1;'flp1_xmodem_zip xmodem LICENSE'"
	mv /home/simon/emulators/ql/emulators/sQLux/flp1/xmodem_zip xmodem.zip
	cp xmodem-qdos xmodem
	qltools xmodem.img -fdd xmodem -W
	cp ../QL-sprites/env.bin env_bin
	qltools xmodem.img -W env_bin xmodem LICENSE
	cp xmodem.zip i.zip
	mdvtool create name xmodem zip_import i.zip write xmodem.mdv 
	rm -f i.zip env_bin xmodem
