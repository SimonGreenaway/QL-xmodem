/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2001-2022 Georges Menie (www.menie.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#define TEST_XMODEM_RECEIVE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* this code needs standard functions memcpy() and memset()
   and input/output functions _inbyte() and _outbyte().

   the prototypes of the input/output functions are:
     int _inbyte(unsigned short timeout); // msec timeout
     void _outbyte(int c);

 */

#include "xmodem.h"
#include "crc16.h"
#include "os.h"

#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18
#define CTRLZ 0x1A

#define DLY_1S 1000
#define MAXRETRANS 25

static int check(int crc, const unsigned char *buf, int sz)
{
	if(crc)
	{
		unsigned short crc = crc16_ccitt(buf, sz);
		unsigned short tcrc = (buf[sz]<<8)+buf[sz+1];

		if (crc == tcrc) return 1;
	}
	else
	{
		int i;
		unsigned char cks = 0;

		for(i=0;i<sz;++i)
		{
			cks+=buf[i];
		}

		if (cks == buf[sz]) return 1;
	}

	return 0;
}

static void flushinput(void)
{
	while (_inbyte(((DLY_1S)*3)>>1) >= 0);
}

int xmodemReceive(unsigned char *dest, int destsz)
{
	unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	unsigned char *p;
	int bufsz, crc = 0;
	unsigned char trychar = 'C';
	unsigned char packetno = 1;
	int i, c, len = 0;
	int retry, retrans = MAXRETRANS;

	for(;;)
	{
		for(retry=0;retry<16;++retry)
		{
			if(trychar)_outbyte(trychar);

			if((c=_inbyte((DLY_1S)<<1))>=0)
			{
				switch(c)
				{
					case SOH:
						bufsz = 128;
						goto start_recv;
					case STX:
						bufsz = 1024;
						goto start_recv;
					case EOT:
						flushinput();
						_outbyte(ACK);
						return len; /* normal end */
					case CAN:
						if((c=_inbyte(DLY_1S))==CAN)
						{
							flushinput();
							_outbyte(ACK);
							return -1; /* canceled by remote */
						}
						break;
					default:
						break;
				}
			}
		}

		if(trychar=='C')
		{
			trychar=NAK;continue;
		}

		flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);

		return -2; /* sync error */

		start_recv:

		if (trychar == 'C') crc = 1;

		trychar = 0;
		p = xbuff;
		*p++ = c;

		for(i=0;i<(bufsz+(crc?1:0)+3); ++i)
		{
			if((c=_inbyte(DLY_1S))<0) goto reject;
			*p++ = c;
		}

		if(xbuff[1] == (unsigned char)(~xbuff[2]) && 
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno-1) &&
			check(crc, &xbuff[3], bufsz))
		{
			if (xbuff[1] == packetno)
			{
				register int count = destsz - len;

				if (count > bufsz) count = bufsz;

				if (count > 0)
				{
					memcpy (&dest[len], &xbuff[3], count);
					len += count;
				}

				++packetno;
				retrans = MAXRETRANS+1;
			}

			if (--retrans <= 0)
			{
				flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				return -3; /* too many retry error */
			}

			_outbyte(ACK);
			continue;
		}

		reject:
			flushinput();
			_outbyte(NAK);
	}
}

int xmodemTransmit(unsigned char *src, int srcsz)
{
	unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	int bufsz, crc = -1;
	unsigned char packetno = 1;
	int i, c, len = 0;
	int retry;

	for(;;)
	{
		for( retry = 0; retry < 16; ++retry)
		{
			if ((c = _inbyte((DLY_1S)<<1)) >= 0)
			{
				switch (c)
				{
					case 'C':
						crc = 1;
						goto start_trans;
					case NAK:
						crc = 0;
						goto start_trans;
					case CAN:
						if ((c = _inbyte(DLY_1S)) == CAN) {
							_outbyte(ACK);
							flushinput();
							return -1; /* canceled by remote */
						}
						break;
					default:
						break;
				}
			}
		}

		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		flushinput();

		return -2; /* no sync */

		for(;;)
		{
			start_trans:

			xbuff[0] = SOH; bufsz = 128;
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;
			c = srcsz - len;

			if (c > bufsz) c = bufsz;

			if(c>=0)
			{
				memset (&xbuff[3], 0, bufsz);

				if(c==0)
				{
					xbuff[3] = CTRLZ;
				}
				else
				{
					memcpy (&xbuff[3], &src[len], c);
					if (c < bufsz) xbuff[3+c] = CTRLZ;
				}

				if(crc)
				{
					unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);

					xbuff[bufsz+3] = (ccrc>>8) & 0xFF;
					xbuff[bufsz+4] = ccrc & 0xFF;
				}
				else
				{
					unsigned char ccks = 0;

					for (i = 3; i < bufsz+3; ++i)
					{
						ccks += xbuff[i];
					}

					xbuff[bufsz+3] = ccks;
				}

				for (retry = 0; retry < MAXRETRANS; ++retry)
				{
					for (i = 0; i < bufsz+4+(crc?1:0); ++i)
					{
						_outbyte(xbuff[i]);
					}

					if ((c = _inbyte(DLY_1S)) >= 0 )
					{
						switch (c)
						{
							case ACK:
								++packetno;
								len += bufsz;
								goto start_trans;
							case CAN:
								if ((c = _inbyte(DLY_1S)) == CAN) {
										_outbyte(ACK);
									flushinput();
									return -1; /* canceled by remote */
									}
								break;
							case NAK:
							default:
								break;
						}
					}
				}

				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				flushinput();

				return -4; /* xmit error */
			}
			else
			{
				for (retry = 0; retry < 10; ++retry)
				{
					_outbyte(EOT);
					if ((c = _inbyte((DLY_1S)<<1)) == ACK) break;
				}

				flushinput();
				return (c == ACK)?len:-5;
			}
		}
	}
}

