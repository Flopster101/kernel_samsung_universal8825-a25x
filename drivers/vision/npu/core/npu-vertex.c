/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/bug.h>
#include <linux/dma-buf.h>

#include "npu-log.h"
#include "vision-ioctl.h"
#include "npu-vertex.h"
#include "vision-dev.h"
#include "vision-buffer.h"
#include "npu-device.h"
#include "npu-session.h"
#include "npu-common.h"
#include "npu-scheduler.h"
#include "npu-stm-soc.h"
#include "npu-system-soc.h"
#ifdef CONFIG_NPU_USE_HW_DEVICE
#include "npu-hw-device.h"
#endif
#include "npu-ver.h"
#include "vs4l.h"
#include "npu-util-regs.h"

#define DEFAULT_KPI_MODE	false	/* can be changed to true while testing */

bool is_kpi_mode_enabled(bool strict)
{
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	if (info->mode_ref_cnt[NPU_PERF_MODE_NPU_BOOST_BLOCKING] > 0)
		return true;
	else if (strict && info->mode_ref_cnt[NPU_PERF_MODE_NPU_BOOST] > 0)
		return true;
	else if (info->dd_direct_path == 0x01)
		return true;
	else
		return (DEFAULT_KPI_MODE);
}

const struct vision_file_ops npu_vertex_fops;
const struct vertex_ioctl_ops npu_vertex_ioctl_ops;
static int __force_streamoff(struct file *);

static int __vref_open(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	atomic_set(&vertex->start_cnt.refcount, 0);
	return npu_device_open(device);
}

static int __vref_close(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	return npu_device_close(device);
}

#ifdef CONFIG_NPU_USE_BOOT_IOCTL
static int __vref_bootup(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	return npu_device_bootup(device);
}

static int __vref_shutdown(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	return npu_device_shutdown(device);
}
#endif

static int __vref_start(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	return npu_device_start(device);
}

static int __vref_stop(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	return npu_device_stop(device);
}

static inline void __vref_init(struct npu_vertex_refcount *vref,
	struct npu_vertex *vertex, int (*first)(struct npu_vertex *vertex), int (*final)(struct npu_vertex *vertex))
{
	vref->vertex = vertex;
	vref->first = first;
	vref->final = final;
	atomic_set(&vref->refcount, 0);
}

static inline int __vref_get(struct npu_vertex_refcount *vref)
{
	return (atomic_inc_return(&vref->refcount) == 1) ? vref->first(vref->vertex) : 0;
}

static inline int __vref_put(struct npu_vertex_refcount *vref)
{
	return (atomic_dec_return(&vref->refcount) == 0) ? vref->final(vref->vertex) : 0;
}

/* Convinient helper to check emergency state */
static inline int check_emergency(struct npu_device *dev)
{
	if (unlikely(npu_device_is_emergency_err((dev)))) {
		npu_warn("EMERGENCY ERROR STATE!\n");
		return NPU_CRITICAL_DRIVER(NPU_ERR_IN_EMERGENCY);
	}
	return 0;
}
static inline int check_emergency_vctx(struct npu_vertex_ctx *vctx)
{
	struct npu_vertex *__vertex = vctx->vertex;
	struct npu_device *__device = container_of(__vertex, struct npu_device, vertex);

	return check_emergency(__device);
}

static int npu_vertex_s_graph(struct file *file, struct vs4l_graph *sinfo)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	profile_point1(PROBE_ID_DD_NW_VS4L_ENTER, 0, 0, NPU_NW_CMD_LOAD);
	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_OPEN))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_s_graph(session, sinfo);
	if (ret) {
		npu_err("fail(%d) in npu_session_config\n", ret);
		goto p_err;
	}

	vctx->state |= BIT(NPU_VERTEX_GRAPH);

p_err:
	mutex_unlock(lock);
	profile_point1(PROBE_ID_DD_NW_VS4L_RET, 0, 0, NPU_NW_CMD_LOAD);
	return ret;
}

