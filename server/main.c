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
#include <fcntl.h>
#include <signal.h>
#include <sys/param.h>

#include <pthread.h>

#include "../sharcs.h"
#include "../packet.h"

#define EACTIVE -1

/* modules */
int modules_size = 0;
struct sharcs_module modules[10];
void* modules_lib_handle[10];

/* profiles */
struct sharcs_profile **profiles,*profile_pending;
int profiles_size,profile_queue;
pthread_mutex_t mutex_profile;

/* env variables */
char *path_binary;

void profile_advance();
void profiles_load();

/*-----------------------------------*/

void profiles_save() {
	struct sharcs_packet *p;
	struct sharcs_profile *profile;
	FILE *f;
	int i,j;

	f = fopen ("/etc/sharcsd/profiles", "wb");
	if(f) {
		p = packet_create();
		/* serialize all profiles */
		packet_append8(p,0);
		
		i = 0;
		while(sharcs_enumerate_profiles(&profile,i++)) {
			packet_append32(p,profile->profile_id);
			packet_append_string(p,profile->profile_name);

			packet_append32(p,profile->profile_size);
			for(j=0;j<profile->profile_size;j++) {
				packet_append32(p,profile->profile_features[j]);
				packet_append32(p,profile->profile_values[j]);
			}
		}

		/* update number of profiles */
		packet_seek(p,0);
		packet_append8(p,i-1);

		fwrite(p->data,p->size,1,f);
		fclose(f);

		packet_delete(p);
	}
}

void profiles_load() {
	struct sharcs_packet *p;
	struct sharcs_profile *profile;
	char *buffer;
	long length;
	FILE *f;
	int i,j;

	if(profiles) {
		fprintf(stderr,"[Profile] error loading profiles, profiles already populated");
		return;
	}
	
	f = fopen ("/etc/sharcsd/profiles", "rb");
	buffer = 0;

	if (f) {
		fseek (f, 0, SEEK_END);
		length = ftell (f);
		fseek (f, 0, SEEK_SET);
		buffer = malloc (length);
		if (buffer) {
			fread (buffer, 1, length, f);
		}
		fclose (f);
	}

	if (buffer) {
		p = packet_create_buffer(buffer,length);
		
		/* load all profiles */
		profiles_size = packet_read8(p);
			
		profiles = (struct sharcs_profile**)malloc(sizeof(struct sharcs_profile*)*profiles_size);
			
		for(i=0;i<profiles_size;i++) {
			profile = (struct sharcs_profile*)malloc(sizeof(struct sharcs_profile));
			profile->profile_id 			= packet_read32(p);
			profile->profile_name 			= strdup(packet_read_string(p));
			profile->profile_size			= packet_read32(p);
			profile->profile_features 		= (int*)malloc(sizeof(int)*profile->profile_size);
			profile->profile_values 		= (int*)malloc(sizeof(int)*profile->profile_size);

			for(j=0;j<profile->profile_size;j++) {
				profile->profile_features[j] 	= packet_read32(p);
				profile->profile_values[j] 		= packet_read32(p);
			}
			
			profiles[i] = profile;
		}
		
		packet_delete(p);
	}
}


void profile_advance() {
	struct sharcs_profile *p;
	int i,r;
	
	p = profile_pending;
	i = profile_queue;
	
	while(i<p->profile_size) {
		fprintf(stdout,"[Profile] step %d/%d\n",i+1,p->profile_size);
	
		profile_queue = i+1;
		
		if((r=sharcs_set_i(p->profile_features[i],p->profile_values[i])) == 1) {
			return;
		}
		
		/* invalid value for feature.. cancel profile */ 
		if(r==0) {
			sharcs_connection_profile(p->profile_id,SHARCS_PROFILE_FAILED);
			fprintf(stdout,"[Profile] failed at step %d/%d!\n",i+1,p->profile_size);
			profile_pending = NULL;
			return;
		} else if(r==EACTIVE) {
			fprintf(stdout,"[Profile] step %d/%d skipped!\n",i+1,p->profile_size);
		}
		i++;
	}
	
	if(i>=p->profile_size) {
		sharcs_connection_profile(p->profile_id,SHARCS_PROFILE_LOADED);
		fprintf(stdout,"[Profile] finished!\n");
		profile_pending = NULL;
	}
}

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

struct sharcs_profile* sharcs_profile(int id) {
	int i;
	for(i=0;i<profiles_size;i++) {
		if(profiles[i]->profile_id == id) {
			return profiles[i];
		}
	}
	return NULL;
}

