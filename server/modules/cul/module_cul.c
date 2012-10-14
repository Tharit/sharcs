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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../sharcs.h"
#include "../../tty.h"

int module_id, device_id;
void (*sharcs_callback)(sharcs_id,void*);
pthread_t thread_handle;
int thread_stop;
TTYCTX tty_ctx;

void tty_callback(const char *s, int len) {
	
	fprintf(stdout,"[cul]: %s:%d\n",s,len);
	
	int v,i;
	if(!strcmp(s,"F758F0011\r")) {
		i = 2;
		v = 1;
	} else if(!strcmp(s,"F758F0000\r")) {
		i = 2;
		v = 0;
	} else if(!strcmp(s,"F758F0111\r")) {
		i = 1;
		v = 1;
	} else if(!strcmp(s,"F758F0100\r")) {
		i = 1;
		v = 0;
	} else {
		return;
	}
	sharcs_callback(SHARCS_ID_FEATURE_MAKE(module_id,device_id,i),&v);
}

void *module_thread(void *threadid) {
	int ret;
	
	ret = 1;
	
	while(!thread_stop && (ret=tty_main(tty_ctx))) {
	}
	
	if(!ret) {
		fprintf(stderr,"mod_cul: %s\n",tty_error(tty_ctx));
	}
	
	pthread_exit(NULL);
}

int module_start() {
	if(thread_handle) {
		return 0;
	}
	
	/* tty mode */
	if(!(tty_ctx = tty_init_tty("/dev/tty.usbmodemfa1441",&tty_callback))) {
		fprintf(stderr,"mod_cul: failed to initialize tty\n");
		return 0;
	}
	
	// activate listening mode
	tty_send(tty_ctx,"X01\r\n");
	
	thread_stop = 0;
	pthread_create(&thread_handle, NULL, module_thread, (void *)0);
	
	return 1;
}

int module_stop() {
	if(!thread_handle) {
		return 0;
	}
	
	thread_stop = 1;
	pthread_join(0,NULL);
	
	tty_stop(tty_ctx);
	
	return 1;
}

int module_set_i(sharcs_id feature, int value) {
	switch(SHARCS_INDEX_FEATURE(feature)) {
		case 2:
			if(value) {
				tty_send(tty_ctx,"F758F0011\r\n");
			} else {
				tty_send(tty_ctx,"F758F0000\r\n");
			}
			break;
		case 1:
			if(value) {
				tty_send(tty_ctx,"F758F0111\r\n");
			} else {
				tty_send(tty_ctx,"F758F0100\r\n");
			}
			break;
	}
	sharcs_callback(feature,&value);
	return 1;
}

int module_set_s(sharcs_id feature, const char *value) {
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

int sharcs_init(struct sharcs_module *mod, void (*cb)(sharcs_id,void *v)) {
	struct sharcs_device *device;
	struct sharcs_feature *feature;
	int i = 0;
	
	device_id = SHARCS_ID_DEVICE_MAKE(mod->module_id,1);
	
	thread_handle = 0;
	
	/* create device structure */
	device = (struct sharcs_device*)malloc(sizeof(struct sharcs_device));
	device->device_id 				= device_id;
	device->device_name 			= "Light";
	device->device_description 		= "CULV3";
	device->device_features_size 	= 2;
	device->device_features 		= (struct sharcs_feature**)malloc(sizeof(struct sharcs_feature*)*2);
	
	for(i=0;i<2;i++) {
		feature = (struct sharcs_feature*)malloc(sizeof(struct sharcs_feature));
		feature->feature_id 		= SHARCS_ID_FEATURE_MAKE(mod->module_id,device_id,1+i);
		feature->feature_flags		= 0;
		device->device_features[i]	= feature;
		
		if(i==0) {
			feature->feature_name = "Ceiling";
		} else {
			feature->feature_name = "Desk";
		}
		feature->feature_description 			= "toggle light";
		feature->feature_flags					= SHARCS_FLAG_POWER;
		feature->feature_type 					= SHARCS_FEATURE_SWITCH;
		feature->feature_value.v_switch.state 	= SHARCS_VALUE_UNKNOWN;
	}
	
	/* fill module structure */
	mod->module_name 			= "CUL";
	mod->module_description 	= "control RF devices via CUL dongle";
	mod->module_version 		= "1.0";
	mod->module_devices_size 	= 1;
	mod->module_devices 		= (struct sharcs_device**)malloc(sizeof(struct sharcs_device*)*1);
	mod->module_devices[0]		= device;
	mod->module_start 			= &module_start;
	mod->module_stop 			= &module_stop;
	mod->module_set_i			= &module_set_i;
	mod->module_set_s 			= &module_set_s;
	
	module_id = mod->module_id;
	
	/* store callback function */
	sharcs_callback = cb;
	
	return 1;
}

#ifdef __cplusplus
}
#endif