static int npu_vertex_open(struct file *file)
{
	int ret = 0;
	struct npu_vertex *vertex = dev_get_drvdata(&vision_devdata(file)->dev);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct mutex *lock = &vertex->lock;
	struct npu_vertex_ctx *vctx = NULL;
	struct npu_session *session = NULL;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	/* check npu_device emergency error */
	ret = check_emergency(device);
	if (ret) {
		npu_warn("Open in emergency recovery - returns -ELIBACC\n");
		return -ELIBACC;
	}

	if (mutex_lock_interruptible(lock)) {
		npu_err("fail in mutex_lock_interruptible\n");
		return -ERESTARTSYS;
	}
	ret = __vref_get(&vertex->open_cnt);
	if (check_emergency(device)) {
		npu_err("start for emergency recovery\n");
		__vref_put(&vertex->open_cnt);
		npu_device_recovery_close(device);
		ret = -EWOULDBLOCK;
		goto ErrorExit;
	}
	if (ret) {
		npu_err("fail(%d) in vref_get", ret);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	ret = npu_session_open(&session, &device->sessionmgr, &device->system.memory);
	if (ret) {
		npu_err("fail(%d) in npu_graph_create", ret);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	/* set max npu core for the SOC */
	session->sched_param.max_npu_core = device->system.max_npu_core;

	vctx			= &session->vctx;
	vctx->id		= session->uid;
	vctx->vertex		= vertex;
	mutex_init(&vctx->lock);
	ret = npu_queue_open(&vctx->queue, &device->system.memory, &vctx->lock);
	if (ret) {
		npu_err("fail(%d) in npu_queue_open", ret);
		npu_session_undo_open(session);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	file->private_data = vctx;
	vctx->state |= BIT(NPU_VERTEX_OPEN);

ErrorExit:
	mutex_unlock(lock);
	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	if (ret < -1000) {
		/* Return value is not acceptable as open's result */
		npu_warn("Error [%d/%x] - convert return value to -ELIBACC\n", ret, ret);
		return -ELIBACC;
	}
	if (vctx)
		npu_iinfo("%s: (%d), open_ref(%d)\n", vctx, __func__, ret,
							atomic_read(&vertex->open_cnt.refcount));
	else
		npu_info("%s: (%d)\n",  __func__, ret);
	return ret;
}

enum npu_vertex_state check_done_state(u32 state)
{
	u32 ret = 0;

	if (state & BIT(NPU_VERTEX_OPEN))
		ret = NPU_VERTEX_OPEN;
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	if (state & BIT(NPU_VERTEX_POWER))
		ret = NPU_VERTEX_POWER;
#endif
	if (state & BIT(NPU_VERTEX_GRAPH))
		ret = NPU_VERTEX_GRAPH;
	if (state & BIT(NPU_VERTEX_FORMAT))
		ret = NPU_VERTEX_FORMAT;
	if (state & BIT(NPU_VERTEX_STREAMON))
		ret = NPU_VERTEX_STREAMON;
	if (state & BIT(NPU_VERTEX_STREAMOFF))
		ret = NPU_VERTEX_STREAMOFF;
	return ret;
}

static int npu_vertex_close(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;
	int id = vctx->id;
	enum npu_vertex_state done_state;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	int hids = session->hids;
#endif
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	u32 save_state = vctx->state;
#endif

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("mutex_lock_interruptible is fail\n", vctx);
		return -ERESTARTSYS;
	}
	done_state = check_done_state(vctx->state);
	switch (done_state) {
	case NPU_VERTEX_STREAMOFF:
		goto normal_complete;
	case NPU_VERTEX_STREAMON:
		goto force_streamoff;
	case NPU_VERTEX_FORMAT:
	case NPU_VERTEX_GRAPH:
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	case NPU_VERTEX_POWER:
#endif
	case NPU_VERTEX_OPEN:
		goto session_free;
	default:
		ret = -EINVAL;
		npu_err("fail(%d) in done_state\n", ret);
		goto p_err;
	}
force_streamoff:
	ret = __force_streamoff(file);
	if (ret) {
		npu_err("fail(%d) in __force_streamoff", ret);
		goto p_err;
	}
normal_complete:
	ret = npu_session_NW_CMD_UNLOAD(session);
	if (ret) {
		npu_err("fail(%d) in npu_session_NW_CMD_UNLOAD", ret);
		goto p_err;
	}
	vctx->state |= BIT(NPU_VERTEX_CLOSE);
	npu_iinfo("npu_vertex_close(): (%d)\n", vctx, ret);
session_free:
#ifdef CONFIG_NPU_SECURE_MODE
	if (vertex->normal_count) {
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	if (vctx->state & BIT(NPU_VERTEX_POWER)) {
		npu_sessionmgr_unregHW(session);
		ret = npu_session_NW_CMD_POWER_NOTIFY(session, false);
		if (ret) {
			npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, ret);
			goto p_err;
		}
	}
#endif
	ret = npu_session_close(session);
	if (ret) {
		npu_err("fail(%d) in npu_session_close", ret);
		goto p_err;
	}

#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	if (vertex->normal_count) {
		ret = __vref_put(&vertex->boot_cnt);
		if (ret) {
			npu_err("fail(%d) in vref_put\n", ret);
			goto p_err;
		}
	}
	if (vertex->normal_count == 1 || vertex->secure_count == 1) {
		if (session->sec_mem_buf) {
			npu_memory_free(session->memory, session->sec_mem_buf);
			kfree(session->sec_mem_buf);
			session->sec_mem_buf = NULL;
		}
		if (!check_emergency(device)) {
			ret = npu_hwdev_shutdown(device, hids);
			if (ret) {
				npu_ierr("fail(%d) in npu_vertex_shutdown\n", vctx, ret);
				goto p_err;
			}
		}
	}
#endif

	if (vertex->normal_count) {
		ret = __vref_put(&vertex->open_cnt);
		if (ret) {
			npu_err("fail(%d) in vref_put", ret);
			goto p_err;
		}
	}

	if (device->is_secure && vertex->secure_count) {
		vertex->secure_count--;
	} else if (vertex->normal_count) {
		vertex->normal_count--;
	}
	device->is_secure = 0;
p_err:
	npu_info("id(%d)\n", id);
	mutex_unlock(lock);
	return ret;
#else
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	if (vctx->state & BIT(NPU_VERTEX_POWER)) {
		npu_sessionmgr_unregHW(session);
		ret = npu_session_NW_CMD_POWER_NOTIFY(session, false);
		if (ret) {
			npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, ret);
			goto p_err;
		}
	}
#endif
	ret = npu_session_close(session);
	if (ret) {
		npu_err("fail(%d) in npu_session_close", ret);
		goto p_err;
	}
#ifdef CONFIG_NPU_USE_BOOT_IOCTL

	if (save_state & BIT(NPU_VERTEX_POWER)) {
		ret = __vref_put(&vertex->boot_cnt);
		if (ret) {
			npu_err("fail(%d) in vref_put\n", ret);
			goto p_err;
		}
	}
	if (!check_emergency(device)) {
		ret = npu_hwdev_shutdown(device, hids);
		if (ret) {
			npu_ierr("fail(%d) in npu_vertex_shutdown\n", vctx,
				 ret);
			goto p_err;
		}
	}

#endif
	ret = __vref_put(&vertex->open_cnt);
	if (ret) {
		npu_err("fail(%d) in vref_put", ret);
		goto p_err;
	}
p_err:
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	npu_info("id(%d), boot_ref(%d), open_ref(%d)\n", id,
		atomic_read(&vertex->boot_cnt.refcount), atomic_read(&vertex->open_cnt.refcount));
#else
	npu_info("id(%d), open_ref(%d)\n", id, atomic_read(&vertex->open_cnt.refcount));
#endif
	mutex_unlock(lock);
	return ret;
#endif
}

