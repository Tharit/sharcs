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

int sharcs_init(int (*)(sharcs_id,int),int (*)(sharcs_id,const char*));
int sharcs_stop();

/* enumeration */
int sharcs_retrieve(void (*)(int));

int sharcs_enumerate_module(struct sharcs_module **module,int index);

struct sharcs_module* sharcs_module(sharcs_id id);
struct sharcs_device* sharcs_device(sharcs_id id);
struct sharcs_feature* sharcs_feature(sharcs_id id);

/* features */
int sharcs_set_i(sharcs_id,int);
int sharcs_set_s(sharcs_id,const char*);

/* profiles */
int sharcs_profile_save(const char *name);
int sharcs_profile_load(const char *name);