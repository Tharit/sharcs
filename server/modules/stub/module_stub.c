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
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string>

#include "../../../sharcs.h"

int module_id, device_id;
void (*sharcs_callback)(sharcs_id,void*);

int module_start() {
	return 1;
}

int module_stop() {
	return 1;
}

int module_set_i(sharcs_id feature, int value) {
	int v;
	
	v = value;
	
	sharcs_callback(feature,&v);
	
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
	char buffer[128];
	
	device_id = SHARCS_ID_DEVICE_MAKE(mod->module_id,1);
	
	sprintf(buffer,"Stub (0x%X)",device_id);
	
	/* create device structure */
	device = (struct sharcs_device*)malloc(sizeof(struct sharcs_device));
	device->device_id 				= device_id;
	device->device_name 			= strdup(buffer);
	device->device_description 		= "Stub";
	device->device_features_size 	= 1;
	device->device_features 		= (struct sharcs_feature**)malloc(sizeof(struct sharcs_feature*)*1);
	
	/* add features */
	feature = (struct sharcs_feature*)malloc(sizeof(struct sharcs_feature));
	feature->feature_id 		= SHARCS_ID_FEATURE_MAKE(mod->module_id,device_id,1);
	feature->feature_flags		= 0;
	device->device_features[0]	= feature;

	feature->feature_name 					= "Power";
	feature->feature_description 			= "toggle device power state";
	feature->feature_flags					= SHARCS_FLAG_POWER;
	feature->feature_type 					= SHARCS_FEATURE_SWITCH;
	feature->feature_value.v_switch.state 	= SHARCS_VALUE_UNKNOWN;
			
	
	/* fill module structure */
	sprintf(buffer,"Stub (0x%X)",mod->module_id);
	mod->module_name 			= strdup(buffer);
	mod->module_description 	= "test module";
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