static unsigned int npu_vertex_poll(struct file *file, poll_table *poll)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_queue *queue = &vctx->queue;

	if (!(vctx->state & BIT(NPU_VERTEX_STREAMON))) {
		ret |= POLLERR;
		goto p_err;
	}

	ret = npu_queue_poll(queue, file, poll);
p_err:
	return ret;
}

static int npu_vertex_flush(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(
			vctx, struct npu_session, vctx);
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;

	if (fatal_signal_pending(current) || (current->exit_code != 0)) {
		mutex_lock(lock);
		npu_info("Flush caused by forced terminated.\n");
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
		if (!(vctx->state & BIT(NPU_VERTEX_STREAMOFF)) &&
				(vctx->state & BIT(NPU_VERTEX_POWER))) {
#else
		if (!(vctx->state & BIT(NPU_VERTEX_STREAMOFF))) {
#endif
			ret = __force_streamoff(file);
			if (ret)
				npu_err("fail(%d) in flush, __force_streamoff\n", ret);
		}

		ret = npu_session_flush(session);
		if (ret)
			npu_err("fail(%d) in npu_session_flush\n", ret);
		mutex_unlock(lock);
	}

	npu_idbg("%s: (%d)\n", vctx, __func__, ret);
	return ret;
}


const struct vision_file_ops npu_vertex_fops = {
	.owner          = THIS_MODULE,
	.open           = npu_vertex_open,
	.release        = npu_vertex_close,
	.poll           = npu_vertex_poll,
	.ioctl          = vertex_ioctl,
#if IS_ENABLED(CONFIG_NPU_COMPAT_IOCTL)
	.compat_ioctl   = vertex_compat_ioctl32,
#endif
	.flush		= npu_vertex_flush
};

int npu_vertex_probe(struct npu_vertex *vertex, struct device *parent)
{
	int ret = 0;
	struct vision_device *vdev;
#ifdef CONFIG_NPU_SECURE_MODE
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
#endif

	BUG_ON(!vertex);
	BUG_ON(!parent);

	get_device(parent);
	mutex_init(&vertex->lock);
	__vref_init(&vertex->open_cnt, vertex, __vref_open, __vref_close);
	__vref_init(&vertex->start_cnt, vertex, __vref_start, __vref_stop);
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	__vref_init(&vertex->boot_cnt, vertex, __vref_bootup, __vref_shutdown);
#endif
#ifdef CONFIG_NPU_SECURE_MODE
	vertex->normal_count = 0;
	vertex->secure_count = 0;
	device->is_secure = 0;
#endif

	vdev = &vertex->vd;
	snprintf(vdev->name, sizeof(vdev->name), "%s", npu_vertex_name);
	vdev->fops		= &npu_vertex_fops;
	vdev->ioctl_ops		= &npu_vertex_ioctl_ops;
	vdev->release		= NULL;
	vdev->lock		= NULL;
	vdev->parent		= parent;
	vdev->type		= VISION_DEVICE_TYPE_VERTEX;
	dev_set_drvdata(&vdev->dev, vertex);

	ret = vision_register_device(vdev, configs[NPU_MINOR], npu_vertex_fops.owner);
	if (ret) {
		probe_err("fail(%d) in vision_register_device\n", ret);
		goto p_err;
	}

	probe_info("%s: (%d)\n", __func__, ret);

p_err:
	return ret;
}

static int npu_vertex_s_format(struct file *file, struct vs4l_format_list *flist)
{
	int ret = 0;
	u32 i = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
#ifdef CONFIG_NPU_SECURE_MODE
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
#endif
	struct mutex *lock = &vctx->lock;
	u32 FM_cnt;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	npu_scheduler_boost_on(info);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

#ifdef CONFIG_NPU_SECURE_MODE
	if (device->is_secure) {
		npu_ierr("Aleady in secure mode; will not honour a blocking call\n", vctx);
		return -EINVAL;
	}
#endif

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & (BIT(NPU_VERTEX_GRAPH) | BIT(NPU_VERTEX_FORMAT)))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (flist->direction == VS4L_DIRECTION_IN) {
		FM_cnt = session->IFM_cnt;
	} else {
		FM_cnt = session->OFM_cnt;
	}

	npu_udbg("flist %d, FM_cnt %d\n", session, flist->count, FM_cnt);
#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
#endif
	{
		if (FM_cnt > flist->count || FM_cnt + FM_SHARED_NUM < flist->count) {
			npu_uinfo("FM_cnt(%d) is not same as flist_cnt(%d)\n", session, FM_cnt, flist->count);
			ret = -EINVAL;
			goto p_err;
		}
	}

	for (i = 0; i < flist->count; i++) {
		npu_uinfo(
			"s_format, dir(%u), cnt(%u), target(%u), format(%u), plane(%u)\n",
			session, flist->direction, i, flist->formats[i].target,
			flist->formats[i].format, flist->formats[i].plane);
		npu_uinfo("width(%u), height(%u), stride(%u)\n", session, flist->formats[i].width, flist->formats[i].height, flist->formats[i].stride);
		npu_uinfo("cstride(%u), channels(%u), pixel_format(%u)\n",
			session, flist->formats[i].cstride, flist->formats[i].channels, flist->formats[i].pixel_format);
	}

	ret = npu_queue_s_format(queue, flist);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_s_format\n", vctx, ret);
		goto p_err;
	}

	npu_uinfo("direction %d\n", session, flist->direction);
	if (flist->direction == VS4L_DIRECTION_OT) {
		ret = npu_session_NW_CMD_LOAD(session);
		ret = chk_nw_result_no_error(session);
		if (ret == NPU_ERR_NO_ERROR) {
			vctx->state |= BIT(NPU_VERTEX_FORMAT);
		} else {
			goto p_err;
		}
	}

	npu_iinfo("%s():%d\n", vctx, __func__, ret);
	mutex_unlock(lock);
	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	return ret;
