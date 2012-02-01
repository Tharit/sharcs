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

static const char *types[] = {
	"unknown",
	"Range",
	"Switch",
	"Enum"
};

void callback_retrieve(int success) {
	int i,j,k,l;
	struct sharcs_module *m;
	struct sharcs_device *d;
	struct sharcs_feature *f;
	
	/* print tree */
	printf("Available devices/features:\n");
	
	i = 0;
	while(sharcs_enumerate_modules(&m,i++)) {
		printf("- [0x%X] %s\n",m->module_id,m->module_name);
		
		for(j=0;j<m->module_devices_size;j++) {
			d = m->module_devices[j];
			
			printf("  - [0x%X] %s\n",d->device_id,d->device_name);
			
			for(k=0;k<d->device_features_size;k++) {
				f = d->device_features[k];
				
				printf("    - [0x%X:%s] %s\n",f->feature_id,types[f->feature_type],f->feature_name);
				
				if(f->feature_type == SHARCS_FEATURE_ENUM) {
					for(l=0;l<f->feature_value.v_enum.size;l++) {
						printf("        - {%02d} %s\n",l,f->feature_value.v_enum.values[l]);
					}
				}
			}
		}
	}
			
	if(feature == 0) {
		sharcs_stop();
		exit(0);
	}
}

int callback_feature_i(sharcs_id id, int value) {
	if(feature == id) {
		if(value == SHARCS_VALUE_ERROR) {
			fprintf(stderr,"failed to set value\n");
		} else {
			fprintf(stdout,"value changed successfully\n");
		}
		sharcs_stop();
		exit(0);
	}
}

int callback_feature_s(sharcs_id id, const char* value) {
	if(feature == id) {
		sharcs_stop();
		exit(0);
	}
}


int main(int argc,char **argv) {
/*	sharcs_id moduleId;
	sharcs_id deviceId;
*/	
	if((argc == 2 && !strcmp(argv[1],"tree")) || argc == 1) {
		feature = 0;
	} else if(argc == 3) {
		/*	
		moduleId 	= SHARCS_ID_MODULE_MAKE(atoi(argv[1]));
		deviceId 	= SHARCS_ID_DEVICE_MAKE(moduleId,atoi(argv[1]));
		feature 	= SHARCS_ID_FEATURE_MAKE(
						moduleId,
						deviceId,
						atoi(argv[3])
						);
		*/
		if(!sscanf(argv[1],"0x%X",&feature) || !SHARCS_ID_TYPE(feature) == SHARCS_FEATURE) {
			printf("Invalid feature id %s\n",argv[1]);
			return 0;
		}
		v = atoi(argv[2]);
	} else {
		return 0;
	}
	
	sharcs_init(&callback_feature_i,&callback_feature_s);
	
	if(feature!=0) {
		sharcs_set_i(feature,v);
	} else {
		sharcs_retrieve(&callback_retrieve);
	}
	
	while(!stop) {
		sleep(1);
	}
	
	sharcs_stop();
	
	return 0;
}