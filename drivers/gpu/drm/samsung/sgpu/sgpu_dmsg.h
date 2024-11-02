/*
* @file sgpu_dmsg.h
* @copyright 2021 Samsung Electronics
*/
#ifndef __SGPU_DMSG_H__
#define __SGPU_DMSG_H__

struct amdgpu_device;

#define SGPU_DMSG_ENTITY_LENGTH	4096
#define SGPU_DMSG_LOG_LENGTH	256

struct sgpu_dmsg {
	char log[SGPU_DMSG_ENTITY_LENGTH][SGPU_DMSG_LOG_LENGTH];
};

enum sgpu_dmsg_func {
	DMSG_DVFS	= (1<<0),
	DMSG_POWER	= (1<<1),
	DMSG_MEMORY	= (1<<2),
	DMSG_ETC	= (1<<3),
	DMSG_FUNCMASK	= (1<<4) - 1,
};

enum sgpu_dmsg_level {
	DMSG_LEVEL_MIN = 0,
	DMSG_DEBUG = 0,
	DMSG_INFO,
	DMSG_WARNING,
	DMSG_ERROR,
	DMSG_LEVEL_MAX,
};

#define SGPU_LOG(adev, level, func, fmt, ...)						\
do {											\
	if (adev && adev->sgpu_dmsg &&							\
	    (level >= adev->sgpu_dmsg_level) &&						\
	    (func & adev->sgpu_dmsg_funcmask)) {					\
		ktime_t time = ktime_get();						\
		int index = atomic64_inc_return(&adev->sgpu_dmsg_index) %		\
			    SGPU_DMSG_ENTITY_LENGTH;					\
		sgpu_dmsg_log(adev, __func__, func, time, index, fmt, ##__VA_ARGS__);	\
	}										\
} while (0)

void sgpu_dmsg_log(struct amdgpu_device *adev, const char *caller, int func,
		   ktime_t time, int index, const char *fmt, ...);
int sgpu_debugfs_dmsg_init(struct amdgpu_device *adev);
void sgpu_debugfs_dmsg_fini(struct amdgpu_device *adev);
int sgpu_dmsg_init(struct amdgpu_device *adev);
void sgpu_dmsg_fini(struct amdgpu_device *adev);

#endif /*__SGPU_DMSG_H__ */