/* profiles */
int sharcs_enumerate_profiles(struct sharcs_profile **profile,int index) {
	if(index<0||index>=profiles_size) {
		*profile = NULL;
		return 0;
	}
	
	*profile = profiles[index];
	
	return 1;
}

int sharcs_profile_save(struct sharcs_profile *profile) {
	int i,last;
	
	pthread_mutex_lock(&mutex_profile);
	
	/* assign id if zero */
	if(!profile->profile_id) {
		last = 0;
		for(i=0;i<profiles_size;i++) {
			if(profiles[i]->profile_id > last) {
				last = profiles[i]->profile_id;
			}
		}
		fprintf(stdout,"[Profile] created profile with new id %d\n",last+1);
		profile->profile_id = last+1;
	} else {
		/* check if an existing profile should be replaced */
		for(i=0;i<profiles_size;i++) {
			if(profiles[i]->profile_id == profile->profile_id) {
				
				if(profiles[i] == profile_pending) {
					return 0;
				}
				
				fprintf(stdout,"[Profile] updated profile with id %d\n",profile->profile_id);
		

				free((void*)profiles[i]->profile_name);
				free(profiles[i]->profile_features);
				free(profiles[i]->profile_values);
				free(profiles[i]);
				break;
			}
		}
	}

	/* if a new profile was created, make room for it */
	if(i>=profiles_size) {
		profiles_size++;
		profiles = (struct sharcs_profile**)realloc(profiles,sizeof(struct sharcs_profile*)*profiles_size);
	}
	
	profiles[i] = profile;
	
	profiles_save();

	pthread_mutex_unlock(&mutex_profile);
	
	return 1;
}

int sharcs_profile_load(int profile_id) {
	struct sharcs_profile *p;
	
	if(profile_pending || !(p = sharcs_profile(profile_id))) {
		return 0;
	}
	
	/** 
	@TODO 
	figure out a way to determine which devices should be switched off because they are not used in this profile
	or should this be handled manually by the user? (e.g. adding it as a feature for each device individually)
	*/
	
	pthread_mutex_lock(&mutex_profile);
	
	profile_queue 	= 0;
	profile_pending = p;
	
	profile_advance();
	
	pthread_mutex_unlock(&mutex_profile);
	
	if(profile_pending) {
		sharcs_connection_profile(p->profile_id,SHARCS_PROFILE_LOADING);
	}
	
	return 1;
}

int sharcs_profile_delete(int profile_id) {
	int i,res;
	
	res = 0;
	pthread_mutex_lock(&mutex_profile);

	for(i=0;i<profiles_size;i++) {
		if(profiles[i]->profile_id == profile_id) {
	
			if(profiles[i] == profile_pending) {
				return 0;
			}
			
			free((void*)profiles[i]->profile_name);
			free(profiles[i]->profile_features);
			free(profiles[i]->profile_values);
			free(profiles[i]);
			
			fprintf(stdout,"[Profile] deleted profile with id %d\n",profile_id);

			profiles_size--;
			for(;i<profiles_size;i++) {
				profiles[i] = profiles[i+1];
			}
			
			profiles = (struct sharcs_profile**)realloc(profiles,sizeof(struct sharcs_profile*)*profiles_size);
			
			profiles_save();

			res = 1;
			break;
		}
	}
	
	pthread_mutex_unlock(&mutex_profile);

	return res;
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
			/* check if value is already set */
			if(SHARCS_V_ENUM(f)==value) {
				return EACTIVE;
			}
			
			/* validate value */
			if(value<0||value>=f->feature_value.v_enum.size) {
				fprintf(stderr,"value of of bounds for enum '%s'\n",f->feature_name);
				return 0;
			}
			
			fprintf(stdout,">> set feature '%s' to '%s'\n",f->feature_name,f->feature_value.v_enum.values[value]);
			break;
		case SHARCS_FEATURE_SWITCH:
			/* check if value is already set */
			if(SHARCS_V_SWITCH(f)==value) {
				return EACTIVE;
			}
			/* validate value */
			if(value<0||value>1) {
				fprintf(stderr,"value out of bounds for switch '%s'\n",f->feature_name);
				return 0;
			}
			
			fprintf(stdout,">> set feature '%s' to '%d'\n",f->feature_name,value);
			break;
		case SHARCS_FEATURE_RANGE:
			/* check if value is already set */
			if(SHARCS_V_RANGE(f)==value) {
				return EACTIVE;
			}
		
			/* validate value */
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
			if(f->feature_flags & SHARCS_FLAG_POWER) {
				if(!*((int*)v)) {
					sharcs_device(id)->device_flags |= SHARCS_FLAG_STANDBY;
				} else {
					sharcs_device(id)->device_flags &= ~SHARCS_FLAG_STANDBY;
				}
			}
			fprintf(stdout,"<< feature '%s' changed to '%d'\n",f->feature_name,*((int*)v));
			f->feature_value.v_switch.state = *((int*)v);
			break;
		case SHARCS_FEATURE_RANGE:
			fprintf(stdout,"<< feature '%s' changed to '%d'\n",f->feature_name,*((int*)v));
			f->feature_value.v_range.value = *((int*)v);
			break;
		}
	}
	
	if(profile_pending) { 
		/* @TODO missing some kind of tick, to handle timeouts...?! */
		pthread_mutex_lock(&mutex_profile);
		
		if(profile_pending && id == profile_pending->profile_features[profile_queue-1]) {
			
			if(*((int*)v) == profile_pending->profile_values[profile_queue-1]) {
				profile_advance();
			} else {
				sharcs_set_i(profile_pending->profile_features[profile_queue-1],
					profile_pending->profile_values[profile_queue-1]);
			}
		}
		
		pthread_mutex_unlock(&mutex_profile);
	}
	
	
	/* notify connection handler */
	sharcs_connection_feature(id);
}