p_err:
	vctx->state &= (~BIT(NPU_VERTEX_GRAPH));
	mutex_unlock(lock);
	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	return ret;
}

static int npu_vertex_s_param(struct file *file, struct vs4l_param_list *plist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_OPEN))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_param(session, plist);
	if (ret) {
		npu_err("fail(%d) in npu_session_param\n", ret);
		goto p_err;
	}

p_err:
	npu_idbg("%s: (%d)\n", vctx, __func__, ret);
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_sched_param(struct file *file, struct vs4l_sched_param *param)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_OPEN))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_nw_sched_param(session, param);
	if (ret) {
		npu_err("fail(%d) in npu_vertex_sched_param\n", ret);
		goto p_err;
	}

p_err:
	npu_iinfo("%s: (%d)\n", vctx, __func__, ret);
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_s_ctrl(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;

	return ret;
}

static int npu_vertex_qbuf(struct file *file, struct vs4l_container_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;

	BUG_ON(!vctx);
	BUG_ON(!clist);
	BUG_ON(!queue);
	BUG_ON(!lock);
	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (clist)
		profile_point1(PROBE_ID_DD_FRAME_VS4L_QBUF_ENTER, 0, clist->id, clist->direction);

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_STREAMON))) {
		npu_ierr("(%d) invalid state(%X)\n", vctx, clist->direction, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_qbuf(queue, clist);
	if (ret) {
		npu_ierr("(%d) npu_queue_qbuf is fail(%d)\n", vctx, clist->direction, ret);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_dqbuf(struct file *file, struct vs4l_container_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
	bool nonblocking = file->f_flags & O_NONBLOCK;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}
	if (!(vctx->state & BIT(NPU_VERTEX_STREAMON))) {
		npu_ierr("(%d) invalid state(%X)\n", vctx, clist->direction, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}
	ret = npu_queue_dqbuf(queue, clist, nonblocking);
	if (ret) {
		if (ret != -EWOULDBLOCK)
			npu_ierr("fail(%d) in (%d) npu_queue_dqbuf\n", vctx, ret, clist->direction);
		goto p_err;
	}
p_err:
	mutex_unlock(lock);
	if (clist)
		profile_point1(PROBE_ID_DD_FRAME_VS4L_DQBUF_RET, 0, clist->id, clist->direction);

	return ret;
}

static int npu_vertex_prepare(struct file *file, struct vs4l_container_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;

	BUG_ON(!vctx);
	BUG_ON(!clist);
	BUG_ON(!queue);
	BUG_ON(!lock);
	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("(%d) invalid state(%X)\n", vctx, clist->direction, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}
	ret = npu_queue_prepare(queue, clist);
	if (ret) {
		npu_ierr("(%d) vpu_queue_qbuf is fail(%d)\n", vctx, clist->direction, ret);
		goto p_err;
	}
p_err:
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_unprepare(struct file *file, struct vs4l_container_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;

	BUG_ON(!vctx);
	BUG_ON(!clist);
	BUG_ON(!queue);
	BUG_ON(!lock);
	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);

	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("(%d) invalid state(%X)\n", vctx, clist->direction, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_unprepare(queue, clist);
	if (ret) {
		npu_ierr("(%d) npu_queue_unprepare is failed(%d)\n", vctx, clist->direction, ret);
		goto p_err;
	}
p_err:
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_streamon(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
#ifdef CONFIG_NPU_SECURE_MODE
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
#endif
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

#ifdef CONFIG_NPU_SECURE_MODE
	if (device->is_secure) {
		npu_ierr("Aleady in secure mode; will not honour a blocking call\n", vctx);
		return -EINVAL;
	}
#endif

	profile_point1(PROBE_ID_DD_NW_VS4L_ENTER, 0, 0, NPU_NW_CMD_STREAMON);
	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (vctx->state & BIT(NPU_VERTEX_STREAMON)) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))
	    || !(vctx->state & BIT(NPU_VERTEX_GRAPH))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = __vref_get(&vertex->start_cnt);
	if (ret) {
		npu_err("fail(%d) in vref_get\n", ret);
		goto p_err;
	}

	ret = npu_queue_start(queue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_start\n", vctx, ret);
		goto p_err;
	}

	ret = npu_session_NW_CMD_STREAMON(session);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_STREAMON\n", vctx, ret);
		goto p_err;
	}
	ret = chk_nw_result_no_error(session);
	if (ret == NPU_ERR_NO_ERROR) {
		vctx->state |= BIT(NPU_VERTEX_STREAMON);
	} else {
		goto p_err;
	}
p_err:
	npu_iinfo("%s: (%d), start_ref(%d)\n", vctx, __func__, ret,
						atomic_read(&vertex->start_cnt.refcount));
	mutex_unlock(lock);
	profile_point1(PROBE_ID_DD_NW_VS4L_RET, 0, 0, NPU_NW_CMD_STREAMON);
	return ret;
}

static int npu_vertex_streamoff(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
#ifdef CONFIG_NPU_SECURE_MODE
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
#endif
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

#ifdef CONFIG_NPU_SECURE_MODE
	if (device->is_secure) {
		npu_ierr("Already in secure mode; will not honour a blocking call\n", vctx);
		return -EINVAL;
	}
#endif

	profile_point1(PROBE_ID_DD_NW_VS4L_ENTER, 0, 0, NPU_NW_CMD_STREAMOFF);
	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_STREAMON))) {
		npu_ierr("invalid state(0x%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))
	    || !(vctx->state & BIT(NPU_VERTEX_GRAPH))) {
		npu_ierr("invalid state(%X)\n", vctx, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_streamoff(queue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_streamoff\n", vctx, ret);
		goto p_err;
	}

	ret = npu_session_NW_CMD_STREAMOFF(session);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_STREAMOFF\n", vctx, ret);
		goto p_err;
	}
	ret = chk_nw_result_no_error(session);
	if (ret == NPU_ERR_NO_ERROR) {
		vctx->state |= BIT(NPU_VERTEX_STREAMOFF);
		vctx->state &= (~BIT(NPU_VERTEX_STREAMON));
	} else {
		goto p_err;
	}

	ret = npu_queue_stop(queue, 0);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_stop\n", vctx, ret);
		goto p_err;
	}

	ret = __vref_put(&vertex->start_cnt);
	if (ret) {
		npu_err("fail(%d) in vref_put\n", ret);
		goto p_err;
	}
p_err:
	npu_iinfo("%s: (%d), start_ref(%d)\n", vctx, __func__, ret,
						atomic_read(&vertex->start_cnt.refcount));
	mutex_unlock(lock);
	profile_point1(PROBE_ID_DD_NW_VS4L_RET, 0, 0, NPU_NW_CMD_STREAMOFF);
	return ret;
}

