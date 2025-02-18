/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 device interface functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-interface-wrap.h"
#include "is-interface-library.h"
#include "is-param.h"

int is_itf_s_param_wrap(struct is_device_ischain *device,
		IS_DECLARE_PMAP(pmap))
{
	struct is_hardware *hardware = NULL;
	u32 instance, ginstance;
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		ret = is_hardware_set_param(hardware, ginstance, device->is_region,
				pmap, hardware->hw_map[ginstance]);
		if (ret) {
			merr("is_hardware_set_param is fail(%d)", device, ret);
			return ret;
		}
	}

	return ret;
}

int is_itf_a_param_wrap(struct is_device_ischain *device, u64 group)
{
	struct is_hardware *hardware = NULL;
	u32 instance, ginstance;
	int ret;

	dbg_hw(2, "%s\n", __func__);

	if (IS_ENABLED(SKIP_SETFILE_LOAD))
		return 0;

	hardware = device->hardware;
	instance = device->instance;

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		ret = is_hardware_apply_setfile(hardware, ginstance,
				device->setfile & IS_SETFILE_MASK,
				hardware->hw_map[ginstance]);
		if (ret) {
			merr("is_hardware_apply_setfile is fail(%d)", device, ret);
			return ret;
		}
	}

	return 0;
}

int is_itf_f_param_wrap(struct is_device_ischain *device, u64 group)
{
	struct is_hardware *hardware= NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	return ret;
}

int is_itf_enum_wrap(struct is_device_ischain *device)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

void is_itf_storefirm_wrap(struct is_device_ischain *device)
{
	dbg_hw(2, "%s\n", __func__);

	return;
}

void is_itf_restorefirm_wrap(struct is_device_ischain *device)
{
	dbg_hw(2, "%s\n", __func__);

	return;
}

int is_itf_set_fwboot_wrap(struct is_device_ischain *device, u32 val)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_open_wrap(struct is_device_ischain *device, u32 flag)
{
	struct is_hardware *hardware;
	struct is_path_info *path;
	struct is_group *group;
	struct is_device_sensor *sensor;
	struct is_groupmgr *groupmgr;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 group_id = -1;
	u32 group_slot, group_slot_c;
	int ret = 0, ret_c = 0;
	int hw_list[GROUP_HW_MAX];
	int hw_index;
	int hw_maxnum = 0;
	u32 rsccount;
	u32 ginstance = 0, ginstance_c = 0;

	info_hw("%s: flag(%d) sen(%d)\n", __func__, flag);

	sensor   = device->sensor;
	instance = device->instance;
	hardware = device->hardware;
	groupmgr = device->groupmgr;

	/*
	 * CAUTION: The path has physical group id.
	 * And physical group is must be used at open commnad.
	 */
	path = &device->path;

	mutex_lock(&hardware->itf_lock);

	rsccount = atomic_read(&hardware->rsccount);

	if (rsccount == 0) {
		ret = is_init_ddk_thread();
		if (ret) {
			err("failed to create threads for DDK, ret %d", ret);
			mutex_unlock(&hardware->itf_lock);
			return ret;
		}

		atomic_set(&hardware->lic_updated, 0);
	}

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		for (group_slot = GROUP_SLOT_PAF; group_slot < GROUP_SLOT_MAX; group_slot++) {
			group = groupmgr->group[ginstance][group_slot];

			group_id = (instance == ginstance) ?
				path->group[group_slot] : path->group_2nd[group_slot];

			dbg_hw(1, "itf_open_wrap: group[SLOT_%d]=[%s]\n",
					group_slot, group_id_name[group_id]);

			hw_maxnum = is_get_hw_list(group_id, hw_list);
			for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
				hw_id = hw_list[hw_index];
				ret = is_hardware_open(hardware, hw_id, group, ginstance, flag);
				if (ret) {
					merr("is_hardware_open(%d) is fail", device, hw_id);
					goto hardware_close;
				}
			}

		}

		hardware->sensor_position[ginstance] = sensor->position;
	}

	atomic_inc(&hardware->rsccount);

	info("%s: done: hw_map[0x%lx][RSC:%d][%d]\n", __func__,
		hardware->hw_map[instance], atomic_read(&hardware->rsccount),
		hardware->sensor_position[instance]);

	mutex_unlock(&hardware->itf_lock);

	return ret;