int sharcs_module_load(const char *module_name) {
	void *lib_handle;
	char *error,*file;
	int (*fn)(struct sharcs_module *mod, void (*cb)(sharcs_id,void*));
	int r;
	
	struct sharcs_module *module;
	module = &modules[modules_size++];
	module->module_id = SHARCS_ID_MODULE_MAKE(modules_size);
	
	file = (char*)malloc(sizeof(char)*(strlen(module_name)+strlen(path_binary)+2));
	sprintf(file,"%s/%s",path_binary,module_name);
	
	lib_handle = dlopen(file, RTLD_LAZY);
	
	free(file);
	
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

void stop() {
	sharcs_shutdown();
	free(path_binary);	
}

void signal_handler(int sig) {
	switch(sig) {
	case SIGHUP:
		break;
	case SIGTERM:	
		stop();
		exit(0);
		break;
	}
}

#define RUNNING_DIR	"/tmp"
#define LOCK_FILE	"sharcsd.pid"

void daemonize() {
	int i,lfp;
	char str[10];
	
	if(getppid()==1) return; /* already a daemon */
	i=fork();
	if (i<0) exit(1); /* fork error */
	if (i>0) exit(0); /* parent exits */
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
	umask(027); /* set newly created file permissions */
	chdir(RUNNING_DIR); /* change running directory */
	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
	if (lfp<0) exit(1); /* can not open */
	if (lockf(lfp,F_TLOCK,0)<0) exit(0); /* can not lock */
	/* first instance continues */
	sprintf(str,"%d\n",getpid());
	write(lfp,str,strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,signal_handler); /* catch hangup signal */
	signal(SIGTERM,signal_handler); /* catch kill signal */	
}

int main(int argc, char **argv) {
	char path[MAXPATHLEN];
	int opt_daemonize = 1,c;
	pthread_mutexattr_t attr;
	
	/* get current directory */
	getcwd(path, MAXPATHLEN);
	path_binary = strdup(path);
	
	/*------------------------------------
	 * parse parameters
	 *------------------------------------*/
	while ((c = getopt (argc, argv, "f")) != -1) {
		switch(c) {
			case 'f':
				opt_daemonize = 0;
				break;
		}
	}
		
	argc-=optind;
	argv+=optind;
	
	/* initialize profiles */
	pthread_mutexattr_init(&attr); 
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); 
	pthread_mutex_init(&mutex_profile, &attr);
	
	profiles 		= 0;
	profiles_size 	= 0;
	
	profiles_load();

	/* detach */
	if(opt_daemonize) {
		daemonize();
	}
	
	sharcs_module_load("mod_cul.so");
	sharcs_module_load("mod_onkyo_av.so");
/*	sharcs_module_load("mod_stub.so");
	sharcs_module_load("mod_stub2.so");
	sharcs_module_load("mod_stub3.so");
*/	
	sharcs_connection_start();
	
	/* will only return here on error*/
	stop();
	
	return 0;
}
