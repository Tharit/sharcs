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
#include <sys/time.h>
#include <sys/select.h>

#include "../../../sharcs.h"
#include "av.h"

int module_id, device_id;
void (*sharcs_callback)(sharcs_id,void*);
pthread_t thread_handle;
int thread_stop;


static int enum_input[] = {
	AV_INPUT_DVD,
	AV_INPUT_VCR_DVR,
	AV_INPUT_CBL_SAT,
	AV_INPUT_GAME_TV,
	AV_INPUT_AUX1,
	AV_INPUT_AUX2,
	AV_INPUT_TAPE,
	AV_INPUT_TUNER,
	AV_INPUT_TUNER_FM,
	AV_INPUT_TUNER_AM,
	AV_INPUT_CD,
	AV_INPUT_PHONO,
	0xFFFF
};

static const char* enum_input_names[] = {
	"DVD",
	"VCR/DVR",
	"CBL/SAT",
	"Game/TV",
	"Aux1",
	"Aux2",
	"Tape",
	"Tuner",
	"Tuner(AM)",
	"CD",
	"Phono"
};


static int enum_mode[] = {
	AV_MODE_PURE_AUDIO,
	AV_MODE_DIRECT,
	AV_MODE_STEREO,
	AV_MODE_SURROUND,
	AV_MODE_ALL_CH_STEREO,
	AV_MODE_FILM,
	AV_MODE_THX,
	AV_MODE_ACTION,
	AV_MODE_MUSICAL,
	AV_MODE_MONO_MOVIE,
	AV_MODE_ORCHESTRA,
	AV_MODE_UNPLUGGED,
	AV_MODE_STUDIO_MIX,
	AV_MODE_TV_LOGIC,
	AV_MODE_THEATER,
	AV_MODE_ENHANCED7,
	AV_MODE_MONO,
	AV_MODE_FULL_MONO,
	AV_MODE_PL_MOVIE,
	AV_MODE_PL_MUSIC,
	AV_MODE_NEO_CINEMA,
	AV_MODE_NEO_MUSIC,
	AV_MODE_NEO_THX_CINEMA,
	AV_MODE_PL_THX_CINEMA,
	AV_MODE_PL_GAME,
	AV_MODE_NEURAL_THX,
	0xFFFF
};


static const char* enum_mode_names[] = {
	"Pure Audio",
	"Direct",
	"Stereo",
	"Surround",
	"All Ch Stereo",
	"Film",
	"THX",
	"Action",
	"Musical",
	"Mono Movie",
	"Orchestra",
	"Unplugged",
	"Studio Mix",
	"TV Logic",
	"Theater",
	"Enhanced7",
	"Mono",
	"Full Mono",
	"PLII Movie",
	"PLII Music",
	"Neo6: Cinema",
	"Neo6: music",
	"Neo THX Cinema",
	"PLII THX Cinema",
	"PLII Game",
	"Neural THX 5.1",
};

static int enum_dimmer[] = {
	AV_DIMMER_BRIGHTEST,
	AV_DIMMER_BRIGHT,
	AV_DIMMER_DIM,
	AV_DIMMER_DARK,
	0xFFFF
};

static const char* enum_dimmer_names[] = {
	"Brightest",
	"Bright",
	"Dim",
	"Dark"
};


void av_callback(int cmd, int v) {
	int *e;
	
	if(cmd == AV_CMD_MODE) {
		e = enum_mode;
	} else if(cmd == AV_CMD_INPUT) {
		e = enum_input;
	} else if(cmd == AV_CMD_DIMMER) {
		e= enum_dimmer;
	}
	if(e) {
		int i;
		
		for(i=0;e[i]!=0xFFFF;i++) {
			if(e[i]==v) {
				v = i;
				break;
			}
		}
	}
	
	sharcs_callback(SHARCS_ID_FEATURE_MAKE(module_id,device_id,cmd+1),&v);
}

void *module_thread(void *threadid) {
	int ret;
	
	while(!thread_stop && (ret=av_main())) {
	}
	
	if(!ret) {
		fprintf(stderr,"mod_onkyo_av: %s\n",av_error());
	}
	
	pthread_exit(NULL);
}

int module_start() {
	if(thread_handle) {
		return 0;
	}
	
	int ret = 0;
	
	/* tty mode
	if((ret = av_init_tty("/dev/tty.usbserial-FTFRUS14",&av_callback)) != 1) {
		fprintf(stderr,"mod_onkyo_av: %s\n",av_error());
		return 0;
	}
	*/
	
	/* libftdi mode */
	if((ret = av_init_libftdi(0x0403, 0x6001,NULL,NULL,0,&av_callback)) != 1) {
		fprintf(stderr,"mod_onkyo_av: %s\n",av_error());
		return 0;
	}
	
	av_req(AV_CMD_ALL);
	
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
	
	av_stop();
	
	return 1;
}

