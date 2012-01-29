#ifndef _SHARCS_H_
#define _SHARCS_H_

typedef int sharcs_id;

enum {
	SHARCS_MODULE = 1,
	SHARCS_DEVICE = 2,
	SHARCS_FEATURE = 3,
};

#define SHARCS_ID_TYPE(id) (id&0xFF000000)>>24
#define SHARCS_ID_MODULE(id) (SHARCS_MODULE<<24)+(id&0x00FF0000)
#define SHARCS_ID_DEVICE(id) (SHARCS_DEVICE<<24)+(id&0x00FFFF00)
#define SHARCS_ID_FEATURE(id) (SHARCS_FEATURE<<24)+(id&0x00FFFFFF)

#define SHARCS_INDEX_MODULE(id) (id&0x00FF0000)>>16
#define SHARCS_INDEX_DEVICE(id) (id&0x0000FF00)>>8
#define SHARCS_INDEX_FEATURE(id) (id&0x000000FF)

#define SHARCS_ID_MODULE_MAKE(m) ((SHARCS_MODULE<<24)+(m<<16))
#define SHARCS_ID_DEVICE_MAKE(m,d) ((SHARCS_DEVICE<<24)+(m&0x00FF0000)+(d<<8))
#define SHARCS_ID_FEATURE_MAKE(m,d,f) ((SHARCS_FEATURE<<24)+(m&0x00FF0000)+(d&0x0000FF00)+f)

/*
 * packets
 */
enum {
	M_S_DISCONNECT,
	M_S_FEATURE_I,
	M_S_FEATURE_S,
	M_S_FEATURE_ERROR,
	M_S_RETRIEVE,
	M_S_PING,
};

enum {
	M_C_PONG,
	M_C_FEATURE_I,
	M_C_FEATURE_S,
	M_C_RETRIEVE,
};

/*
 * features
 */
enum {
	SHARCS_FEATURE_RANGE 	= 0x1,
	SHARCS_FEATURE_SWITCH 	= 0x2,
	SHARCS_FEATURE_ENUM		= 0x3,
};

enum { 
	SHARCS_VALUE_ON 		= 0x1,
	SHARCS_VALUE_OFF 		= 0x0,
	SHARCS_VALUE_UNKNOWN 	= 0x0FFFFFFF,
	SHARCS_VALUE_ERROR		= 0x0EFFFFFF,
};

struct sharcs_feature_range {
	int start;
	int end;
	int value;
};

struct sharcs_feature_enum {
	int size;
	const char **values;
	int value;
};

struct sharcs_feature_switch {
	int state;
};

struct sharcs_feature {
	sharcs_id feature_id;
	
	const char *feature_name;
	const char *feature_description;
	
	int feature_type;
	
	union values {
	/* SHARCS_FEATURE_RANGE */
		struct sharcs_feature_range v_range;
		
	/* SHARCS_FEATURE_ENUM */
		struct sharcs_feature_enum v_enum;
	
	/* SHARCS_FEATURE_SWITCH */
		struct sharcs_feature_switch v_switch;
	} feature_value;
};

/*
 * devices
 */
struct sharcs_device {
	sharcs_id device_id;
	
	const char *device_name;
	const char *device_description;
	
	int device_features_size;
	struct sharcs_feature **device_features;
};

/*
 * modules
 */
struct sharcs_module {
	sharcs_id module_id;
	
	const char *module_name;
	const char *module_description;
	const char *module_version;
	
	int module_devices_size;
	struct sharcs_device **module_devices;
	
	int (*module_start)();
	int (*module_stop)();
	int (*module_set_i)(sharcs_id feature_id, int value);
	int (*module_set_s)(sharcs_id feature_id, const char *value);
};

#endif