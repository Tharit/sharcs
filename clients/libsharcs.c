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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include "libsharcs.h"
#include "../packet.h"

#define MAX(a,b) a>b?a:b

pthread_t thread_handle;
int thread_stop;
pthread_mutex_t mutex_write = PTHREAD_MUTEX_INITIALIZER;
int pipeFD[2];

int clientSocket = -1;
char *writeBuffer,*readBuffer;
int readCounter, writeCounter, writeBufferSize, readBufferSize;
time_t lastPing,lastPong;

int (*sharcs_callback_i)(sharcs_id,int);
int (*sharcs_callback_s)(sharcs_id,const char*);
void (*sharcs_callback_retrieve)(int);

struct sharcs_module *modules = NULL;
int numModules = 0;

void handlePacket(struct sharcs_packet *p);
void readFromSocket();
void writeToSocket();
void sendPacket(struct sharcs_packet *p);
void wakeUp();
void* run(void *threadid);

void readFromSocket() {
	int nBytes = recv(clientSocket, readBuffer + readCounter, readBufferSize - readCounter, 0);
	if (nBytes <= 0) {
		return;
    }

    readCounter += nBytes;

    if(readCounter >= 0.75 * readBufferSize) {
        readBufferSize*=2;
        readBuffer = (char*)realloc(readBuffer, readBufferSize);
    }

	/* check if a complete packet was received */
	int packetLen = 0;
	struct sharcs_packet *p;
	
	while(1) {
		if(readCounter > 0) {
			packetLen = bswap_32(*(uint32_t*)readBuffer);
		} else {
			break;
		}

		/* check that the packet was completly received */
		if(packetLen > readCounter) {
			if(packetLen > 1024) {
				/* @TODO close connection */
			}
            break;
		}

        if(packetLen <= 0) {
			printf("received invalid packet\n");
			break;
        } else {
			
			p = packet_create_buffer(readBuffer,packetLen);

            readCounter -= packetLen;
            memmove(readBuffer, readBuffer + packetLen, readCounter);
			
			/* handle packets */
			handlePacket(p);
			
			packet_delete(p);
        }
	}
}

void writeToSocket() {
	if(writeCounter <= 0) {
		return;
	}

	pthread_mutex_lock(&mutex_write);

	int nBytes = send(clientSocket, writeBuffer, writeCounter, 0);
	
    if (nBytes < 0) {
		#ifdef _WIN32
			if(WSAGetLastError() == WSAEWOULDBLOCK) {
				return;
			}
		#endif
		return;
    } else if (nBytes == writeCounter) {
        writeCounter = 0;
	} else if(nBytes > 0) {
		writeCounter -= nBytes;
		memmove(writeBuffer, writeBuffer + nBytes, writeCounter);
    }
	pthread_mutex_unlock(&mutex_write);
}	

void sendPacket(struct sharcs_packet *p) {
	pthread_mutex_unlock(&mutex_write);
	
	int len = packet_size(p);

	packet_seek(p,0);
	packet_append32(p,len);

	const char *buffer = packet_buffer(p);

    if(writeBufferSize <= writeCounter + len) {
        while(writeBufferSize <= writeCounter + len) {
            writeBufferSize*=2;
        }
        writeBuffer = (char*)realloc(writeBuffer, writeBufferSize);
    }

	memcpy(writeBuffer + writeCounter, buffer, len);
	writeCounter += len;
		
	pthread_mutex_unlock(&mutex_write);
}

void wakeUp() {
	write(pipeFD[1], ".", 1);
}

