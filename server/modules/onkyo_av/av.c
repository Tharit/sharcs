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

#include "av.h"

/* 
@TODO ! gracefully handle unknown values.. there might be more strange listening modes, or N/A return values, or other receiver models... 
*/

int fd = -1,pipeFD[2];
struct termios options;

/*------------------------------------------------------
 * buffers
 *------------------------------------------------------*/
#define BUFFER_SIZE 128
char readBuffer[BUFFER_SIZE], writeBuffer[BUFFER_SIZE],error[256];
int readCounter, writeCounter;
pthread_mutex_t mutex_write = PTHREAD_MUTEX_INITIALIZER;

/*------------------------------------------------------
 * global variables and helper functions
 *------------------------------------------------------*/

enum {
	AV_MODE_STOPPED,
	AV_MODE_TTY,
	AV_MODE_LIBFTDI,
};

#define MAP_CMD(i,cmd)\
if(!strncmp(cmd,"PWR",3)) {\
	i = AV_CMD_POWER;\
} else if(!strncmp(cmd,"AMT",3)) {\
	i = AV_CMD_MUTE;\
} else if(!strncmp(cmd,"SLI",3)) {\
	i = AV_CMD_INPUT;\
} else if(!strncmp(cmd,"LMD",3)) {\
	i = AV_CMD_MODE;\
} else if(!strncmp(cmd,"MVL",3)) {\
	i = AV_CMD_VOLUME;\
} else if(!strncmp(cmd,"LTN",3)) {\
	i = AV_CMD_LATENIGHT;\
} else if(!strncmp(cmd,"DIM",3)) {\
	i = AV_CMD_DIMMER;\
} else if(!strncmp(cmd,"PRS",3)) {\
	i = AV_CMD_PRESET;\
}

#define MAP_CMD2(i,cmd)\
switch(i){\
	case AV_CMD_POWER: cmd = "PWR"; break;\
	case AV_CMD_MUTE: cmd = "AMT"; break;\
	case AV_CMD_INPUT: cmd = "SLI"; break;\
	case AV_CMD_MODE: cmd = "LMD"; break;\
	case AV_CMD_VOLUME: cmd = "MVL"; break;\
	case AV_CMD_LATENIGHT: cmd = "LTN"; break;\
	case AV_CMD_DIMMER: cmd = "DIM"; break;\
	case AV_CMD_PRESET: cmd = "PRS"; break;\
	default: cmd = NULL; break;\
}

int state[AV_NUM_COMMANDS],pending[AV_NUM_COMMANDS];
int numPending;
void (*callback)(int,int);
int (*av_main)() = NULL;
int av_main_tty();
int av_main_libftdi();
int av_mode = AV_MODE_STOPPED;

struct ftdi_context ftdic;

void _sendCmd(const char *buf) {
	int n;
	
	n = strlen(buf);
	
	pthread_mutex_lock( &mutex_write );
	
	if(writeCounter+n>BUFFER_SIZE) {
		fprintf(stderr,"write buffer overflow detected\n");
		exit(-1);
	}
	memcpy(writeBuffer+writeCounter,buf,n);
	writeCounter+=n;
	
	pthread_mutex_unlock( &mutex_write );
	
	write(pipeFD[1], ".", 1);
}

int reqCmd(int i) {
	char buf[64];
	const char *cmd;
	
	MAP_CMD2(i,cmd);
	if(!cmd) {
		return 0;
	}
	
	if(pending[i]==0) {
		
		sprintf(buf,"!1%sQSTN\n",cmd);
		_sendCmd(buf);

		pending[i] = time(NULL);
		numPending++;
	}
	
	return 1;
}

int sendCmds(int i, const char *v) {
	char buf[64];
	const char *cmd;
	int j,len;
	
	MAP_CMD2(i,cmd);
	if(!cmd) {
		return 0;
	}
	
	sprintf(buf,"!1%s%s\n",cmd,v);
	
	/* convert value to upper case */
	for(len=strlen(v),j=5;j<5+len;j++) {
		buf[j] = toupper(buf[j]);
	}
	
	_sendCmd(buf);
	
	return 1;
}

