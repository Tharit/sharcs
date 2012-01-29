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

#ifndef _PACKET_H_
#define _PACKET_H_

#ifndef HAVE_BYTE_SWAP_H
# ifndef bswap_64
extern unsigned long long bswap_64(unsigned long long v);
# endif
# ifndef bswap_32
extern unsigned long bswap_32(unsigned long v);
# endif
# ifndef bswap_16
extern unsigned short bswap_16(unsigned short v);
# endif
#endif

#ifndef uint8_t
#define uint8_t unsigned char
#endif

#ifndef uint16_t
#define uint16_t unsigned short
#endif

#ifndef uint32_t
#define uint32_t unsigned int
#endif

#ifndef uint64_t
#define uint64_t unsigned long long
#endif

struct sharcs_packet {
	char *data;
	int cursor;
	int size,bufferSize;
};
		
struct sharcs_packet* packet_create_buffer(const char *src, int len);
struct sharcs_packet* packet_create();
void packet_free(struct sharcs_packet *packet);

void packet_append8(struct sharcs_packet *packet,uint8_t byte);
void packet_append16(struct sharcs_packet *packet,uint16_t word);
void packet_append32(struct sharcs_packet *packet,uint32_t dword);
void packet_append64(struct sharcs_packet *packet,uint64_t qword);
void packet_append_string(struct sharcs_packet *packet,const char *string);
void packet_append_float(struct sharcs_packet *packet,float f);
         
uint8_t packet_read8(struct sharcs_packet *packet);
uint16_t packet_read16(struct sharcs_packet *packet);
uint32_t packet_read32(struct sharcs_packet *packet);
uint64_t packet_read64(struct sharcs_packet *packet);
const char* packet_read_string(struct sharcs_packet *packet);
float packet_read_float(struct sharcs_packet *packet);

const char* packet_buffer(struct sharcs_packet *packet);

int packet_size(struct sharcs_packet *packet);
void packet_seek(struct sharcs_packet *packet,int pos);
void packet_dump(struct sharcs_packet *packet);

#endif