void* run(void *threadid) {
	struct timeval tv;
	fd_set ReadFDs, WriteFDs, ExceptFDs;
	int res;
	
	/* connection loop */
	while(!thread_stop) {
		tv.tv_sec 	= 30;
		tv.tv_usec 	= 0;

		FD_ZERO(&ReadFDs);
		FD_ZERO(&WriteFDs);
		FD_ZERO(&ExceptFDs);

		FD_SET(clientSocket, &ReadFDs);
		if(writeCounter>0) {
			FD_SET(clientSocket, &WriteFDs);
		}
		
		FD_SET(pipeFD[0],&ReadFDs);
		FD_SET(clientSocket,&ExceptFDs);
		
		res = select((MAX(clientSocket,pipeFD[0]))+1, &ReadFDs, &WriteFDs, &ExceptFDs, &tv);
		
		/* error occured */
		if(res < 0) {

		/* timeout */
		} else if(res == 0) {

		/* handle sockets */
		} else {
			/* wakeup trigger */
			char tmpBuffer[32];

			if(FD_ISSET(pipeFD[0],&ReadFDs)) {
				read(pipeFD[0], tmpBuffer, 32);
			}

			/* handle errors */
			if (FD_ISSET(clientSocket, &ExceptFDs)) {
				/* @TODO detect & handle errors ... */
				printf("connection lost!\n");
				exit(-1);
			}
			/* read data */
			if (FD_ISSET(clientSocket, &ReadFDs)) {
				readFromSocket();
			} 
			/* write data */
			if (FD_ISSET(clientSocket, &WriteFDs)) {
				writeToSocket();
			}	
		}
	}
	
	pthread_exit(NULL);
}



