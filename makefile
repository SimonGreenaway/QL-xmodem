# Compile with 'make TARGET=qdos' to make qdos version

TARGET=linux
ARCH := $(shell arch)

COPT=-O3 -fomit-frame-pointer -Wall

ifeq ($(TARGET),qdos)
CC=qgcc qdos-gcc
else
CC=gcc
endif

xmodem:	xmodem.o crc16.o os.o makefile os.h
	$(CC) -o xmodem xmodem.o crc16.o os.o
	mv xmodem xmodem-$(TARGET)-$(ARCH)

xmodem.o:	xmodem.h xmodem.c crc16.h makefile
	$(CC) $(COPT) -c xmodem.c

crc16.o:	crc16.c crc16.h makefile
	$(CC) $(COPT) -c crc16.c

os.o:	os.c os.h makefile
	$(CC) $(COPT) -c os.c -D$(TARGET)

clean:
	rm -f xmodem.o os.o crc16.o

git:	clean
	git add .
	git commit
	git push