#ifdef CONFIG_NPU_USE_BOOT_IOCTL
#ifdef CONFIG_NPU_SECURE_MODE
int npu_hwdev_secure_bootup(struct npu_device *device, struct npu_vertex_ctx *vctx, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(&vertex->lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (vertex->normal_count) {
		int retry = 1000;
		bool done = false;

		mutex_unlock(&vertex->lock);
		while (retry) {
			mutex_lock(&vertex->lock);
			if (!vertex->normal_count) {
				done = true;
				break;
			}

			mutex_unlock(&vertex->lock);
			retry--;
			mdelay(5);
		}

		if (!done) {
			ret = -ETIMEDOUT;
			npu_err("normal operation(%u) was not finished\n",
					vertex->normal_count);
			goto p_err_check;
		}
	}

	if (vertex->secure_count) {
		vertex->secure_count++;
		npu_info("secure normal count is increased(%u)\n", vertex->normal_count);
		mutex_unlock(&vertex->lock);
		return 0;
	}

	device->is_secure = 1;
	ret = npu_hwdev_bootup(device, ctrl->value);
	if (ret) {
		npu_ierr("fail(%d) in npu_vertex_bootup : hids %x\n", vctx, ret, ctrl->value);
		goto p_err;
	}
	npu_info(" secure_count is %u\n", vertex->secure_count);
	npu_info(" normal_count is %u\n", vertex->normal_count);
	npu_info(" mem_size     is %u\n", ctrl->mem_size);

	if (ctrl->mem_size) {
		struct npu_memory_buffer *buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
		buf->size = ctrl->mem_size;
		if (unlikely(!buf)) {
			npu_err("failed to alloc buf.\n");
			ret = -ENOMEM;
			return ret;
		}
		session->sec_mem_buf = buf;
		ret = npu_memory_alloc_secure(session->memory, buf, configs[NPU_PBHA_HINT_00]);
		if (unlikely(ret)) {
			npu_err("npu_memory_alloc_secure is failed(%d).\n", ret);
			goto p_err_mem;
		}
		ctrl->mem_addr = (unsigned int)buf->daddr;
		ctrl->mem_addr_h = (unsigned int)(buf->daddr >> 32);
		npu_info("ctrl->mem_addr : %u\n", (unsigned int)buf->daddr);
		npu_info("ctrl->mem_addr_h : %u\n", (unsigned int)(buf->daddr >> 32));
	}

	vertex->secure_count = 1;
	mutex_unlock(&vertex->lock);

	return 0;
p_err_mem:
	kfree(session->sec_mem_buf);
	npu_hwdev_shutdown(device, ctrl->value);
p_err_check:
p_err:
	device->is_secure = 0;
	mutex_unlock(&vertex->lock);
	return ret;
}

int npu_hwdev_secure_bootdown(struct npu_device *device, struct npu_vertex_ctx *vctx, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(&vertex->lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (vertex->secure_count == 0) {
		npu_ierr("secure_count is zero.\n", vctx);
		mutex_unlock(&vertex->lock);
		return -1;
	}

	if (vertex->secure_count > 1) {
		vertex->secure_count--;
		npu_info("secure normal count is decreased(%u)\n", vertex->normal_count);
		mutex_unlock(&vertex->lock);
		return 0;
	}

	if (session->sec_mem_buf) {
		npu_memory_free(session->memory, session->sec_mem_buf);
		kfree(session->sec_mem_buf);
		session->sec_mem_buf = NULL;
	}

	npu_hwdev_shutdown(device, ctrl->value);

	device->is_secure = 0;
	vertex->secure_count--;
	mutex_unlock(&vertex->lock);

	return 0;
}

int npu_hwdev_normal_bootup(struct npu_device *device, struct npu_vertex_ctx *vctx, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(&vertex->lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (vertex->secure_count) {
		int retry = 1000;
		bool done = false;

		mutex_unlock(&vertex->lock);
		while (retry) {
			mutex_lock(&vertex->lock);
			if (!vertex->secure_count) {
				done = true;
				break;
			}

			mutex_unlock(&vertex->lock);
			retry--;
			mdelay(5);
		}

		if (!done) {
			ret = -ETIMEDOUT;
			npu_err("normal operation(%u) was not finished\n",
					vertex->secure_count);
			goto p_err_check;
		}
	}

	if (vertex->normal_count) {
		vertex->normal_count++;
		npu_info("secure normal count is increased(%u)\n", vertex->secure_count);
		mutex_unlock(&vertex->lock);
		return 0;
	}

	device->system.saved_warm_boot_flag = device->system.mbox_hdr->warm_boot_enable;

	device->is_secure = 0;
	ret = npu_hwdev_bootup(device, ctrl->value);
	if (ret) {
		npu_ierr("fail(%d) in npu_vertex_bootup : hids %x\n", vctx, ret, ctrl->value);
		goto p_err;
	}

	ret = __vref_get(&vertex->boot_cnt);
	if (ret) {
		npu_err("fail(%d) in vref_get", ret);
		__vref_put(&vertex->boot_cnt);
		goto p_err;
	}

	npu_sessionmgr_regHW(session);
	if (device->system.mbox_hdr->warm_boot_enable)
	{
		npu_session_restore_cnt(session);
		npu_session_NW_CMD_RESUME(session);
		npu_session_restart();
	} else {
		ret = npu_session_NW_CMD_POWER_NOTIFY(session, true);
		if (ret) {
			npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, ret);
			goto p_err;
		}

		{
			struct npu_hw_device *hdev =
				npu_get_hdev_by_id(session->hids);
			if (atomic_read(&hdev->boot_cnt.refcount) == 1) {
				ret = npu_stm_enable(&device->system, session->hids);
				if (ret)
					npu_err("fail(%d) in npu_stm_enable(%u)\n", ret, session->hids);
				/* enable HWACG */
				npu_hwdev_hwacg(&hdev->device->system, hdev->id, true);
			}
		}
	}
	vertex->normal_count = 1;
	mutex_unlock(&vertex->lock);
	return 0;
p_err_check:
p_err:
	return ret;
}
int npu_hwdev_normal_bootdown(struct npu_device *device, struct npu_vertex_ctx *vctx, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_vertex_refcount *vref;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(&vertex->lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	if (vertex->normal_count == 0) {
		npu_ierr("normal count is zero.\n", vctx);
		mutex_unlock(&vertex->lock);
		return -1;
	}

	if (vertex->normal_count > 1) {
		vertex->normal_count--;
		npu_info("normal normal count is decreased(%u)\n", vertex->normal_count);
		mutex_unlock(&vertex->lock);
		return 0;
	}

	ret = npu_session_NW_CMD_SUSPEND(session);
	if (ret) {
		npu_err("fail(%d) in npu_session_NW_CMD_SUSPEND\n", vctx, ret);
		goto p_err;
	}
	npu_system_soc_suspend(&device->system);
	npu_sessionmgr_unregHW(session);
	vref = &vertex->boot_cnt;
	device->active_non_secure_sessions = 1;
	device->active_non_secure_sessions = atomic_xchg(&vref->refcount, device->active_non_secure_sessions);
	__vref_put(&vertex->boot_cnt);
	device->active_non_secure_sessions--;
	device->is_secure = 0;

	npu_hwdev_shutdown(device, ctrl->value);

	vertex->normal_count--;
	mutex_unlock(&vertex->lock);

	return 0;
p_err:
	npu_iinfo("%s (%d)\n", vctx, __func__, ret);
	mutex_unlock(&vertex->lock);
	return ret;
}

static int npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	//struct npu_queue *queue = &vctx->queue;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	session->hids = ctrl->value;

	if ((ctrl->ctrl & MASK_BIT_UP_DOWN) == BOOT_UP) {
		if ((ctrl->ctrl & MASK_BIT_SECURE) == SECURE) {
			ret = npu_hwdev_secure_bootup(device, vctx, ctrl);
			if (ret) {
				npu_ierr("fail(%d) in npu_vertex_secure_bootup : hids %x\n", vctx, ret, ctrl->value);
				goto p_err;
			}
			npu_info("success(%d) in npu_vertex_secure_bootup : hids %x\n", vctx, ret, ctrl->value);

		} else if ((ctrl->ctrl & MASK_BIT_SECURE) == NON_SECURE) {
			ret = npu_hwdev_normal_bootup(device, vctx, ctrl);
			if (ret) {
				npu_ierr("fail(%d) in npu_vertex_normal_bootup : hids %x\n", vctx, ret, ctrl->value);
				goto p_err;
			}
			npu_info("success(%d) in npu_vertex_normal_bootup : hids %x\n", vctx, ret, ctrl->value);
		} else {
			npu_ierr("ctrl is wrong(%u)\n", vctx, ctrl->ctrl);
			ret = -1;
		}
	} else if ((ctrl->ctrl & MASK_BIT_UP_DOWN) == BOOT_DOWN) {
		if ((ctrl->ctrl & MASK_BIT_SECURE) == SECURE) {
			ret = npu_hwdev_secure_bootdown(device, vctx, ctrl);
			if (ret) {
				npu_ierr("fail(%d) in npu_vertex_secure_bootoff : hids %x\n", vctx, ret, ctrl->value);
				goto p_err;
			}
			npu_info("success(%d) in npu_vertex_secure_bootoff : hids %x\n", vctx, ret, ctrl->value);
		} else if ((ctrl->ctrl & MASK_BIT_SECURE) == NON_SECURE) {
			ret = npu_hwdev_normal_bootdown(device, vctx, ctrl);
			if (ret) {
				npu_ierr("fail(%d) in npu_vertex_normal_bootoff : hids %x\n", vctx, ret, ctrl->value);
				goto p_err;
			}
			npu_info("success(%d) in npu_vertex_normal_bootoff : hids %x\n", vctx, ret, ctrl->value);
		} else {
			npu_ierr("ctrl is wrong(%u)\n", vctx, ctrl->ctrl);
			ret = -1;
		}
	} else {
		npu_ierr("ctrl is wrong(%u)\n", vctx, ctrl->ctrl);
		ret = -1;
	}
	vctx->state |= BIT(NPU_VERTEX_POWER);
#ifdef CONFIG_NPU_USE_LLC
	npu_scheduler_send_wait_info_to_hw(session,	info);
#endif
p_err:
	npu_iinfo("%s: (%d)\n", vctx, __func__, ret);
	return ret;
}
#else

static int __npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;
	//struct npu_queue *queue = &vctx->queue;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	session->hids = ctrl->value;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	ret = npu_hwdev_bootup(device, ctrl->value);
	if (ret) {
		npu_ierr("fail(%d) in %s : hids %x\n", vctx, ret, __func__, ctrl->value);
		goto p_err;
	}

	ret = __vref_get(&vertex->boot_cnt);
	if (check_emergency(device)) {
		npu_err("start for emergency recovery\n");
		__vref_put(&vertex->boot_cnt);
		npu_device_recovery_close(device);
		ret = -EWOULDBLOCK;
		goto p_err;
	}

	if (ret) {
		npu_err("fail(%d) in vref_get", ret);
		__vref_put(&vertex->boot_cnt);
		goto p_err;
	}

	npu_sessionmgr_regHW(session);
	ret = npu_session_NW_CMD_POWER_NOTIFY(session, true);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, ret);
		goto p_err;
	}

	{
		struct npu_hw_device *hdev =
				npu_get_hdev_by_id(session->hids);
		if (atomic_read(&hdev->boot_cnt.refcount) == 1) {
			ret = npu_stm_enable(&device->system, session->hids);
			if (ret)
				npu_err("fail(%d) in npu_stm_enable(%u)\n", ret, session->hids);
			/* enable HWACG */
			npu_hwdev_hwacg(&hdev->device->system, hdev->id, true);
		}
	}

	vctx->state |= BIT(NPU_VERTEX_POWER);