void handlePacket(struct sharcs_packet *p) {
	int packetLen, packetType;
	struct sharcs_packet *p2;
	
	packetLen = packet_read32(p);
	packetType = packet_read8(p);

	/* ping => reply with pong */
	switch(packetType) {
		case M_S_PING: {
			p2 = packet_create();
			packet_append32(p2,0);
			packet_append8(p2,M_C_PONG);
			packet_append64(p2,packet_read64(p));
	
			sendPacket(p2);
			wakeUp();

			packet_delete(p2);
			break;
		}
		case M_S_FEATURE_ERROR: {
			int f;
			
			f = packet_read32(p);
			
			sharcs_callback_i(f,SHARCS_VALUE_ERROR);
			break;
		}
		case M_S_FEATURE_I: {
			struct sharcs_feature *feature;
			int f,v;
			
			f = packet_read32(p);
			v = packet_read32(p);
			
			/* update data structures */
			feature = sharcs_feature(f);
			if(feature) {
				switch(feature->feature_type) {
				case SHARCS_FEATURE_ENUM:
					feature->feature_value.v_enum.value = v;
					break;
				case SHARCS_FEATURE_SWITCH:
					feature->feature_value.v_switch.state = v;
					
					if(feature->feature_flags & SHARCS_FLAG_POWER) {
						if(!v) {
							sharcs_device(f)->device_flags |= SHARCS_FLAG_STANDBY;
						} else {
							sharcs_device(f)->device_flags &= ~SHARCS_FLAG_STANDBY;
						}
					}
					
					break;
				case SHARCS_FEATURE_RANGE:
					feature->feature_value.v_range.value = v;
					break;
				}
			}
			
			sharcs_callback_i(f,v);
			break;
		}
		case M_S_FEATURE_S: {
			int f;
			const char *s;
			f = packet_read32(p);
			s = packet_read_string(p);
			
			sharcs_callback_s(f,s);
			break;
		}
        case M_S_UPDATE: {
            int i,n,f,v;
			
            n = packet_read32(p);
            for(i=0;i<n;i++) {
                f = packet_read32(p);
                v = packet_read32(p);
			    sharcs_callback_i(f,v);
            }
            
            break;
        }
		case M_S_RETRIEVE: {
			int i,j,k,l;
			struct sharcs_module *m;
			struct sharcs_device *d;
			struct sharcs_feature *f;
			
			if(modules) {
				printf("ERROR: modules already received!");
				return;
			}
			
			/* number of modules */
			numModules = packet_read8(p);
			
			modules = (struct sharcs_module*)malloc(sizeof(struct sharcs_module)*numModules);
			memset(modules,0,sizeof(struct sharcs_module)*numModules);
			
			/* read modules */
			for(i=0;i<numModules;i++) {
				m = &modules[i];
				
				m->module_id = packet_read32(p);
				m->module_name = strdup(packet_read_string(p));
				m->module_description = strdup(packet_read_string(p));
				m->module_version = strdup(packet_read_string(p));
				
				m->module_devices_size = packet_read32(p);
				m->module_devices = (struct sharcs_device**)malloc(sizeof(struct sharcs_device*)*m->module_devices_size);
				
				for(j=0;j<m->module_devices_size;j++) {
					d = m->module_devices[j] = (struct sharcs_device*)malloc(sizeof(struct sharcs_device));
						
					d->device_id = packet_read32(p);
					d->device_name = strdup(packet_read_string(p));
					d->device_description = strdup(packet_read_string(p));
					
					d->device_flags = packet_read32(p);
					
					d->device_features_size = packet_read32(p);
					d->device_features = (struct sharcs_feature**)malloc(sizeof(struct sharcs_feature*)*d->device_features_size);
					
					for(k=0;k<d->device_features_size;k++) {
						f = d->device_features[k] = (struct sharcs_feature*)malloc(sizeof(struct sharcs_feature));
						
						f->feature_id = packet_read32(p);
						f->feature_name = strdup(packet_read_string(p));
						f->feature_description = strdup(packet_read_string(p));

						f->feature_type = packet_read32(p);
						f->feature_flags = packet_read32(p);
												
						switch(f->feature_type) {
							case SHARCS_FEATURE_ENUM:
								f->feature_value.v_enum.size = packet_read32(p);
								f->feature_value.v_enum.values = (const char**)malloc(sizeof(char*)*f->feature_value.v_enum.size);
																										
								for(l=0;l<f->feature_value.v_enum.size;l++) {
									f->feature_value.v_enum.values[l] = strdup(packet_read_string(p));
								}
								f->feature_value.v_enum.value = packet_read32(p);
								break;
							case SHARCS_FEATURE_SWITCH:
								f->feature_value.v_switch.state = packet_read32(p);
								break;
							case SHARCS_FEATURE_RANGE:
								f->feature_value.v_range.start = packet_read32(p);
								f->feature_value.v_range.end = packet_read32(p);
								f->feature_value.v_range.value = packet_read32(p);
								break;
						}
					}		
				}
			}
			
			if(sharcs_callback_retrieve) {
				sharcs_callback_retrieve(1);
			}
			
			break;
		}
		case M_S_PROFILE_LOAD: {
			break;
		}
		case M_S_PROFILE_SAVE: {
			break;			
		}
		case M_S_PROFILE_DELETE: {
			break;
		}
		default: {
			printf("<< received unhandled packet 0x%02x, size %d\n",packetType,packetLen);

			break;
		}
	}
}

/*
 *============================================================
 * API
 *============================================================
 */
int sharcs_init(const char *server,
                int (*callback_i)(sharcs_id,int),
				int (*callback_s)(sharcs_id,const char*)) {
	
	sharcs_callback_i = callback_i;
	sharcs_callback_s = callback_s;
					
	/* try to connect */
	struct sockaddr_in addr;
	struct timeval to;
	int res;
	
	to.tv_sec	= 5;
	to.tv_usec	= 0;

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(clientSocket < 0) {
		return 0;
	}

	memset(&addr,0,sizeof(addr)); 
	addr.sin_family			=	AF_INET;
	addr.sin_port			=	htons(8585); 
	inet_pton(AF_INET, server, &(addr.sin_addr));
	
	res = connect(clientSocket,(struct sockaddr*)&addr,sizeof(addr));

	/* check for success */
	if(res < 0) {
		close(clientSocket);
		return 0;
	}
	
	pipe(pipeFD);
	
	/* initialize buffers */
	readBufferSize =
	writeBufferSize = 128;
	
	readCounter =
	writeCounter = 0;
			
	writeBuffer = (char*)malloc(writeBufferSize*sizeof(char));
	readBuffer 	= (char*)malloc(readBufferSize*sizeof(char));
	
	/* start thread */
	thread_stop = 0;
	pthread_create(&thread_handle, NULL, run, (void *)0);
	
	return 1;
}

