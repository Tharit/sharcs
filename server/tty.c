/*
 * Copyright (c) 2012 Martin Kleinhans <mail@mkleinhans.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/select.h>

#include <ftdi.h>
 
enum {
 	AV_MODE_STOPPED,
 	AV_MODE_TTY,
	AV_MODE_LIBFTDI,
};

#define BUFFER_SIZE 128

struct tty_context {
	int fd,pipeFD[2],mode,ctrlChar;
	struct termios options;
	struct ftdi_context ftdic;
	void (*callback)(const char*,int );
	
	// buffers
	char readBuffer[BUFFER_SIZE], writeBuffer[BUFFER_SIZE],error[256];
	int readCounter, writeCounter;
	pthread_mutex_t mutex_write;// = PTHREAD_MUTEX_INITIALIZER;
};

/*------------------------------------------------------
 * functions
 *------------------------------------------------------*/
	 
int tty_main_tty(struct tty_context *ctx);
int tty_main_libftdi(struct tty_context *ctx);
	
const char* tty_error(struct tty_context *ctx) {
	if(ctx->error[0]==0) {
		return NULL;
	}
	return ctx->error;
}


struct tty_context *tty_init_internal(void (*cb)(const char*,int)) {
	struct tty_context *ctx;
	
	ctx = (struct tty_context*)malloc(sizeof(struct tty_context));
	
	ctx->callback = cb;
	ctx->ctrlChar = 0x0A;
	
	pipe(ctx->pipeFD);
	
	ctx->readCounter = 0;
	ctx->writeCounter = 0;
	
	pthread_mutex_init(&ctx->mutex_write, NULL);
	
	return ctx;
}

int tty_set_event_char(struct tty_context *ctx,char e) {
	ctx->ctrlChar = e;
	
	if(ctx->mode == AV_MODE_LIBFTDI) {
		ftdi_set_event_char(&ctx->ftdic,ctx->ctrlChar,1);
	}
	
	return 1;
}

struct tty_context *tty_init_libftdi(int vendor,int product,const char *description,const char *serial,unsigned int index,void (*cb)(const char*,int)) {
	int ret;
	
	struct tty_context *ctx;
	ctx = tty_init_internal(cb);
	
	if (ftdi_init(&ctx->ftdic) < 0) {
    	sprintf(ctx->error, "ftdi_init failed\n");
    	return 0;
 	}
	
	if ((ret = ftdi_usb_open_desc_index(&ctx->ftdic, vendor,product,description,serial,index)) < 0) {
    	sprintf(ctx->error, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ctx->ftdic));
    	return 0;
	}

	ftdi_usb_purge_rx_buffer(&ctx->ftdic);
	ftdi_set_latency_timer(&ctx->ftdic,40);
	ftdi_set_event_char(&ctx->ftdic,ctx->ctrlChar,1);

	// set tty_main implementation
	ctx->mode = AV_MODE_LIBFTDI;
	
	return ctx;
}

struct tty_context *tty_init_tty(const char *devicename,void (*cb)(const char*,int)) {
	struct tty_context *ctx;
	ctx = tty_init_internal(cb);
	
	ctx->fd = open(devicename,O_RDWR | O_NOCTTY | O_NDELAY);
	if (ctx->fd < 0) {
		sprintf(ctx->error,"%s - %s",devicename,strerror(errno));
		fprintf(stderr,ctx->error);
		return 0;
	}
	
	tcgetattr(ctx->fd, &ctx->options);
	cfsetispeed(&ctx->options, B9600);
	
	ctx->options.c_cflag |= (CLOCAL | CREAD);
	ctx->options.c_cflag &= ~PARENB;
	ctx->options.c_cflag &= ~CSTOPB;
	ctx->options.c_cflag &= ~CSIZE;
	ctx->options.c_cflag |= CS8;
	
	tcsetattr(ctx->fd, TCSANOW, &ctx->options);
	tcflush(ctx->fd,TCIOFLUSH);
	
