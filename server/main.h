#ifndef _MAIN_H_
#define _MAIN_H_

int sharcs_enumerate_module(struct sharcs_module **module,int index);

struct sharcs_module* sharcs_module(sharcs_id id);
struct sharcs_device* sharcs_device(sharcs_id id);
struct sharcs_feature* sharcs_feature(sharcs_id id);
struct sharcs_profile* sharcs_profile(int id);

int sharcs_set_i(sharcs_id feature,int value);
int sharcs_set_s(sharcs_id feature,const char* value);

/* profiles */
int sharcs_enumerate_profiles(struct sharcs_profile **profile,int index);

int sharcs_profile_save(struct sharcs_profile *profile);
int sharcs_profile_load(int profile_id);
int sharcs_profile_delete(int profile_id);

#endif