int sharcs_stop() {
	if(clientSocket<0||thread_stop) {
		return 0;
	}
	
	thread_stop = 1;
	wakeUp();
	
	pthread_join(0,NULL);
	
	close(clientSocket);
	close(pipeFD[0]);
	close(pipeFD[0]);	
	
    clientSocket = -1;
    
	return 1;
}

int sharcs_set_i(sharcs_id feature,int value) {
	struct sharcs_packet *p;
	
    if(clientSocket<0) {
        return 0;
    }
    
	p = packet_create();
	packet_append32(p,0);
	packet_append8(p,M_C_FEATURE_I);
	packet_append32(p,feature);
	packet_append32(p,value);
	
	sendPacket(p);
	
	wakeUp();
	
	packet_delete(p);
    
    return 1;
}

int sharcs_set_s(sharcs_id feature,const char* value) {
	struct sharcs_packet *p;
	
    if(clientSocket<0) {
        return 0;
    }
    
	p = packet_create();
	packet_append32(p,0);
	packet_append8(p,M_C_FEATURE_S);
	packet_append32(p,feature);
	packet_append_string(p,value);
	
	sendPacket(p);
	
	wakeUp();
	
	packet_delete(p);
    
    return 1;
}

/* profiles */
int sharcs_enumerate_profiles(struct sharcs_profile **profile,int index) {
	return 0;
}

int sharcs_profile_save(const char *name) {
	return 0;
}

int sharcs_profile_load(int profile_id) {
	return 0;
}

int sharcs_profile_delete(int profile_id) {
	return 0;
}

/* enumeration */
int sharcs_retrieve(void (*cb)(int)) {
	struct sharcs_packet *p;
    
    if(clientSocket<0) {
        return 0;
    }
	
	sharcs_callback_retrieve = cb;
	
    p = packet_create();
    packet_append32(p,0);
    
    if(modules) {
        packet_append8(p,M_C_UPDATE);    
    } else {
        packet_append8(p,M_C_RETRIEVE);
	}
    
	sendPacket(p);
	
	wakeUp();
	
	packet_delete(p);
    
    return 1;
}

int sharcs_enumerate_modules(struct sharcs_module **module,int index) {
	if(index<0||index>=numModules) {
		*module = NULL;
		return 0;
	}
	
	*module = &modules[index];
	
	return 1;
}

struct sharcs_module* sharcs_module(sharcs_id id) {
	int i = 0;
	for(i=0;i<numModules;i++) {
		if(modules[i].module_id == SHARCS_ID_MODULE(id)) {
			return &modules[i];
		}
	}
	return NULL;
}

struct sharcs_device* sharcs_device(sharcs_id id) {
	int i = 0;
	struct sharcs_module *m;
	
	m = sharcs_module(id);
	if(!m) {
		return NULL;
	}
	
	for(i=0;i<m->module_devices_size;i++) {
		if(m->module_devices[i]->device_id == SHARCS_ID_DEVICE(id)) {
			return m->module_devices[i];
		}
	}
	
	return NULL;
}

struct sharcs_feature* sharcs_feature(sharcs_id id) {
	int i = 0;
	struct sharcs_device *d;
	
	if(SHARCS_ID_TYPE(id)!=SHARCS_FEATURE) {
		return NULL;
	}
	
	d = sharcs_device(id);
	if(!d) {
		return NULL;
	}
	
	for(i=0;i<d->device_features_size;i++) {
		if(d->device_features[i]->feature_id == id) {
			return d->device_features[i];
		}
	}
	
	return NULL;
}

struct sharcs_profile* sharcs_profile(int id) {
	/* @TODO profiles */
}