hardware_close:
	ginstance_c = ginstance;
	group_slot_c = group_slot;

	for (ginstance = 0; ginstance <= ginstance_c; ginstance++) {
		u32 gslot_max = (ginstance == ginstance_c) ? group_slot_c : GROUP_SLOT_MAX;

		if (!test_bit(ginstance, &device->ginstance_map))
			continue;
		for (group_slot = GROUP_SLOT_PAF; group_slot <= gslot_max; group_slot++) {
			group_id = (instance == ginstance) ?
				path->group[group_slot] : path->group_2nd[group_slot];

			hw_maxnum = is_get_hw_list(group_id, hw_list);
			for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
				hw_id = hw_list[hw_index];
				info_hw("[%d][ID:%d]itf_close_wrap: call hardware_close(), (%s), ret(%d)\n",
						instance, hw_id, group_id_name[group_id], ret);
				ret_c = is_hardware_close(hardware, hw_id, ginstance);
				if (ret_c)
					merr("is_hardware_close(%d) is fail", device, hw_id);
			}
		}
	}

	mutex_unlock(&hardware->itf_lock);

	return ret;
}

int is_itf_close_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	struct is_path_info *path;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 group_id = -1;
	u32 group_slot;
	int ret = 0;
	int hw_list[GROUP_HW_MAX];
	int hw_index;
	int hw_maxnum = 0;
	u32 rsccount;
	u32 ginstance = 0;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	/*
	 * CAUTION: The path has physical group id.
	 * And physical group is must be used at close commnad.
	 */
	path = &device->path;

	mutex_lock(&hardware->itf_lock);

	rsccount = atomic_read(&hardware->rsccount);

	if (rsccount == 1)
		is_flush_ddk_thread();

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		if (!IS_ENABLED(SKIP_SETFILE_LOAD)) {
			ret = is_hardware_delete_setfile(hardware, instance,
					hardware->logical_hw_map[instance]);
			if (ret) {
				merr("is_hardware_delete_setfile is fail(%d)", device,
						ret);
				mutex_unlock(&hardware->itf_lock);
				return ret;
			}
		}

		for (group_slot = GROUP_SLOT_PAF; group_slot < GROUP_SLOT_MAX; group_slot++) {
			group_id = (instance == ginstance) ?
				path->group[group_slot] : path->group_2nd[group_slot];

			dbg_hw(1, "itf_close_wrap: group[SLOT_%d]=[%s]\n",
					group_slot, group_id_name[group_id]);
			hw_maxnum = is_get_hw_list(group_id, hw_list);
			for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
				hw_id = hw_list[hw_index];
				ret = is_hardware_close(hardware, hw_id, ginstance);
				if (ret)
					merr("is_hardware_close(%d) is fail", device, hw_id);
			}
		}
	}

	atomic_dec(&hardware->rsccount);

	info("%s: done: hw_map[0x%lx][RSC:%d]\n", __func__,
		hardware->hw_map[instance], atomic_read(&hardware->rsccount));

	for (hw_id = 0; hw_id < DEV_HW_END; hw_id++)
		clear_bit(hw_id, &hardware->hw_map[instance]);

	mutex_unlock(&hardware->itf_lock);

	return ret;
}

