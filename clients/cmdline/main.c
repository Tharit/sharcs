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

#include "../libsharcs.h"

#include <stdio.h>
#include <stdlib.h>

int stop;
int v;
sharcs_id feature;

int callback_feature_i(sharcs_id id, int value) {
	if(feature == id) {
		if(value == SHARCS_VALUE_ERROR) {
			fprintf(stderr,"failed to set value\n");
		} else {
			fprintf(stdout,"value changed successfully\n");
		}
		exit(0);
	}
}

int callback_feature_s(sharcs_id id, const char* value) {
	if(feature == id) {
		exit(0);
	}
}


int main(int argc,char **argv) {
	sharcs_id moduleId;
	sharcs_id deviceId;
	
	if(argc<5) {
		return -1;
	}
	
	moduleId 	= SHARCS_ID_MODULE_MAKE(atoi(argv[1]));
	deviceId 	= SHARCS_ID_DEVICE_MAKE(moduleId,atoi(argv[1]));
	feature 	= SHARCS_ID_FEATURE_MAKE(
					moduleId,
					deviceId,
					atoi(argv[3])
			);
			
	v = atoi(argv[4]);
	
	sharcs_init(&callback_feature_i,&callback_feature_s);
	
	sharcs_set_i(feature,v);
	
	while(!stop) {
		sleep(1);
	}
	
	sharcs_stop();
	
	return 0;
}