#ifdef CONFIG_NPU_USE_LLC
	npu_scheduler_send_wait_info_to_hw(session,	info);
#endif
p_err:
	npu_iinfo("%s: (%d), boot_ref(%d)\n", vctx, __func__, ret,
						atomic_read(&vertex->boot_cnt.refcount));
	mutex_unlock(lock);
	return ret;
}

static int npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl)
{
	if (!file->private_data) {
		return -EINVAL;
	}
	return __npu_vertex_bootup(file, ctrl);

}
#endif
#endif

static int npu_vertex_profileon(struct file *file, struct vs4l_profiler *phead)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	//struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}
	// allocate vision buffer
	mutex_unlock(lock);
	return 0;
}

static int npu_vertex_profileoff(struct file *file, struct vs4l_profiler *phead)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	//struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}
	// deallocate vision buffer & update
	mutex_unlock(lock);
	return 0;
}

static int npu_vertex_version(struct file *file, struct vs4l_version *version)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;
	//struct npu_queue *queue = &vctx->queue;
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx);
		return -ERESTARTSYS;
	}

	npu_ver_info(device, version);

	if ((vctx->state & BIT(NPU_VERTEX_GRAPH)))
		npu_ver_fw_info(device, version);

	mutex_unlock(lock);
	return 0;
}