int is_itf_change_chain_wrap(struct is_device_ischain *device, struct is_group *group, u32 next_id)
{
	int ret = 0;
	struct is_hardware *hardware;

	FIMC_BUG(!device);

	hardware = device->hardware;

	mutex_lock(&hardware->itf_lock);

	ret = is_hardware_change_chain(hardware, group, group->instance, next_id);

	mutex_unlock(&hardware->itf_lock);

	mginfo("%s: %s (next_id: %d)\n", group, group, __func__,
		ret ? "fail" : "success", next_id);

	return ret;
}

int is_itf_setfile_wrap(struct is_interface *itf, ulong setfile_addr,
	struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance = 0, ginstance = 0;
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		if (!IS_ENABLED(SKIP_SETFILE_LOAD)) {
			ret = is_hardware_load_setfile(hardware, setfile_addr, ginstance,
					hardware->logical_hw_map[ginstance]);
			if (ret) {
				merr("is_hardware_load_setfile is fail(%d)", device,
						ret);
				return ret;
			}
		}
	}

	return ret;
}

int is_itf_map_wrap(struct is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_unmap_wrap(struct is_device_ischain *device, u32 group)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_stream_on_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance, ginstance;
	ulong hw_map;
	int ret = 0;
	struct is_group *group_leader;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	is_set_ddk_thread_affinity();

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		hw_map = hardware->hw_map[ginstance];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device));
		if (instance != ginstance)
			group_leader = group_leader->head->pnext;

		ret = is_hardware_sensor_start(hardware, ginstance, hw_map, group_leader);
		if (ret) {
			merr("is_hardware_stream_on is fail(%d)", device, ret);
			return ret;
		}
	}

	return ret;
}

int is_itf_stream_off_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance, ginstance;
	ulong hw_map;
	int ret = 0;
	struct is_group *group_leader;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		hw_map = hardware->hw_map[ginstance];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device));
		if (instance != ginstance)
			group_leader = group_leader->head->pnext;

		ret = is_hardware_sensor_stop(hardware, ginstance, hw_map, group_leader);
		if (ret) {
			merr("is_hardware_stream_off is fail(%d)", device, ret);
			return ret;
		}
	}

	return ret;
}

int is_itf_process_on_wrap(struct is_device_ischain *device, u64 group_ids)
{
	struct is_hardware *hardware;
	u32 instance = 0;
	u32 ginstance, group_slot, group_id;
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_path_info *path;

	hardware = device->hardware;
	instance = device->instance;
	groupmgr = device->groupmgr;
	path = &device->path;

	minfo_hw("itf_process_on_wrap: [G:0x%lx]\n", instance, group_ids);

	mutex_lock(&hardware->itf_lock);

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		for (group_slot = GROUP_SLOT_PAF; group_slot < GROUP_SLOT_MAX; group_slot++) {
			group = groupmgr->group[ginstance][group_slot];
			if (!group)
				continue;

			group_id = group->id;
			if (((group_ids) & GROUP_ID(group_id)) &&
					(GET_DEVICE_TYPE_BY_GRP(group_id) == IS_DEVICE_ISCHAIN)) {
				ret = is_hardware_process_start(hardware, ginstance, group_id);
				if (ret) {
					merr("is_hardware_process_start is fail(%d)", device, ret);
					mutex_unlock(&hardware->itf_lock);
					return ret;
				}
			}
		}
	}

	mutex_unlock(&hardware->itf_lock);

	return ret;
}

int is_itf_process_off_wrap(struct is_device_ischain *device, u64 group_ids,
	u32 fstop)
{
	struct is_hardware *hardware;
	u32 instance = 0;
	u32 ginstance, group_slot, group_id;
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_path_info *path;

	hardware = device->hardware;
	instance = device->instance;
	groupmgr = device->groupmgr;
	path = &device->path;

	minfo_hw("itf_process_off_wrap: [G:0x%lx](%d)\n", instance, group_ids, fstop);

	mutex_lock(&hardware->itf_lock);

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		for (group_slot = GROUP_SLOT_PAF; group_slot < GROUP_SLOT_MAX; group_slot++) {
			group = groupmgr->group[ginstance][group_slot];
			if (!group)
				continue;

			group_id = group->id;
			if ((group_ids) & GROUP_ID(group_id)) {
				if (GET_DEVICE_TYPE_BY_GRP(group_id) == IS_DEVICE_ISCHAIN) {
					is_hardware_process_stop(hardware,
							ginstance, group_id, fstop);
				} else {
					/* in case of sensor group */
					if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
						is_sensor_group_force_stop(device->sensor, group_id);
				}
			}
		}
	}


	mutex_unlock(&hardware->itf_lock);

	return ret;
}

