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
	
#include "../sharcs.h"
#include "main.h"
#include "connections.h"
#include "../packet.h"

struct sharcs_connection {
	unsigned int id;
	int connected;
	int socket;
	char *writeBuffer,*readBuffer;
	int readCounter, writeCounter, writeBufferSize, readBufferSize;
	time_t lastPing,lastPong;
};

unsigned int connectionId = 0;
int serverSocket;		
int pipeFD[2];
int stopEvent;
struct sharcs_connection connections[10];
pthread_mutex_t mutex_connections = PTHREAD_MUTEX_INITIALIZER;

void handlePacket(struct sharcs_connection *con, struct sharcs_packet *p);

void closeConnection(struct sharcs_connection *con) {
	if(!con->connected) {
		return;
	}
	
	close(con->socket);
	
	free(con->writeBuffer);
	free(con->readBuffer);
	
	con->connected = 0;
	
	fprintf(stdout,"[NET] client #%u disconnected\n",con->id);
}

void readFromSocket(struct sharcs_connection *con) {
	int nBytes = recv(con->socket, con->readBuffer + con->readCounter, con->readBufferSize - con->readCounter, 0);
	if (nBytes <= 0) {
		closeConnection(con);					
		return;
    }

    con->readCounter += nBytes;

    if(con->readCounter >= 0.75 * con->readBufferSize) {
        con->readBufferSize*=2;
        con->readBuffer = (char*)realloc(con->readBuffer, con->readBufferSize);
    }

	/* check if a complete packet was received */
	int packetLen = 0;
	struct sharcs_packet *p;

	while(1) {
		if(con->readCounter > 0) {
			packetLen = bswap_32(*(uint32_t*)con->readBuffer);
		} else {
			break;
		}

		/* check that the packet was completly received */
		if(packetLen > con->readCounter) {
			if(packetLen > 1024) {
				fprintf(stdout,"[NET] received invalid packet - packet length exceeds maximum size\n");
				closeConnection(con);
			}
            break;
		}

        if(packetLen <= 0) {
			fprintf(stdout,"[NET] received invalid packet - invalid header\n");
            closeConnection(con);
			break;
        } else {
			
			p = packet_create_buffer(con->readBuffer,packetLen);

            con->readCounter -= packetLen;
            memmove(con->readBuffer, con->readBuffer + packetLen, con->readCounter);

            handlePacket(con, p);

			packet_delete(p);
        }
	}
}

void writeToSocket(struct sharcs_connection *con) {
	if(con->writeCounter <= 0) {
		return;
	}

	int nBytes = send(con->socket, con->writeBuffer, con->writeCounter, 0);

    if (nBytes < 0) {
		#ifdef _WIN32
			if(WSAGetLastError() == WSAEWOULDBLOCK) {
				return;
			}
		#endif
		closeConnection(con);					
		return;
    } else if (nBytes == con->writeCounter) {
        con->writeCounter = 0;
	} else if(nBytes > 0) {
		con->writeCounter -= nBytes;
		memmove(con->writeBuffer, con->writeBuffer + nBytes, con->writeCounter);
    }
}	

void sendPacket(struct sharcs_connection *con, struct sharcs_packet *p) {
	int len = packet_size(p);

	packet_seek(p,0);
	packet_append32(p,len);

	const char *buffer = packet_buffer(p);

    if(con->writeBufferSize <= con->writeCounter + len) {
        while(con->writeBufferSize <= con->writeCounter + len) {
            con->writeBufferSize*=2;
        }
        con->writeBuffer = (char*)realloc(con->writeBuffer, con->writeBufferSize);
    }

	memcpy(con->writeBuffer + con->writeCounter, buffer, len);
	con->writeCounter += len;
}

void wakeUp() {
	write(pipeFD[1], ".", 1);
}

