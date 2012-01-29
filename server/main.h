#ifndef _MAIN_H_
#define _MAIN_H_

int sharcs_enumerate_module(struct sharcs_module **module,int index);

struct sharcs_module* sharcs_module(sharcs_id id);
struct sharcs_device* sharcs_device(sharcs_id id);
struct sharcs_feature* sharcs_feature(sharcs_id id);

int sharcs_set_i(sharcs_id feature,int value);
int sharcs_set_s(sharcs_id feature,const char* value);

#endif