void is_itf_sudden_stop_wrap(struct is_device_ischain *device, u32 instance, struct is_group *group)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	int index;
	struct is_core *core;

	if (!device) {
		mwarn_hw("%s: device(null)\n", instance, __func__);
		return;
	}

	sensor = device->sensor;
	if (!sensor) {
		mwarn_hw("%s: sensor(null)\n", instance, __func__);
		return;
	}

	core = (struct is_core *)sensor->private_data;
	if (!core) {
		mwarn_hw("%s: core(null)\n", instance, __func__);
		return;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
		for (index = 0; index < IS_SENSOR_COUNT; index++) {
			sensor = &core->sensor[index];
			if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state))
				continue;

			minfo_hw("%s: sensor[%d] sudden close, call sensor_front_stop()\n",
				instance, __func__, index);

			ret = is_sensor_front_stop(sensor, true);
			if (ret)
				merr("is_sensor_front_stop is fail(%d)", sensor, ret);
		}
	}

	if (group) {
		if (test_bit(IS_GROUP_FORCE_STOP, &group->state)) {
			ret = is_itf_force_stop(device, GROUP_ID(group->id));
			if (ret)
				mgerr(" is_itf_force_stop is fail(%d)", device, group, ret);
		} else {
			ret = is_itf_process_stop(device, GROUP_ID(group->id));
			if (ret)
				mgerr(" is_itf_process_stop is fail(%d)", device, group, ret);
		}
	}

	return;
}

int __nocfi is_itf_power_down_wrap(struct is_interface *interface, u32 instance)
{
	int ret = 0;
	struct is_core *core;
	struct is_resourcemgr *rsc_mgr;
#ifdef USE_DDK_SHUT_DOWN_FUNC
	void *data = NULL;
#endif

	dbg_hw(2, "%s\n", __func__);

	core = (struct is_core *)interface->core;
	if (!core) {
		mwarn_hw("%s: core(null)\n", instance, __func__);
		return ret;
	}

	rsc_mgr = &core->resourcemgr;

	is_itf_sudden_stop_wrap(&core->ischain[instance], instance, NULL);

#ifdef USE_DDK_SHUT_DOWN_FUNC
	if (!IS_ENABLED(SKIP_LIB_LOAD) &&
			test_bit(IS_BINARY_LOADED, &rsc_mgr->binary_state)) {
		is_fpsimd_get_func();
		((ddk_shut_down_func_t)DDK_SHUT_DOWN_FUNC_ADDR)(data);
		is_fpsimd_put_func();
	}
#endif
	if (IS_ENABLED(DYNAMIC_HEAP_FOR_DDK_RTA))
		is_heap_mem_free(rsc_mgr);

	check_lib_memory_leak();

	return ret;
}

int is_itf_sys_ctl_wrap(struct is_device_ischain *device,
	int cmd, int val)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_sensor_mode_wrap(struct is_device_ischain *device,
	struct is_sensor_cfg *cfg)
{
#ifdef USE_RTA_BINARY
	void *data = NULL;

	dbg_hw(2, "%s\n", __func__);

	if (cfg && cfg->mode == SENSOR_MODE_DEINIT) {
		info_hw("%s: call RTA_SHUT_DOWN\n", __func__);
		is_fpsimd_get_func();
		((rta_shut_down_func_t)RTA_SHUT_DOWN_FUNC_ADDR)(data);
		is_fpsimd_put_func();
	}
#else
	dbg_hw(2, "%s\n", __func__);
#endif