#ifdef CONFIG_NPU_USE_FENCE_SYNC
static int npu_vertex_sync(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_frame *frame = session->current_frame;

	if (!frame) {
		npu_ierr("error in sync: NULL frame, oid %d\n", vctx, session->uid);
		return -EINVAL;
	}

	/* sync buffers */
	vb_queue_sync(DMA_TO_DEVICE, frame->input);

	ret = npu_cmd_map(&device->system, "sync");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for sync\n", ret);
	}
	/* store start time in timestamp[4], and update it once the frame is done */
	frame->pwm_start_time = npu_cmd_map(&device->system, "fwpwm");
	frame->is_fence = true;
	npu_dbg("sync frame_id #%d, start time : %x\n", ctrl->value, frame->pwm_start_time);

	return ret;
}
#endif

static int __force_streamoff(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_queue *queue = &vctx->queue;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);

	profile_point1(PROBE_ID_DD_NW_VS4L_ENTER, 0, 0, NPU_NW_CMD_STREAMOFF);

	ret = npu_queue_streamoff(queue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_streamoff)\n", vctx, ret);
		goto p_err;
	}

	ret = npu_session_NW_CMD_STREAMOFF(session);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_STREAMOFF\n", vctx, ret);
		goto p_err;
	}

	if (!npu_device_is_emergency_err(device)) {
		ret = chk_nw_result_no_error(session);
		if (ret == NPU_ERR_NO_ERROR) {
			vctx->state |= BIT(NPU_VERTEX_STREAMOFF);
			vctx->state &= (~BIT(NPU_VERTEX_STREAMON));
		} else
			npu_warn("%s() : NPU DEVICE IS EMERGENCY ERROR\n", __func__);
	} else {
		npu_info("EMERGENCY_RECOVERY - %ums delay insearted instead of STREAMOFF.\n",
			configs[STREAMOFF_DELAY_ON_EMERGENCY]);
		msleep(configs[STREAMOFF_DELAY_ON_EMERGENCY]);
	}

	ret = npu_queue_stop(queue, 1);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_stop(forced)\n", vctx, ret);
		goto p_err;
	}

	ret = __vref_put(&vertex->start_cnt);
	if (ret) {
		npu_err("fail(%d) in vref_put\n", ret);
		goto p_err;
	}
