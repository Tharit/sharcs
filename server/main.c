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
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include <dlfcn.h>

#include "../sharcs.h"

int modules_size = 0;
struct sharcs_module modules[10];
void* modules_lib_handle[10];

/*-----------------------------------
 * threadsafe API
 *-----------------------------------
 */
int sharcs_enumerate_modules(struct sharcs_module **module,int index) {
	if(index<0||index>=modules_size) {
		*module = NULL;
		return 0;
	}
	
	*module = &modules[index];
	
	return 1;
}

struct sharcs_module* sharcs_module(sharcs_id id) {
	int i = 0;
	for(i=0;i<modules_size;i++) {
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

int sharcs_set_i(sharcs_id feature,int value) {
	struct sharcs_module *m;
	struct sharcs_feature *f;
	
	m = sharcs_module(feature);
	if(!m || !(f=sharcs_feature(feature))) {
		return 0;
	}
	
	/* better debug output */
	switch(f->feature_type) {
		case SHARCS_FEATURE_ENUM:
			if(value<0||value>=f->feature_value.v_enum.size) {
				fprintf(stderr,"value of of bounds for enum '%s'\n",f->feature_name);
				return 0;
			}
			fprintf(stdout,">> set feature '%s' to '%s'\n",f->feature_name,f->feature_value.v_enum.values[value]);
			break;
		case SHARCS_FEATURE_SWITCH:
			if(value<0||value>1) {
				fprintf(stderr,"value of of bounds for switch '%s'\n",f->feature_name);
				return 0;
			}
			fprintf(stdout,">> set feature '%s' to '%d'\n",f->feature_name,value);
			break;
		case SHARCS_FEATURE_RANGE:
			if(value<f->feature_value.v_range.start||value>f->feature_value.v_range.end) {
				fprintf(stderr,"value of of bounds for range '%s'\n",f->feature_name);
				return 0;
			}
			fprintf(stdout,">> set feature '%s' to '%d'\n",f->feature_name,value);
			break;
		default:
			fprintf(stderr,"unknown feature type '%d'\n",f->feature_type);
			return 0;
	}
	
	return m->module_set_i(feature,value);
}

int sharcs_set_s(sharcs_id feature,const char* value) {
	struct sharcs_module *m;
	struct sharcs_feature *f;
	
	m = sharcs_module(feature);
	if(!m || !(f=sharcs_feature(feature))) {
		return 0;
	}

	return 0;
	
	/* 
	 * might be required for additional value types
	
	//return m->module_set_s(feature,value);
	
	 */
}

void sharcs_callback_feature(sharcs_id id,void *v) {
	struct sharcs_feature *f;
	int i = 0;
	
	f = sharcs_feature(id);
	if(f) {
		switch(f->feature_type) {
		case SHARCS_FEATURE_ENUM:
			fprintf(stdout,"<< feature '%s' changed to '%s'\n",f->feature_name,f->feature_value.v_enum.values[*((int*)v)]);
			f->feature_value.v_enum.value = *((int*)v);
			break;
		case SHARCS_FEATURE_SWITCH:
			fprintf(stdout,"<< feature '%s' changed to '%d'\n",f->feature_name,*((int*)v));
			f->feature_value.v_switch.state = *((int*)v);
			break;
		case SHARCS_FEATURE_RANGE:
			fprintf(stdout,"<< feature '%s' changed to '%d'\n",f->feature_name,*((int*)v));
			f->feature_value.v_range.value =*((int*)v);
			break;
		}
	}
	
	/* notify connection handler */
	sharcs_connection_feature(id);
}

int sharcs_module_load(const char *module_name) {
	void *lib_handle;
	char *error;
	int (*fn)(struct sharcs_module *mod, void (*cb)(sharcs_id,void*));
	int r;
	
	struct sharcs_module *module;
	module = &modules[modules_size++];
	module->module_id = SHARCS_ID_MODULE_MAKE(modules_size);
	
	lib_handle = dlopen(module_name, RTLD_LAZY);
	if (!lib_handle)  {
		fprintf(stderr, "%s\n", dlerror());
		return 0;
	}

	fn = dlsym(lib_handle, "sharcs_init");
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "%s\n", error);
		return 0;
	}

	r = (*fn)(module,&sharcs_callback_feature);
	if(!r) {
		fprintf(stderr,"error loading module '%s'\n",module_name);
		return 0;
	}
	
	fprintf(stdout,"Initializing module '%s' with %d devices...\n",module->module_name,module->module_devices_size);
	
	r = module->module_start();
	
	modules_lib_handle[modules_size-1] = lib_handle;
	
	return module->module_id;
}

void sharcs_shutdown() {
	int i;
	
	for(i=0;i<modules_size;i++) {
		modules[i].module_stop();
		dlclose(modules_lib_handle[i]);
	}
}

/*-----------------------------------*/

int main(int argc, char **argv) {
	char *path_home;
	
	/* get path of home directory */
	if(!(path_home = getenv("HOME"))) {
		struct passwd *pw;
		pw = getpwuid(getuid());
		path_home = pw->pw_dir;
	}
	
	/* read configuration */
	
	/* start daemon */
	sharcs_module_load("mod_onkyo_av.so");
	
	sharcs_connection_start();
	
	sharcs_shutdown();
	
	return 0;
}
