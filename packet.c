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

#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#ifndef HAVE_BYTE_SWAP_H
# ifndef bswap_64
uint64_t bswap_64(uint64_t x)
{
#ifdef WIN32
	uint8_t *ptr = (uint8_t *) &x, tmp;
  tmp = ptr[0];
  ptr[0] = ptr[7];
  ptr[7] = tmp;
  tmp = ptr[1];
  ptr[1] = ptr[6];
  ptr[6] = tmp;
  tmp = ptr[2];
  ptr[2] = ptr[5];
  ptr[5] = tmp;
  tmp = ptr[3];
  ptr[3] = ptr[4];
  ptr[4] = tmp;
  return x;
#else 
	#ifndef INTEL86
		__asm("bswap	%0":
			  "=r" (x)     :
			  "0" (x));
		return x;
	#else
		register union { __extension__ uint64_t __ll;
			uint32_t __l[2]; } __x;
		asm("xchgl	%0,%1":
			"=r"(__x.__l[0]),"=r"(__x.__l[1]):
			"0"(bswap_32((unsigned long)x)),"1"(bswap_32((unsigned long)(x>>32))));
		return __x.__ll;
	#endif
#endif
}
# endif
# ifndef bswap_32
unsigned long bswap_32(unsigned long v)
{
    unsigned char c,*x=(unsigned char *)&v;
    c=x[0];x[0]=x[3];x[3]=c;
    c=x[1];x[1]=x[2];x[2]=c;
    return v;
}
# endif
# ifndef bswap_16
unsigned short bswap_16(unsigned short v)
{
    unsigned char c,*x=(unsigned char *)&v;
    c=x[0];x[0]=x[1];x[1]=c;
    return v;
}
# endif
#endif 


struct sharcs_packet* packet_create() {
	struct sharcs_packet *packet;
	
	packet = (struct sharcs_packet*)malloc(sizeof(struct sharcs_packet));
	
    packet->bufferSize    = 1024;
	packet->data          = (char*)malloc(sizeof(char)*packet->bufferSize);
	packet->cursor        = 0;
	packet->size          = 0;
	
	return packet;
}

struct sharcs_packet* packet_create_buffer(const char *src, int len) {
    struct sharcs_packet *packet;
	
	packet = (struct sharcs_packet*)malloc(sizeof(struct sharcs_packet));
	
	packet->bufferSize    = len;
	packet->data          = (char*)malloc(sizeof(char)*len);
	packet->cursor        = 0;
	packet->size          = len;

	memcpy(packet->data, src, len*sizeof(char));
	
	return packet;
}

void packet_delete(struct sharcs_packet *packet) {
	free(packet->data);
	free(packet);
}

#define ADVANCE(a)				\
	packet->cursor += a;				\
	if(packet->cursor > packet->size) {		\
		packet->size = packet->cursor;		\
	}

#define CHECKSIZE(a)                            \
    if(packet->bufferSize-packet->size < a) {                   \
        while(packet->bufferSize-packet->size < a) {            \
            packet->bufferSize*=2;                      \
        }                                       \
        packet->data = (char*)realloc(packet->data,packet->bufferSize); \
    }   

void packet_append8(struct sharcs_packet *packet,uint8_t byte) {
    CHECKSIZE(1);
	memcpy(packet->data+packet->cursor, &byte, sizeof(uint8_t));
	ADVANCE(sizeof(uint8_t));
}

void packet_append16(struct sharcs_packet *packet,uint16_t word) {
    CHECKSIZE(2);
	*(uint16_t *)(packet->data+packet->cursor) = bswap_16(word);
	ADVANCE(sizeof(uint16_t));
}

void packet_append32(struct sharcs_packet *packet,uint32_t dword) {
    CHECKSIZE(4);
	*(uint32_t *)(packet->data+packet->cursor) =bswap_32(dword);
	ADVANCE(sizeof(uint32_t));
}

void packet_append64(struct sharcs_packet *packet,uint64_t qword) {
    CHECKSIZE(8);

	*(uint64_t *)(packet->data+packet->cursor) =bswap_64(qword);
	ADVANCE(sizeof(uint64_t));
}

void packet_append_float(struct sharcs_packet *packet,float f) {
    CHECKSIZE(4);
	uint32_t dwValue = *((uint32_t*)(&f));
	packet_append32(packet,dwValue);
}

void packet_append_string(struct sharcs_packet *packet,const char *cstr) {
  	int len = strlen(cstr);
    CHECKSIZE(len+4);
	packet_append32(packet,len+4);
	memcpy(packet->data+packet->cursor, cstr, (len+1) * sizeof(char));
	ADVANCE(len);
}

uint8_t packet_read8(struct sharcs_packet *packet) {
	uint8_t val = packet->data[packet->cursor];
	ADVANCE(sizeof(uint8_t));
	return val;
}

uint16_t packet_read16(struct sharcs_packet *packet) {
	uint16_t val = bswap_16(*(uint16_t *)(packet->data+packet->cursor));
	ADVANCE(sizeof(uint16_t));
	return val;
}

uint32_t packet_read32(struct sharcs_packet *packet) {
	uint32_t val = bswap_32(*(uint32_t *)(packet->data+packet->cursor));
	ADVANCE(sizeof(uint32_t));
	return val;
}

uint64_t packet_read64(struct sharcs_packet *packet) {
	uint64_t val = bswap_64(*(uint64_t *)(packet->data+packet->cursor));
	ADVANCE(sizeof(uint64_t));
	return val;
}

const char* packet_read_string(struct sharcs_packet *packet) {
	int len = packet_read32(packet)-4;
	const char *cstr;
	cstr = packet->data+packet->cursor;
	
	ADVANCE(len);
	
	return cstr;
}

float packet_read_float(struct sharcs_packet *packet) {
	uint32_t dwValue = packet_read32(packet);
	return *((float*)(&dwValue));
}

const char* packet_buffer(struct sharcs_packet *packet) {
	return (const char*)packet->data;
}

int packet_size(struct sharcs_packet *packet) {
	return packet->size;
}

void packet_seek(struct sharcs_packet *packet,int pos) {
	if(pos < 0 || pos > packet->size) {
		return;
	}
	packet->cursor = pos;
}

void packet_dump(struct sharcs_packet *packet) {
	char conv[4];
	int i,j;
	
	for(i=0;i<(packet->size/16)+1;i++) {
		for(j=0;j<16;j++) {
			if(j==8) {
				printf("  ");
			}
			if((i*16)+j >= packet->size) {
				continue;
			}
			printf("%02X ", (unsigned char)packet->data[(i*16)+j]);
		}
		printf("\n");
	}
}