	// set tty_main implementation
	ctx->mode = AV_MODE_TTY;
	
	return ctx;
}

int tty_busy(struct tty_context *ctx) {
	return ctx->writeCounter>0||ctx->readCounter>0;
}

int tty_stop(struct tty_context *ctx) {
	if(ctx->mode == AV_MODE_STOPPED) {
		return 0;
	}
	
	if(ctx->mode == AV_MODE_TTY && ctx->fd>=0) {
		tcsetattr(ctx->fd,TCSANOW,&ctx->options);
		close(ctx->fd);
		ctx->fd = -1;
	} else if(ctx->mode == AV_MODE_LIBFTDI) {
		int ret = 0;
		if ((ret = ftdi_usb_close(&ctx->ftdic)) < 0) {
			sprintf(ctx->error, "unable to close ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ctx->ftdic));
			return 0;
		}

		ftdi_deinit(&ctx->ftdic);
	}
	
	close(ctx->pipeFD[0]);
	close(ctx->pipeFD[1]);
	
	ctx->mode = AV_MODE_STOPPED;
	
	return 1;
}

int tty_send(struct tty_context *ctx,const char *buf) {
	int n;
	
	n = strlen(buf);
	
	pthread_mutex_lock( &ctx->mutex_write );
	
	if(ctx->writeCounter+n>BUFFER_SIZE) {
		fprintf(stderr,"tty: write buffer full, command skipped!\n");
		pthread_mutex_unlock( &ctx->mutex_write );
		return 0;
	}
	memcpy(ctx->writeBuffer+ctx->writeCounter,buf,n);
	ctx->writeCounter+=n;
	
	pthread_mutex_unlock( &ctx->mutex_write );
	
	write(ctx->pipeFD[1], ".", 1);
	
	return 1;
}

int tty_recv(struct tty_context *ctx,const char *s) {
	ctx->callback(s,strlen(s));
	return 1;
}

int tty_main(struct tty_context *ctx) {
	if(ctx->mode == AV_MODE_LIBFTDI) {
		return tty_main_libftdi(ctx);
	} else {
		return tty_main_tty(ctx);
	}
}

int tty_main_libftdi(struct tty_context *ctx) {
	int ret,i,s,c;
	char buf[64];
	
	if(ctx->mode != AV_MODE_LIBFTDI) {
		return 0;
	}
	
	/* read data */
	ret = ftdi_read_data(&ctx->ftdic,(unsigned char*)(ctx->readBuffer+ctx->readCounter),BUFFER_SIZE-ctx->readCounter);
	if(ret > 0) {
		ctx->readCounter += ret;

		/* check for complete messages */
		for(s=0,i=0;i<ctx->readCounter;i++) {
			if(ctx->readBuffer[i] == ctx->ctrlChar) {
				memcpy(buf,ctx->readBuffer+s,i-s);
				buf[i-s] = 0x0;
				tty_recv(ctx,buf);
				s = i+1;
			}
		}
		
		if(s>=ctx->readCounter) {
			ctx->readCounter = 0;
		} else if(s>0) {
			memmove(ctx->readBuffer,ctx->readBuffer+s,ctx->readCounter-s);
			ctx->readCounter-=s;
		}
		return 1;
	} else if(ret < 0) {
		sprintf(ctx->error,"read() failed");
		tty_stop(ctx);
		
		return 0;
	}
	
	/* write data */
	if(ctx->writeCounter>0) {
		// find length of first command
		for(c=0;c<ctx->writeCounter;c++) {
			if(ctx->writeBuffer[c] == '\n') {
				c++;
				break;
			}
		}
		
		pthread_mutex_lock( &ctx->mutex_write );
		
		
		ret = ftdi_write_data(&ctx->ftdic,(unsigned char*)ctx->writeBuffer,c);
		if(ret<0) {
			sprintf(ctx->error,"write() failed");
			tty_stop(ctx);
			return 0;
		} else if(ret==ctx->writeCounter) {
			ctx->writeCounter=0;
		} else if(ret>0){
			memmove(ctx->writeBuffer,ctx->writeBuffer+ret,ctx->writeCounter-ret);
			ctx->writeCounter-=ret;
		}
	
		pthread_mutex_unlock( &ctx->mutex_write );
		return 1;
	} 
	
	return 1;
}

int tty_main_tty(struct tty_context *ctx) {
	int res,i,s;
	char buf[64];
	struct timeval tv;
	
	fd_set readFDs, writeFDs, exceptFDs;
	
	if(ctx->mode != AV_MODE_TTY) {
		return 0;
	}
	
	tv.tv_sec 	= 30;
	tv.tv_usec 	= 0;
	
	if(ctx->fd<=0) {
		sprintf(ctx->error,"not connected");
		return 0;
	}
	
	FD_ZERO(&readFDs);
	FD_ZERO(&writeFDs);
	FD_ZERO(&exceptFDs);

	if(ctx->writeCounter>0) {
		FD_SET(ctx->fd, &writeFDs);
	}
	FD_SET(ctx->fd, &readFDs);
	FD_SET(ctx->fd, &exceptFDs);
	FD_SET(ctx->pipeFD[0],&readFDs);
	
	res = select((ctx->fd>ctx->pipeFD[0]?ctx->fd:ctx->pipeFD[0])+1, &readFDs, &writeFDs, &exceptFDs, &tv);
	
	if(FD_ISSET(ctx->pipeFD[0],&readFDs)) {
		char tmpBuffer[32];
		read(ctx->pipeFD[0], tmpBuffer, 32);
	}
		
	/* socket ready to read */
	if(FD_ISSET(ctx->fd,&readFDs)) {
		res = read(ctx->fd,ctx->readBuffer+ctx->readCounter,BUFFER_SIZE-ctx->readCounter);
		/* received some bytes */
		if(res>0) {
			ctx->readCounter += res;

			/* check for complete messages */
			for(s=0,i=0;i<ctx->readCounter;i++) {
				if(ctx->readBuffer[i] == ctx->ctrlChar) {
					memcpy(buf,ctx->readBuffer+s,i-s);
					buf[i-s] = 0x0;
					tty_recv(ctx,buf);
					s = i+1;
				}
			}
			
			if(s>=ctx->readCounter) {
				ctx->readCounter = 0;
			} else if(s>0) {
				memmove(ctx->readBuffer,ctx->readBuffer+s,ctx->readCounter-s);
				ctx->readCounter-=s;
			}
		/* error */
		} else if(res<0) {
			sprintf(ctx->error,"read() failed");
			tty_stop(ctx);
			return 0;
		}
		
		if(ctx->readCounter == BUFFER_SIZE) {
			sprintf(ctx->error,"read buffer overflow detected\n");
			tty_stop(ctx);
			return 0;
		}
	}
	
	/* socket ready to write */
	if(FD_ISSET(ctx->fd,&writeFDs)) {
		pthread_mutex_lock( &ctx->mutex_write );
		
		res = write(ctx->fd,ctx->writeBuffer,ctx->writeCounter);
		if(res<0) {
			sprintf(ctx->error,"write() failed");
			tty_stop(ctx);
			return 0;
		} else if(res==ctx->writeCounter) {
			ctx->writeCounter=0;
		} else if(res>0){
			memmove(ctx->writeBuffer,ctx->writeBuffer+res,ctx->writeCounter-res);
			ctx->writeCounter-=res;
		}
		
		pthread_mutex_unlock( &ctx->mutex_write );
	}
	
	/* socket error */
	if(FD_ISSET(ctx->fd,&exceptFDs)) {
		sprintf(ctx->error,"device error");
		tty_stop(ctx);
		return 0;
	}		
		
	return 1;
}
	 