void handlePacket(struct sharcs_connection *con, struct sharcs_packet *p) {
	int packetLen, packetType;
	struct sharcs_packet *p2;
	
	/* check for valid header */
	if(p->size < 4+1) {
		return;
	}
	
	packetLen 	= packet_read32(p);
	packetType 	= packet_read8(p);

	switch(packetType) {
		/* ping => reply with pong */
		case M_C_PONG: {
			con->lastPong = time(NULL);
			break;
		}
		case M_C_FEATURE_I: {
			int f,v,r;
			
			/* check packet size */
			if(p->size < 4+1+4+4) {
				return;
			}
			
			f = packet_read32(p);
			v = packet_read32(p);
			
			if(!sharcs_set_i(f,v)) {
				p2 = packet_create();
				packet_append32(p2,0);
				packet_append8(p2,M_S_FEATURE_ERROR);
				packet_append32(p2,f);
				sendPacket(con,p2);
				packet_delete(p2);
			}
			
			break;
		}
		case M_C_FEATURE_S: {
			int f;
			const char *s;
			
			/* check packet size */
			if(p->size < 4+1+4+4) {
				return;
			}
			
			f = packet_read32(p);
			s = packet_read_string(p);
			
			if(!sharcs_set_s(f,s)) {
				p2 = packet_create();
				packet_append32(p2,0);
				packet_append8(p2,M_S_FEATURE_ERROR);
				packet_append32(p2,f);
				sendPacket(con,p2);
				packet_delete(p2);
			}
			
			break;
		}
		case M_C_RETRIEVE: {
			struct sharcs_module *m;
			struct sharcs_device *d;
			struct sharcs_feature *f;
			
			int i,j,k,l;
			
			/* enumerate modules */
			p2 = packet_create();
			packet_append32(p2,0);
			packet_append8(p2,M_S_RETRIEVE);
			packet_append8(p2,0);
			
			i = 0;
			while(sharcs_enumerate_modules(&m,i++)) {

				packet_append32(p2,m->module_id);
				packet_append_string(p2,m->module_name);
				packet_append_string(p2,m->module_description);
				packet_append_string(p2,m->module_version);
				
				packet_append32(p2,m->module_devices_size);
				for(j=0;j<m->module_devices_size;j++) {
					
					d = m->module_devices[j];
						
					packet_append32(p2,d->device_id);
					packet_append_string(p2,d->device_name);
					packet_append_string(p2,d->device_description);
					
					packet_append32(p2,d->device_features_size);
					for(k=0;k<d->device_features_size;k++) {
						f = d->device_features[k];
						
						packet_append32(p2,f->feature_id);
						packet_append_string(p2,f->feature_name);
						packet_append_string(p2,f->feature_description);
						packet_append32(p2,f->feature_type);
						
						switch(f->feature_type) {
							case SHARCS_FEATURE_ENUM:
								packet_append32(p2,f->feature_value.v_enum.size);
								for(l=0;l<f->feature_value.v_enum.size;l++) {
									packet_append_string(p2,f->feature_value.v_enum.values[l]);
								}
								packet_append32(p2,f->feature_value.v_enum.value);
								break;
							case SHARCS_FEATURE_SWITCH:
								packet_append32(p2,f->feature_value.v_switch.state);
								break;
							case SHARCS_FEATURE_RANGE:
								packet_append32(p2,f->feature_value.v_range.start);
								packet_append32(p2,f->feature_value.v_range.end);
								packet_append32(p2,f->feature_value.v_range.value);
								break;
						}
					}		
				}
			}
			
			/* update number of modules */
			packet_seek(p2,5);
			packet_append32(p2,i-1);
			
			/* send packet */
			sendPacket(con,p2);
			packet_delete(p2);
			
			break;
		}
		default: {
			fprintf(stdout,"[NET] received unhandled packet 0x%02x, size %d from client #%u\n",con->id,packetType,packetLen);
			break;
		}
	}
}

/*------------------------------------------ 
 * connection control 
 ------------------------------------------*/

int sharcs_connection_stop() {
	return 0;
}