int sendCmdi(int i, int v) {
	char buf[64];
	const char *cmd;
	
	MAP_CMD2(i,cmd);
	if(!cmd || state[i] == v) {
		return 0;
	}
	
	/* value conversion */
	if(i == AV_CMD_VOLUME) {
		v = (80-v+2);
	}
	
	sprintf(buf,"!1%s%02X\n",cmd,v);
	_sendCmd(buf);
	
	return 1;
}

int recvCmd(const char *buf) {
	char cmd[3];
	int v = 0, i = -1;
	
	sscanf(buf,"!1%3s%2x",cmd,&v);
	
	MAP_CMD(i,cmd);
	if(i<0) {
		return -1;
	}
	
	/* value conversion */
	if(i == AV_CMD_VOLUME) {
		v = (80-v+2);
	}
	
	if(!av_validvi(i,v)) {
		printf("av: received unknown value 0x%2x for '%s'\n",v,av_cmd2str(i));
		return 0;
	}
	
	state[i] = v;
	
	if(callback) {
		callback(i,v);
	}
	
	if(pending[i]>0) {
		pending[i]=0;
		numPending--;
	}
	
	return i;
}

/*------------------------------------------------------------*/

const char* av_error() {
	if(error[0]==0) {
		return NULL;
	}
	return error;
}


int av_init_internal(void (*cb)(int,int)) {
	callback = cb;
	
	memset(state,-1,sizeof(int)*AV_NUM_COMMANDS);
	memset(pending,0,sizeof(int)*AV_NUM_COMMANDS);
	
	pipe(pipeFD);
	
	readCounter = 0;
	writeCounter = 0;
	numPending = 0;
	
	return 1;
}