#define BUFFERSIZE 8192

#define READ 1
#define WRITE 2

int main(int argc,char *argv[])
{
	#ifdef linux
	char *serialPath="/dev/ttyUSB0";
	#else // QDOS
	char *serialPath="ser1";
	#endif

	unsigned int baud=9600,size=0;
	char *file=NULL;
	unsigned int read=0;

	int st;
	unsigned char *buffer=NULL;

	for(st=1;st<argc;st++)
	{
		if(strcmp(argv[st],"-p")==0)
			serialPath=argv[++st];
		else if(strcmp(argv[st],"-b")==0)
			baud=atoi(argv[++st]);
		else if(strcmp(argv[st],"-d")==0)
			file=argv[++st];
		else if(strcmp(argv[st],"-s")==0)
			size=atoi(argv[++st]);
		else if(strcmp(argv[st],"-r")==0)
			read=READ;
		else if(strcmp(argv[st],"-w")==0)
			read=WRITE;
		else
		{
			puts("Usage: xmodem [-b baud] [-b serial_device] [-f filename] [-r | -w -s size]");
			exit(0);
		}
	}

	if(read==READ)
	{
		struct stat st;
		stat(file, &st);
		size = st.st_size;
	}
	else if(read==0)
	{
		puts("ERROR: Please specify -r or -w");
		exit(2);
	}

	buffer=(unsigned char *)malloc(size);

	if(!openSerial(serialPath,baud))
	{
		fprintf(stderr,"ERROR: Cannot open serial device '%s'\n",serialPath);
		exit(1);
	}

	if(read==READ) // READ
	{
		FILE *out;

		fprintf (stderr,"Send data using the xmodem protocol from your terminal emulator now...\n");

		st = xmodemReceive(buffer,size);

		if(st<0)
		{
			fprintf(stderr,"Xmodem receive error: status: %d\n", st);
			exit(3);
		}
		else
		{
			fprintf(stderr,"Xmodem successfully received %d bytes\n", st);
		}
	
		closeSerial();

		// Save date to file or send to stdout
	
		out=file==NULL?stdout:fopen(file, "wb");

		if(out!=NULL)
		{
			size_t to_go = size;

			while(to_go>0)
			{
				const size_t wrote=fwrite(buffer, to_go, 1, out);

				if(wrote==0) break;

				to_go-=wrote;
			}

			fclose(out);
		}	
		
		free(buffer);
	
		return 0;
	}
	else // if(read==WRITE)
	{
                FILE *out=fopen(file, "rb");

                if(out!=NULL)
                {
                        size_t to_go = size;

                        while(to_go>0)
                        {
                                const size_t read=fread(buffer, to_go, 1, out);

                                if(read==0) break;

                                to_go-=read;
                        }

                        fclose(out);
                }

		fprintf(stderr,"Prepare your terminal emulator to receive data now...\n");

		/* the following should be changed for your environment:
		   0x30000 is the download address,
		   12000 is the maximum size to be send from this address
		 */

		st = xmodemTransmit(buffer,BUFFERSIZE);

		if (st < 0)
		{
			fprintf(stderr,"Xmodem transmit error: status: %d\n", st);
			exit(4);
		}
		else
		{
			fprintf(stderr,"Xmodem successfully transmitted %d bytes\n", st);
		}
	
		closeSerial();
		free(buffer);
	
		return 0;
	}
}