int module_set_i(sharcs_id feature, int value) {
	int cmd = SHARCS_INDEX_FEATURE(feature)-1;
	if(cmd == AV_CMD_MODE) {
		return av_seti(cmd,enum_mode[value]);
	} else if(cmd == AV_CMD_INPUT) {
		return av_seti(cmd,enum_input[value]);
	} else if(cmd == AV_CMD_DIMMER) {
		return av_seti(cmd,enum_dimmer[value]);
	}
	return av_seti(cmd,value);
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
	device->device_name 			= "TX-SR875";
	device->device_description 		= "AV-Receiver";
	device->device_features_size 	= AV_NUM_COMMANDS;
	device->device_features 		= (struct sharcs_feature**)malloc(sizeof(struct sharcs_feature*)*AV_NUM_COMMANDS);
	
	/* add features */
	for(i=0;i<AV_NUM_COMMANDS;i++) {
		feature = (struct sharcs_feature*)malloc(sizeof(struct sharcs_feature));
		feature->feature_id 		= SHARCS_ID_FEATURE_MAKE(mod->module_id,device_id,i+1);
		feature->feature_flags		= 0;
		device->device_features[i]	= feature;
		
		switch(i) {
			case AV_CMD_POWER:
				feature->feature_name 					= "Power";
				feature->feature_description 			= "toggle device power state";
				feature->feature_flags					= SHARCS_FLAG_POWER;
				feature->feature_type 					= SHARCS_FEATURE_SWITCH;
				feature->feature_value.v_switch.state 	= SHARCS_VALUE_UNKNOWN;
				break;
			case AV_CMD_MUTE:
				feature->feature_name 					= "Mute";
				feature->feature_description 			= "toggle mute";
				feature->feature_type 					= SHARCS_FEATURE_SWITCH;
				feature->feature_value.v_switch.state 	= SHARCS_VALUE_UNKNOWN;
				break;
			case AV_CMD_INPUT:
				feature->feature_name 					= "Input";
				feature->feature_description 			= "select av input";
				feature->feature_type 					= SHARCS_FEATURE_ENUM;
				feature->feature_value.v_enum.value		= SHARCS_VALUE_UNKNOWN;
				feature->feature_value.v_enum.size		= 11;
				feature->feature_value.v_enum.values	= (const char**)enum_input_names;
				break;
			case AV_CMD_MODE:
				feature->feature_name 					= "Listening Mode";
				feature->feature_description 			= "listening mode";
				feature->feature_type 					= SHARCS_FEATURE_ENUM;
				feature->feature_value.v_enum.value		= SHARCS_VALUE_UNKNOWN;
				feature->feature_value.v_enum.size		= 26;
				feature->feature_value.v_enum.values	= (const char**)enum_mode_names;
				break;
			case AV_CMD_VOLUME:
				feature->feature_name 					= "Volume";
				feature->feature_description 			= "main zone volume";
				feature->feature_type 					= SHARCS_FEATURE_RANGE;
				feature->feature_flags				 	= SHARCS_FLAG_SLIDER | SHARCS_FLAG_INVERSE;
				feature->feature_value.v_range.start 	= 0;
				feature->feature_value.v_range.end	 	= 60;
				feature->feature_value.v_range.value	= SHARCS_VALUE_UNKNOWN;
				break;
			case AV_CMD_LATENIGHT:
				feature->feature_name 					= "Late Night Mode";
				feature->feature_description 			= "adjust sound levels";
				feature->feature_type 					= SHARCS_FEATURE_RANGE;
				feature->feature_value.v_range.start 	= 0;
				feature->feature_value.v_range.end	 	= 2;
				feature->feature_value.v_range.value	= SHARCS_VALUE_UNKNOWN;
				break;
			case AV_CMD_DIMMER:
				feature->feature_name 					= "Dimmer";
				feature->feature_description 			= "set device illumination";
				feature->feature_type 					= SHARCS_FEATURE_ENUM;
				feature->feature_value.v_enum.value		= SHARCS_VALUE_UNKNOWN;
				feature->feature_value.v_enum.size		= 4;
				feature->feature_value.v_enum.values	= (const char**)enum_dimmer_names;
				
				break;
			case AV_CMD_PRESET:
				feature->feature_name 					= "Preset";
				feature->feature_description 			= "tuner preset";
				feature->feature_type 					= SHARCS_FEATURE_RANGE;
				feature->feature_value.v_range.start 	= 0;
				feature->feature_value.v_range.end	 	= 30;
				feature->feature_value.v_range.value	= SHARCS_VALUE_UNKNOWN;
				break;
		}
	}
	
	/* fill module structure */
	mod->module_name 			= "OnkyoAV";
	mod->module_description 	= "control Onkyo AV-Receiver via RS232";
	mod->module_version 		= AV_VERSION;
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