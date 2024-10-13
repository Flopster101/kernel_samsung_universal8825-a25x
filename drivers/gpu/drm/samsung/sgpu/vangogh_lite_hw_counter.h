/*
* @file vangogh_lite_hw_counter.h
* @copyright 2020 Samsung Electronics
*/

#ifndef __VANGOGH_LITE_HW_COUNTER__
#define __VANGOGH_LITE_HW_COUNTER__

#include "sgpu_utilization.h"

uint64_t vangogh_lite_get_hw_utilization(struct amdgpu_device *adev);
void vangogh_lite_reset_hw_utilization(struct amdgpu_device *adev);
void vangogh_lite_hw_utilization_init(struct amdgpu_device *adev);
int vangogh_lite_get_hw_time(struct amdgpu_device *adev,
			     uint32_t *busy, uint32_t *total);
#endif