int sharcs_connection_start() {
	fd_set ReadFDs, WriteFDs, ExceptFDs;

	time_t timePingCheck,timeNow;
	
	int res,maxSocket,i,client;
	struct sharcs_connection *connection;
	struct timeval tv;
	
	struct sockaddr_in addr;
	struct sharcs_packet *p;
	
	socklen_t len = sizeof(addr);
	
	/* initialize variables */
	stopEvent = 0;
	for(i=0;i<10;i++) {
		connections[i].connected = 0;
	}
	
	/* create pipe */
	pipe(pipeFD);

	/* Open up listener socket */
	serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8585);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	i = 1;
	if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i)) == -1) { 
	    perror("(setsockopt) "); 
	    return; 
	  }
	
	if(bind(serverSocket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		return 0;
	}

	/* Set a limit on connection queue */
	if(listen(serverSocket, 5) != 0) {
		perror("listen");
		return 0;
	}

	fprintf(stdout,"[NET] Server started..\n");
	
	timePingCheck = time(NULL);

	/* run loop */
	while(!stopEvent) {

		maxSocket = serverSocket;
		tv.tv_sec 	= 30;
		tv.tv_usec 	= 0;

		FD_ZERO(&ReadFDs);
		FD_ZERO(&WriteFDs);
		FD_ZERO(&ExceptFDs);

		FD_SET(serverSocket, &ReadFDs);

		FD_SET(pipeFD[0],&ReadFDs);

		pthread_mutex_lock(&mutex_connections);

		for(i=0;i<10;i++) {
			connection = &connections[i];
			if(!connection->connected) {
				continue;
			}
			
			FD_SET(connection->socket,&ReadFDs);
			if(connection->writeCounter > 0) {
				FD_SET(connection->socket,&WriteFDs);
			}
			FD_SET(connection->socket,&ExceptFDs);

			if(connection->socket > maxSocket) {
				maxSocket = connection->socket;
			}

		}

		pthread_mutex_unlock(&mutex_connections);

		res = select(maxSocket+1, &ReadFDs, &WriteFDs, &ExceptFDs, &tv);
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
			
			/* handle events from sockets */
			pthread_mutex_lock(&mutex_connections);
			for(i=0;i<10;i++) {
				connection = &connections[i];
				if(!connection->connected) {
					continue;
				}

				/* handle errors */
				if (FD_ISSET(connection->socket, &ExceptFDs)) {
					closeConnection(connection);
				}
				/* read data */
				if (FD_ISSET(connection->socket, &ReadFDs)) {
					readFromSocket(connection);
				} 
				/* write data */
				if (FD_ISSET(connection->socket, &WriteFDs)) {
					writeToSocket(connection);
				}
			}
			pthread_mutex_unlock(&mutex_connections);
			
			/* If master is set then someone is trying to connect */
			if(FD_ISSET(serverSocket, &ReadFDs)) {
				pthread_mutex_lock(&mutex_connections);
				for(i=0;i<10;i++) {
					connection = &connections[i];
					if(!connection->connected) {
						connection->socket 		= accept(serverSocket, (struct sockaddr *)&addr, &len);;
						connection->lastPing	= 
						connection->lastPong 	= time(NULL);

					    connection->writeCounter   	= 0;
					    connection->readCounter    	= 0;
					    connection->readBufferSize 	=
					    connection->writeBufferSize = 128;

					    connection->readBuffer     = (char*)malloc(connection->readBufferSize);
					    connection->writeBuffer    = (char*)malloc(connection->writeBufferSize);
						
						connection->connected = 1;
						connection->id = connectionId++;
						
						fprintf(stdout,"[NET] client #%u connected\n",connection->id);
						break;
					}
				}
				pthread_mutex_unlock(&mutex_connections);
			}
		}

		timeNow = time(NULL);
		if(timeNow - timePingCheck > 10) {
			timePingCheck = timeNow;
			
			p = packet_create();
			packet_append32(p,0);
			packet_append8(p,M_S_PING);
			packet_append64(p,timeNow);
			
			pthread_mutex_lock(&mutex_connections);
			for(i=0;i<10;i++) {
				connection = &connections[i];
				if(!connection->connected) {
					continue;
				}

				if(timeNow - connection->lastPing >= 15) {
					if(connection->lastPong < connection->lastPing) {
						closeConnection(connection);
					} else {
						sendPacket(connection,p);
						connection->lastPing = timeNow;
					}
				}
			}
			pthread_mutex_unlock(&mutex_connections);
			
			packet_delete(p);
		}
	}

	/* disconnect all clients */
	p = packet_create();
	packet_append32(p,0);
	packet_append8(p,M_S_DISCONNECT);
	
	pthread_mutex_lock(&mutex_connections);
	for(i=0;i<10;i++) {
		connection = &connections[i];
		if(!connection->connected) {
			continue;
		}

		sendPacket(connection,p);
		writeToSocket(connection);
		close(connection->socket);
	}
	pthread_mutex_unlock(&mutex_connections);

	packet_delete(p);

	close(serverSocket);
	close(pipeFD[0]);
	close(pipeFD[1]);
		
	return 1;
}

int sharcs_connection_feature(sharcs_id feature) {
	struct sharcs_connection *connection;
	struct sharcs_packet *p;
	struct sharcs_feature *f;
	int i;
	
	f = sharcs_feature(feature);
	if(!f) {
		return 0;
	}
	
	p = packet_create();
	packet_append32(p,0);
	
	switch(f->feature_type) {
		case SHARCS_FEATURE_ENUM:
			packet_append8(p,M_S_FEATURE_I);
			packet_append32(p,feature);
			packet_append32(p,f->feature_value.v_enum.value);
			break;
		case SHARCS_FEATURE_SWITCH:
			packet_append8(p,M_S_FEATURE_I);
			packet_append32(p,feature);
			packet_append32(p,f->feature_value.v_switch.state);
			break;
		case SHARCS_FEATURE_RANGE:
			packet_append8(p,M_S_FEATURE_I);
			packet_append32(p,feature);
			packet_append32(p,f->feature_value.v_range.value);
			break;
	}
	
	pthread_mutex_lock(&mutex_connections);
	for(i=0;i<10;i++) {
		connection = &connections[i];
		if(!connection->connected) {
			continue;
		}

		sendPacket(connection,p);
	}
	pthread_mutex_unlock(&mutex_connections);
	
	wakeUp();
	
	packet_delete(p);
}