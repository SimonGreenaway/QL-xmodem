TARGET=linux # qdos

ifeq ($(TARGET),qdos)
CC=qgcc qdos-gcc
OS=qdos.c
else
CC=gcc
OS=linux.c
endif

xmodem:	xmodem.o crc16.o os.o makefile
	$(CC) -o xmodem xmodem.o crc16.o os.o
	mv xmodem xmodem-$(TARGET)

xmodem.o:	xmodem.c crc16.h makefile
	$(CC) -c xmodem.c

crc16.o:	crc16.c crc16.h makefile
	$(CC) -c crc16.c

os.o:	$(OS) linux.h makefile
	$(CC) -o os.o -c $(OS)

clean:
	rm -f xmodem.o os.o crc16.o

git:	clean
	git add .
	git commit
	git push