int av_init_libftdi(int vendor,int product,const char *description,const char *serial,unsigned int index,void (*cb)(int,int)) {
	int ret;
	
	if (ftdi_init(&ftdic) < 0) {
        sprintf(error, "ftdi_init failed\n");
        return 0;
    }
	
	if ((ret = ftdi_usb_open_desc_index(&ftdic, vendor,product,description,serial,index)) < 0) {
        sprintf(error, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
        return 0;
    }

	ftdi_usb_purge_rx_buffer(&ftdic);
	ftdi_set_latency_timer(&ftdic,40);
	ftdi_set_event_char(&ftdic,0x1A,1);
	
	// set av_main implementation
	av_mode = AV_MODE_LIBFTDI;
	av_main = &av_main_libftdi;
	
	// initialize variables
	av_init_internal(cb);
	
	return 1;
}

int av_init_tty(const char *devicename,void (*cb)(int,int)) {
	fd = open(devicename,O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		sprintf(error,"%s - %s",devicename,strerror(errno));
		return 0;
	}
	
	tcgetattr(fd, &options);
	cfsetispeed(&options, B9600);
	
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	
	tcsetattr(fd, TCSANOW, &options);
	tcflush(fd,TCIOFLUSH);
	
	// set av_main implementation
	av_mode = AV_MODE_TTY;
	av_main = &av_main_tty;
	
	// initialize variables
	av_init_internal(cb);
	
	return 1;
}

void av_req(int cmd) {
	if(cmd < 0 || cmd >= AV_NUM_COMMANDS) {
		int i;
		for(i=0;i<AV_NUM_COMMANDS;i++) {
			reqCmd(i);
		}
	} else {
		reqCmd(cmd);
	}
}

int av_seti(int cmd, int v) {
	if(!av_validvi(cmd,v)) {
		sprintf(error,"invalid value '%d' for '%s'",v,av_cmd2str(cmd));
		return 0;
	}
	
	return sendCmdi(cmd,v);
}

int av_sets(int cmd, const char* v) {
	int iv;
	
	if(!av_validvs(cmd,v)) {
		sprintf(error,"invalid value '%s' for '%s'",v,av_cmd2str(cmd));
		return 0;
	}
	
	iv = av_str2v(cmd,v);
	if(iv>=0) {
		return sendCmdi(cmd,iv);
	}
	return sendCmds(cmd,v);
}

int av_state(int cmd) {
	if(cmd < 0 || cmd >= AV_NUM_COMMANDS) {
		return -1;
	}
	return state[cmd];
}

int av_busy() {
	return writeCounter>0||readCounter>0||numPending>0;
}

int av_stop() {
	if(av_mode == AV_MODE_STOPPED) {
		return 0;
	}
	
	if(av_mode == AV_MODE_TTY && fd>=0) {
		tcsetattr(fd,TCSANOW,&options);
		close(fd);
		fd = -1;
	} else if(av_mode == AV_MODE_LIBFTDI) {
		int ret = 0;
		if ((ret = ftdi_usb_close(&ftdic)) < 0) {
			sprintf(error, "unable to close ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
			return 0;
		}

		ftdi_deinit(&ftdic);
	}
	
	close(pipeFD[0]);
	close(pipeFD[1]);
	
	av_mode = AV_MODE_STOPPED;
	
	return 1;
}

int av_main_libftdi() {
	static int answer_pending = -1,answer_length;
	static time_t answer_timer;
	static char answer_buffer[64];
	
	int ret,i,s,c;
	char buf[64];
	
	if(av_mode != AV_MODE_LIBFTDI) {
		return 0;
	}
	
	/* read data */
	ret = ftdi_read_data(&ftdic,(unsigned char*)(readBuffer+readCounter),BUFFER_SIZE-readCounter);
	if(ret > 0) {
		readCounter += ret;

		/* check for complete messages */
		for(s=0,i=0;i<readCounter;i++) {
			if(readBuffer[i] == 0x1A) {
				memcpy(buf,readBuffer+s,i-s);
				buf[i-s] = 0x0;
				if(answer_pending==recvCmd(buf)) {
					answer_pending = -1;
				}
				s = i+1;
			}
		}
		
		if(s>=readCounter) {
			readCounter = 0;
		} else if(s>0) {
			memmove(readBuffer,readBuffer+s,readCounter-s);
			readCounter-=s;
		}
		return 1;
	} else if(ret < 0) {
		sprintf(error,"read() failed");
		av_stop();
		
		return 0;
	}
	
	/* write data */
	if(writeCounter>0 && answer_pending<0) {
		// find length of first command
		for(c=0;c<writeCounter;c++) {
			if(writeBuffer[c] == '\n') {
				c++;
				break;
			}
		}
		
		pthread_mutex_lock( &mutex_write );
		
		if(!strncmp(writeBuffer+5,"QSTN",4)) {
			MAP_CMD(answer_pending,writeBuffer+2);
			answer_timer = time(NULL);
			memcpy(answer_buffer,writeBuffer,c);
			answer_length = c;
		}
		
		ret = ftdi_write_data(&ftdic,(unsigned char*)writeBuffer,c);
		if(ret<0) {
			sprintf(error,"write() failed");
			av_stop();
			return 0;
		} else if(ret==writeCounter) {
			writeCounter=0;
		} else if(ret>0){
			memmove(writeBuffer,writeBuffer+ret,writeCounter-ret);
			writeCounter-=ret;
		}
	
		pthread_mutex_unlock( &mutex_write );
		return 1;
	} 
	
	if(answer_pending>=0 && time(NULL)-answer_timer>1) {
		ftdi_write_data(&ftdic,(unsigned char*)answer_buffer,answer_length);
		answer_timer = time(NULL);
		return 1;
	}
	
	return 1;
}

int av_main_tty() {
	int res,i,s;
	char buf[64];
	struct timeval tv;
	
	fd_set readFDs, writeFDs, exceptFDs;
	
	if(av_mode != AV_MODE_TTY) {
		return 0;
	}
	
	tv.tv_sec 	= 30;
	tv.tv_usec 	= 0;
	
	if(fd<=0) {
		sprintf(error,"not connected");
		return 0;
	}
	
	FD_ZERO(&readFDs);
	FD_ZERO(&writeFDs);
	FD_ZERO(&exceptFDs);

	if(writeCounter>0) {
		FD_SET(fd, &writeFDs);
	}
	FD_SET(fd, &readFDs);
	FD_SET(fd, &exceptFDs);
	FD_SET(pipeFD[0],&readFDs);
	
	res = select((fd>pipeFD[0]?fd:pipeFD[0])+1, &readFDs, &writeFDs, &exceptFDs, &tv);
		
	if(FD_ISSET(pipeFD[0],&readFDs)) {
		char tmpBuffer[32];
		read(pipeFD[0], tmpBuffer, 32);
	}
		
	/* socket ready to read */
	if(FD_ISSET(fd,&readFDs)) {
		res = read(fd,readBuffer+readCounter,BUFFER_SIZE-readCounter);
		/* received some bytes */
		if(res>0) {
			readCounter += res;

			/* check for complete messages */
			for(s=0,i=0;i<readCounter;i++) {
				if(readBuffer[i] == 0x1A) {
					memcpy(buf,readBuffer+s,i-s);
					buf[i-s] = 0x0;
					recvCmd(buf);
					s = i+1;
				}
			}
			
			if(s>=readCounter) {
				readCounter = 0;
			} else if(s>0) {
				memmove(readBuffer,readBuffer+s,readCounter-s);
				readCounter-=s;
			}
		/* error */
		} else if(res<0) {
			sprintf(error,"read() failed");
			av_stop();
			return 0;
		}
		
		if(readCounter == BUFFER_SIZE) {
			sprintf(error,"read buffer overflow detected\n");
			av_stop();
			return 0;
		}
	}
	
	/* socket ready to write */
	if(FD_ISSET(fd,&writeFDs)) {
		pthread_mutex_lock( &mutex_write );
		
		res = write(fd,writeBuffer,writeCounter);
		if(res<0) {
			sprintf(error,"write() failed");
			av_stop();
			return 0;
		} else if(res==writeCounter) {
			writeCounter=0;
		} else if(res>0){
			memmove(writeBuffer,writeBuffer+res,writeCounter-res);
			writeCounter-=res;
		}
		
		pthread_mutex_unlock( &mutex_write );
	}
	
	/* socket error */
	if(FD_ISSET(fd,&exceptFDs)) {
		sprintf(error,"device error");
		av_stop();
		return 0;
	}		
	
	/* when receiver is in standby, some requests get "lost" => try to resend */
	if(res<=0&&numPending>0) {
		time_t now = time(NULL);
		for(i=0;i<AV_NUM_COMMANDS;i++) {
			if(pending[i]>0 && now-pending[i]>=1) {
				pending[i] = 0;
				numPending--;
				av_req(i);
				break;
			}
		}
	}
	
	return 1;
}

const char* av_cmd2str(int cmd) {
	switch(cmd){
		case AV_CMD_POWER: return "power";
		case AV_CMD_MUTE: return "mute";
		case AV_CMD_INPUT: return "input";
		case AV_CMD_MODE: return "mode";
		case AV_CMD_VOLUME: return "volume";
		case AV_CMD_LATENIGHT: return "latenight";
		case AV_CMD_DIMMER: return "dimmer";
		case AV_CMD_PRESET: return "preset";
	}
	return NULL;
}

int av_str2cmd(const char* cmd) {
	if(!strcmp(cmd,"power")) {
		return AV_CMD_POWER;
	} else if(!strcmp(cmd,"mute")) {
		return AV_CMD_MUTE;
	} else if(!strcmp(cmd,"input")) {
		return AV_CMD_INPUT;
	} else if(!strcmp(cmd,"mode")) {
		return AV_CMD_MODE;
	} else if(!strcmp(cmd,"volume")) {
		return AV_CMD_VOLUME;
	} else if(!strcmp(cmd,"latenight")) {
		return AV_CMD_LATENIGHT;
	} else if(!strcmp(cmd,"dimmer")) {
		return AV_CMD_DIMMER;
	} else if(!strcmp(cmd,"preset")) {
		return AV_CMD_PRESET;
	}
	return -1;
}

const char* av_v2str(int cmd,int v) {
	static const char *res;
	static char buffer[256];
	
	res = NULL;
	
	switch(cmd) {
		case AV_CMD_INPUT:
			switch(v) {
				case AV_INPUT_DVD: res = "dvd"; break;
				case AV_INPUT_VCR_DVR: res = "vcr/dvr"; break;
				case AV_INPUT_CBL_SAT: res = "cbl/sat"; break;
				case AV_INPUT_GAME_TV: res = "game/tv"; break;
				case AV_INPUT_AUX1: res = "aux1"; break;
				case AV_INPUT_AUX2: res = "aux2"; break;
				case AV_INPUT_TAPE: res = "tape"; break;
				case AV_INPUT_TUNER: res = "tuner"; break;
				case AV_INPUT_TUNER_AM: res = "tuner(AM)"; break;
				case AV_INPUT_CD: res = "cd"; break;
				case AV_INPUT_PHONO: res = "phono"; break;
			}
			break;
		case AV_CMD_DIMMER:
			switch(v) {
				case AV_DIMMER_BRIGHTEST: res = "brightest"; break;
				case AV_DIMMER_BRIGHT: res = "bright"; break;
				case AV_DIMMER_DIM: res = "dim"; break;
				case AV_DIMMER_DARK: res = "dark"; break;
			}
			break;
		case AV_CMD_MODE:
			switch(v) {
				case AV_MODE_STEREO: res = "stereo"; break;
				case AV_MODE_DIRECT: res = "direct"; break;
				case AV_MODE_SURROUND: res = "surround"; break;
				case AV_MODE_ALL_CH_STEREO: res = "allchstereo"; break;
				case AV_MODE_FILM: res = "film"; break;
				case AV_MODE_THX: res = "thx"; break;
				case AV_MODE_ACTION: res = "action"; break;
				case AV_MODE_MUSICAL: res = "musical"; break;
				case AV_MODE_MONO_MOVIE: res = "monomovie"; break;
				case AV_MODE_ORCHESTRA: res = "orchestra"; break;
				case AV_MODE_UNPLUGGED: res = "unplugged"; break;
				case AV_MODE_STUDIO_MIX: res = "studiomix"; break;
				case AV_MODE_TV_LOGIC: res = "tvlogic"; break;
				case AV_MODE_THEATER: res = "theater"; break;
				case AV_MODE_ENHANCED7: res = "enhanced7"; break;
				case AV_MODE_MONO: res = "mono"; break;
				case AV_MODE_PURE_AUDIO: res = "pureaudio"; break;
				case AV_MODE_PL_MOVIE: res = "plmovie"; break;
				case AV_MODE_PL_MUSIC: res = "plmusic"; break;
				case AV_MODE_NEO_CINEMA: res = "neocinema"; break;
				case AV_MODE_NEO_MUSIC: res = "neomusic"; break;
				case AV_MODE_NEO_THX_CINEMA: res = "neothxcinema"; break;
				case AV_MODE_PL_THX_CINEMA: res = "plthxcinema"; break;
				case AV_MODE_PL_GAME: res = "plgame"; break;
				case AV_MODE_NEURAL_THX: res = "neuralthx"; break;
			}
			break;
	}
	if(!res) {
		sprintf(buffer,"%d",v);
		res = buffer;
	}
	return res;
}

int av_str2v(int cmd,const char* v) {
	char *endptr;
	int iv;
	
	switch(cmd) {
		case AV_CMD_INPUT:
			if(!strcmp(v,"dvd")) {
				return AV_INPUT_DVD;
			} else if(!strcmp(v,"vcr/dvr") || !strcmp(v,"vcr") || !strcmp(v,"dvr")) {
				return AV_INPUT_VCR_DVR;
			} else if(!strcmp(v,"cbl/sat") || !strcmp(v,"cbl") || !strcmp(v,"sat")) {
				return AV_INPUT_CBL_SAT;
			} else if(!strcmp(v,"game/tv") || !strcmp(v,"game") || !strcmp(v,"tv") || !strcmp(v,"dock")) {
				return AV_INPUT_GAME_TV;
			} else if(!strcmp(v,"aux1")) {
				return AV_INPUT_AUX1;
			} else if(!strcmp(v,"aux2")) {
				return AV_INPUT_AUX2;
			} else if(!strcmp(v,"tape")) {
				return AV_INPUT_TAPE;
			} else if(!strcmp(v,"tuner")) {
				return AV_INPUT_TUNER;
			} else if(!strcmp(v,"tuner(AM)")) {
				return AV_INPUT_TUNER_AM;
			} else if(!strcmp(v,"cd")) {
				return AV_INPUT_CD;
			} else if(!strcmp(v,"phono")) {
				return AV_INPUT_PHONO;
			}
			break;
		case AV_CMD_DIMMER:
			if(!strcmp(v,"brightest")) {
				return AV_DIMMER_BRIGHTEST;
			} else if(!strcmp(v,"bright")) {
				return AV_DIMMER_BRIGHT;
			} else if(!strcmp(v,"dim")) {
				return AV_DIMMER_DIM;
			} else if(!strcmp(v,"dark")) {
				return AV_DIMMER_DARK;
			}
			break;
		case AV_CMD_MODE:
			if(!strcmp(v,"stereo")) {
				return AV_MODE_STEREO;
			} else if(!strcmp(v,"direct")) {
				return AV_MODE_DIRECT;
			} else if(!strcmp(v,"surround")) {
				return AV_MODE_SURROUND;
			} else if(!strcmp(v,"allchstereo")) {
				return AV_MODE_ALL_CH_STEREO;
			} else if(!strcmp(v,"film")) {
				return AV_MODE_FILM;
			} else if(!strcmp(v,"thx")) {
				return AV_MODE_THX;
			} else if(!strcmp(v,"action")) {
				return AV_MODE_ACTION;
			} else if(!strcmp(v,"musical")) {
				return AV_MODE_MUSICAL;
			} else if(!strcmp(v,"monomovie")) {
				return AV_MODE_MONO_MOVIE;
			} else if(!strcmp(v,"orchestra")) {
				return AV_MODE_ORCHESTRA;
			} else if(!strcmp(v,"unplugged")) {
				return AV_MODE_UNPLUGGED;
			} else if(!strcmp(v,"studiomix")) {
				return AV_MODE_STUDIO_MIX;
			} else if(!strcmp(v,"tvlogic")) {
				return AV_MODE_TV_LOGIC;
			} else if(!strcmp(v,"theater")) {
				return AV_MODE_THEATER;
			} else if(!strcmp(v,"enhanced7")) {
				return AV_MODE_ENHANCED7;
			} else if(!strcmp(v,"mono")) {
				return AV_MODE_MONO;
			} else if(!strcmp(v,"pureaudio")) {
				return AV_MODE_PURE_AUDIO;
			} else if(!strcmp(v,"plmovie")) {
				return AV_MODE_PL_MOVIE;
			} else if(!strcmp(v,"plmusic")) {
				return AV_MODE_PL_MUSIC;
			} else if(!strcmp(v,"neocinema")) {
				return AV_MODE_NEO_CINEMA;
			} else if(!strcmp(v,"neomusic")) {
				return AV_MODE_NEO_MUSIC;
			} else if(!strcmp(v,"neothxcinema")) {
				return AV_MODE_NEO_THX_CINEMA;
			} else if(!strcmp(v,"plthxcinema")) {
				return AV_MODE_PL_THX_CINEMA;
			} else if(!strcmp(v,"plgame")) {
				return AV_MODE_PL_GAME;
			} else if(!strcmp(v,"neuralthx")) {
				return AV_MODE_NEURAL_THX;
			}
			
			break;
	}

	iv = strtol(v,&endptr,10);
	if(endptr==v) {
		return -1;
	}
	return iv;
}

int av_validvi(int cmd,int i) {
	switch(cmd) {
		case AV_CMD_POWER:
		case AV_CMD_MUTE:
			return i==0||i==1;
		case AV_CMD_VOLUME:
			return i>=0 && i<=60; /* can go "up" to -20... but that would break the current implementation */
		case AV_CMD_PRESET:
			return i>=0 && i<=30;
		case AV_CMD_DIMMER:
			return (i==AV_DIMMER_BRIGHTEST)||(i==AV_DIMMER_BRIGHT)||(i==AV_DIMMER_DIM)||(i==AV_DIMMER_DARK);
		case AV_CMD_LATENIGHT:
			return i>=0 && i<=2;
		case AV_CMD_INPUT:
			return av_v2str(cmd,i)[0]>=97; /* quick and dirty.. avoids another switch/case */
		case AV_CMD_MODE:
			return av_v2str(cmd,i)[0]>=97;
	}
	return i>=0 && i<=99;
}

int av_validvs(int cmd,const char* v) {
	int iv;
	
	iv = av_str2v(cmd,v);
	if(iv>=0) {
		return av_validvi(cmd,iv);
	}
	
	switch(cmd) {
		case AV_CMD_VOLUME:
		case AV_CMD_MODE:
		case AV_CMD_INPUT:
		case AV_CMD_PRESET:
			if(!strcmp(v,"up")||!strcmp(v,"down")) {
				return 1;
			}
			break;
		case AV_CMD_LATENIGHT:
			if(!strcmp(v,"up")) {
				return 1;
			}
			break;
		case AV_CMD_DIMMER:
			if(!strcmp(v,"dim")) {
				return 1;
			}
			break;
	}
	
	return 0;
}