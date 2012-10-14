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

#include "../sharcs.h"

enum {
	LIBSHARCS_EVENT_RETRIEVE,
	LIBSHARCS_EVENT_PROFILES,
	LIBSHARCS_EVENT_PROFILE_DELETE,
	LIBSHARCS_EVENT_PROFILE_SAVE,
	LIBSHARCS_EVENT_PROFILE_LOAD,
};

int sharcs_init(const char* server,int (*)(sharcs_id,int),int (*)(sharcs_id,const char*),void (*)(int,int,int));
int sharcs_stop();

/* enumeration */
int sharcs_retrieve();
int sharcs_profiles();

int sharcs_enumerate_modules(struct sharcs_module **module,int index);
int sharcs_enumerate_profiles(struct sharcs_profile **profile,int index);

struct sharcs_module* sharcs_module(sharcs_id id);
struct sharcs_device* sharcs_device(sharcs_id id);
struct sharcs_feature* sharcs_feature(sharcs_id id);
struct sharcs_profile* sharcs_profile(int id);

/* features */
int sharcs_set_i(sharcs_id,int);
int sharcs_set_s(sharcs_id,const char*);

/* profiles */
int sharcs_profile_save(struct sharcs_profile *profile);
int sharcs_profile_load(int profile_id);
int sharcs_profile_delete(int profile_id);