p_err:
	npu_iinfo("%s: (%d)\n", vctx, __func__, ret);
	profile_point1(PROBE_ID_DD_NW_VS4L_RET, 0, 0, NPU_NW_CMD_STREAMOFF);
	return ret;
}

const struct vertex_ioctl_ops npu_vertex_ioctl_ops = {
	.vertexioc_s_graph      = npu_vertex_s_graph,
	.vertexioc_s_format     = npu_vertex_s_format,
	.vertexioc_s_param      = npu_vertex_s_param,
	.vertexioc_s_ctrl       = npu_vertex_s_ctrl,
	.vertexioc_sched_param	= npu_vertex_sched_param,
	.vertexioc_qbuf         = npu_vertex_qbuf,
	.vertexioc_dqbuf        = npu_vertex_dqbuf,
	.vertexioc_prepare      = npu_vertex_prepare,
	.vertexioc_unprepare    = npu_vertex_unprepare,
	.vertexioc_streamon     = npu_vertex_streamon,
	.vertexioc_streamoff    = npu_vertex_streamoff,
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
	.vertexioc_bootup	= npu_vertex_bootup,
#endif
	.vertexioc_profileon    = npu_vertex_profileon,
	.vertexioc_profileoff   = npu_vertex_profileoff,
	.vertexioc_version	= npu_vertex_version,
#ifdef CONFIG_NPU_USE_FENCE_SYNC
	.vertexioc_sync		= npu_vertex_sync,
#endif
};