	return 0;
}

void is_itf_fwboot_init(struct is_interface *this)
{
	clear_bit(IS_IF_LAUNCH_FIRST, &this->launch_state);
	clear_bit(IS_IF_LAUNCH_SUCCESS, &this->launch_state);
	clear_bit(IS_IF_RESUME, &this->fw_boot);
	clear_bit(IS_IF_SUSPEND, &this->fw_boot);
	this->fw_boot_mode = COLD_BOOT;
}

bool check_setfile_change(struct is_group *group_leader,
	struct is_group *group, struct is_hardware *hardware,
	u32 instance, u32 scenario)
{
	struct is_group *group_ischain = group;
	struct is_hw_ip *hw_ip = NULL;
	enum exynos_sensor_position sensor_position;

	if (group_leader->id != group->id)
		return false;

	if ((group->device_type == IS_DEVICE_SENSOR)
		&& (group->next)) {
		group_ischain = group->next;
	}

	hw_ip = is_get_hw_ip(group_ischain->id, hardware);
	if (!hw_ip) {
		mgerr("invalid hw_ip", group_ischain, group_ischain);
		return false;
	}

	sensor_position = hardware->sensor_position[instance];
	/* If the 3AA hardware is shared between front preview and reprocessing instance, (e.g. PIP)
	   apply_setfile funciton needs to be called for sensor control. There is two options to check
	   this, one is to check instance change and the other is to check scenario(setfile_index) change.
	   The scenario(setfile_index) is different front preview instance and reprocessing instance.
	   So, second option is more efficient way to support PIP scenario.
	 */
	if (scenario != hw_ip->applied_scenario) {
		msinfo_hw("[G:0x%lx,0x%lx,0x%lx]%s: scenario(%d->%d), instance(%d->%d)\n",
			instance, hw_ip,
			GROUP_ID(group_leader->id), GROUP_ID(group_ischain->id),
			GROUP_ID(group->id), __func__,
			hw_ip->applied_scenario, scenario,
			atomic_read(&hw_ip->instance), instance);
		return true;
	}

	return false;
}

int is_itf_shot_wrap(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *frame)
{
	struct is_hardware *hardware;
	struct is_interface *itf;
	struct is_group *group_leader;
	u32 instance, ginstance;
	int ret = 0;
	ulong flags;
	u32 scenario;
	bool apply_flag = false;

	scenario = device->setfile & IS_SETFILE_MASK;
	hardware = device->hardware;
	instance = device->instance;
	itf = device->interface;
	group_leader = get_ischain_leader_group(device);

	apply_flag = check_setfile_change(group_leader, group, hardware, instance, scenario);

	if (!IS_ENABLED(SKIP_SETFILE_LOAD) &&
			(!atomic_read(&group_leader->scount) || apply_flag)) {
		for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
			if (!test_bit(ginstance, &device->ginstance_map))
				continue;

			mdbg_hw(1, "[G:0x%lx]%s: call apply_setfile()\n",
					ginstance, GROUP_ID(group->id), __func__);

			ret = is_hardware_apply_setfile(hardware, ginstance,
					scenario, hardware->hw_map[ginstance]);
			if (ret) {
				merr("is_hardware_apply_setfile is fail(%d)",
						device, ret);
				return ret;
			}
		}
	}

	ret = is_hardware_grp_shot(hardware, instance, group, frame,
					hardware->hw_map[instance]);
	if (ret) {
		merr("is_hardware_grp_shot is fail(%d)", device, ret);
		return ret;
	}

	spin_lock_irqsave(&itf->shot_check_lock, flags);
	atomic_set(&itf->shot_check[instance], 1);
	spin_unlock_irqrestore(&itf->shot_check_lock, flags);

	return ret;
}
