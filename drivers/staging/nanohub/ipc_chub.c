// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IPC Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#include "ipc_chub.h"
#include "chub_exynos.h"
#include <linux/clocksource.h>

static struct ipc_info ipc;

#if defined(CHUB_IPC)
#define NAME_PREFIX "ipc"
#else /* defined(AP_IPC) */
#define NAME_PREFIX "nanohub-ipc"

#ifndef CSP_PRINTF_INFO
#include "chub.h"
#define CSP_PRINTF_INFO(fmt, ...) nanohub_info(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) nanohub_err(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_DEBUG(fmt, ...) nanohub_debug(fmt, ##__VA_ARGS__)
#endif
#endif

#define SENSORMAP_MAGIC	"SensorMap"
#define MAX_ACTIVE_SENSOR_NUM (10)

bool ipc_have_sensor_info(struct sensor_map *sensor_map)
{
	if (sensor_map)
		if (!strncmp
		    (SENSORMAP_MAGIC, sensor_map->magic,
		     sizeof(SENSORMAP_MAGIC)))
			return true;
	return false;
}

void *ipc_get_base(enum ipc_region area)
{
	return ipc.ipc_addr[area].base;
}
EXPORT_SYMBOL(ipc_get_base);

u32 ipc_get_size(enum ipc_region area)
{
	return ipc.ipc_addr[area].size;
}

static u32 ipc_get_offset(enum ipc_region area)
{
	return (u32) (ipc.ipc_addr[area].base - ipc.sram_base);
}

static u32 ipc_get_offset_addr(void *addr)
{
	return (u32) (addr - ipc.sram_base);
}

static void ipc_set_info(enum ipc_region area, void *base, int size, char *name)
{
	ipc.ipc_addr[area].base = base;
	ipc.ipc_addr[area].size = size;
	memset(ipc.ipc_addr[area].name, 0, IPC_NAME_MAX);
	strncpy(ipc.ipc_addr[area].name, name, IPC_NAME_MAX - 1);
}

void ipc_set_chub_boottime(bool alive_mct_enabled)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	if (!map) {
		CSP_PRINTF_INFO("Cannot get BL_MAP base\n");
		return;
	}

	if (alive_mct_enabled) {
		static u64 time_gap; // XXX: make sure timer starts after mct
		u64 ap, ktime_tick;
		u32 shift, mult, rate, mct_alive_addr, mct_alive_offset;
		struct device_node *np;
		void *mct_alive = NULL;

		if (!time_gap) {
			np = of_find_compatible_node(NULL, NULL, "samsung,exynos-nanohub");

			if (of_property_read_bool(np, "chub_mct_alive_base")) {
				of_property_read_u32(np, "chub_mct_alive_base", &mct_alive_addr);
			} else {
				CSP_PRINTF_INFO("cannot find property.(chub_mct_alive_base)\n");
				return;
			}

			if (of_property_read_bool(np, "chub_mct_frc_offset")) {
				of_property_read_u32(np, "chub_mct_frc_offset", &mct_alive_offset);
			} else {
				CSP_PRINTF_INFO("cannot find property.(chub_mct_frc_offset)\n");
				return;
			}

			mct_alive = ioremap(mct_alive_addr + mct_alive_offset, 0x10);

			ap = readl_relaxed(mct_alive + 0x4); // read MCT_ALIVE_UPPER.
			ap = (ap << 32) | readl_relaxed(mct_alive);

			np = of_find_node_by_name(NULL, "timer");
			of_property_read_u32(np, "clock-frequency", &rate);

			clocks_calc_mult_shift(&mult, &shift, rate, NSEC_PER_SEC, 3600);
			ktime_tick = (ktime_get_boottime() << shift) / mult;

			time_gap = ap - ktime_tick;
			iounmap(mct_alive);
		}
		map->boottime = time_gap;

		CSP_PRINTF_INFO("Calculated time gap(tick) : %llu\n", map->boottime);

	} else {
		map->boottime = ktime_get_boottime();
		CSP_PRINTF_INFO("send kernel time : %llu\n", map->boottime);
	}
}

/* API for bootmode and runtimelog */
void ipc_set_chub_bootmode(u16 bootmode, u16 rtlog)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->bootmode = bootmode;
	map->runtimelog = rtlog;
}

u16 ipc_get_chub_bootmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->bootmode;
}

u16 ipc_get_chub_rtlogmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->runtimelog;
}

/* API for ap sleep */
void ipc_set_ap_wake(u16 wake)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	map->wake = wake;
}

u16 ipc_get_ap_wake(void)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	return map->wake;
}

struct sampleTimeTable *ipc_get_dfs_sampleTime(void)
{

	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	return (struct sampleTimeTable *)map->persist.dfs.sampleTime;
}

u32 ipc_get_dfs_gov(void)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	return map->persist.dfs.governor;
}

void ipc_set_dfs_gov(uint32_t gov)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	map->persist.dfs.governor = gov;
}

void ipc_set_dfs_numSensor(uint32_t num)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	map->persist.dfs.numSensor = num;
}

u32 ipc_get_dfs_numSensor(void)
{
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

	return map->persist.dfs.numSensor;
}

static void ipc_print_logbuf(void)
{
	struct ipc_logbuf *logbuf = &ipc.ipc_map->logbuf;

	CSP_PRINTF_DEBUG
	    ("%s: channel: eq:%d, dq:%d, size:%d, full:%d, dbg_full_cnt:%d, err(overwrite):%d, ipc-reqcnt:%d, fw:%d\n",
	     __func__, logbuf->eq, logbuf->dq, logbuf->size, logbuf->full,
	     logbuf->dbg_full_cnt, logbuf->errcnt, logbuf->reqcnt,
	     logbuf->fw_num);
}

void ipc_dump(void)
{
	if (strncmp(CHUB_IPC_MAGIC, ipc.ipc_map->magic, sizeof(CHUB_IPC_MAGIC))) {
		CSP_PRINTF_DEBUG("%s: [%s]: ipc crash\n", NAME_PREFIX, __func__);
		return;
	}

	CSP_PRINTF_DEBUG("[%s]: magic:%s\n", __func__, ipc.ipc_map->magic);
	ipc_print_logbuf();
	cipc_dump(CIPC_USER_AP2CHUB);
	cipc_dump(CIPC_USER_CHUB2AP);
}


struct ipc_info *ipc_get_info(void)
{
	return &ipc;
}

enum ipc_fw_loglevel ipc_logbuf_loglevel(enum ipc_fw_loglevel loglevel, int set)
{
	if (ipc.ipc_map) {
		struct ipc_logbuf *logbuf = &ipc.ipc_map->logbuf;

		if (set)
			logbuf->loglevel = (u8) loglevel;
		return (enum ipc_fw_loglevel)logbuf->loglevel;
	}

	return 0;
}

void ipc_write_value(enum ipc_value_id id, u64 val)
{
	if (ipc.ipc_map)
		ipc.ipc_map->value[id] = val;
}

u64 ipc_read_value(enum ipc_value_id id)
{
	if (ipc.ipc_map)
		return ipc.ipc_map->value[id];
	return 0;
}

void ipc_write_hw_value(enum ipc_value_id id, u64 val)
{
	u32 low = val & 0xffffffff;
	u32 high;

	switch (id) {
	case IPC_VAL_HW_BOOTMODE:
		ipc_hw_write_shared_reg(ipc.mb_base, low, SR_BOOT_MODE);
		break;
	case IPC_VAL_HW_AP_STATUS:
		ipc_hw_write_shared_reg(ipc.mb_base, low, SR_CHUB_ALIVE);
		break;
	case IPC_VAL_HW_DEBUG:
		high = val >> 32;
		ipc_hw_write_shared_reg(ipc.mb_base, low, SR_DEBUG_VAL_LOW);
		ipc_hw_write_shared_reg(ipc.mb_base, high, SR_DEBUG_VAL_HIGH);
		break;
	default:
		CSP_PRINTF_ERROR("%s: invalid id:%d\n", __func__, id);
		break;
	};
}

u64 ipc_read_hw_value(enum ipc_value_id id)
{
	u64 val = 0;
	u32 low;
	u64 high;

	switch (id) {
	case IPC_VAL_HW_BOOTMODE:
		val = ipc_hw_read_shared_reg(ipc.mb_base, SR_BOOT_MODE);
		break;
	case IPC_VAL_HW_AP_STATUS:
		val = ipc_hw_read_shared_reg(ipc.mb_base, SR_3);
		break;
	case IPC_VAL_HW_DEBUG:
		low = ipc_hw_read_shared_reg(ipc.mb_base, SR_DEBUG_VAL_LOW);
		high = ipc_hw_read_shared_reg(ipc.mb_base, SR_DEBUG_VAL_HIGH);
		val = low | (high << 32);
		break;
	default:
		CSP_PRINTF_ERROR("%s: invalid id:%d\n", __func__, id);
		break;
	};
	return val;
}
#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
#define SENSORTASK_NO_TS (3)
static struct sensor_check_cnt cnt;

int ipc_sensor_alive_check(void)
{

	if (cnt.sensor_cnt_last == ipc.ipc_map->sensor_cnt ||
		cnt.event_rtc_last == ipc.ipc_map->event_rtc_cnt) {
		cnt.sensor_cnt_no_update++;
	} else {
		cnt.sensor_cnt_last = ipc.ipc_map->sensor_cnt;
		cnt.event_flush_last = ipc.ipc_map->event_flush_cnt;
		cnt.event_rtc_last = ipc.ipc_map->event_rtc_cnt;
		cnt.event_hrm_last = ipc.ipc_map->event_hrm_cnt;
		cnt.rtc_expired_last = ipc.ipc_map->rtc_expired_cnt;
		cnt.sensor_cnt_no_update = 0;
	}

	nanohub_info("%s: sensor_cnt:%d/%d, evflush:%d/%d, \
			evrtc:%d/%d, evhrm:%d/%d, rtcexp:%d/%d, no_cnt:%d\n",
			__func__, ipc.ipc_map->sensor_cnt, cnt.sensor_cnt_last,
		ipc.ipc_map->event_flush_cnt, cnt.event_flush_last,
		ipc.ipc_map->event_rtc_cnt, cnt.event_rtc_last,
		ipc.ipc_map->event_hrm_cnt, cnt.event_hrm_last,
		ipc.ipc_map->rtc_expired_cnt, cnt.rtc_expired_last,
		cnt.sensor_cnt_no_update);

	if (cnt.sensor_cnt_no_update > SENSORTASK_NO_TS) {
		cnt.sensor_cnt_last = 0;
		cnt.sensor_cnt_no_update = 0;
		ipc.ipc_map->sensor_cnt = 0;
		return -1;
	}

	return 0;
}

#endif

static void *ipc_set_map(char *sram_base)
{
	struct chub_bootargs *map =
	    (struct chub_bootargs *)(sram_base + MAP_INFO_OFFSET);

	CSP_PRINTF_INFO("%s: ipc_map_area:0x%x, offset:0x%x\n", __func__,
			 sizeof(struct ipc_map_area), CIPC_START_OFFSET);
	if (sizeof(struct ipc_map_area) > CIPC_START_OFFSET) {
		CSP_PRINTF_ERROR("%s: ipc_map exceeds cipc offset, boot fail!\n", __func__);
		return 0;
	}

#ifdef AP_IPC
	if (strncmp(OS_UPDT_MAGIC, map->magic, sizeof(OS_UPDT_MAGIC))) {
		CSP_PRINTF_ERROR("%s: %s: %p has wrong magic key: %s -> %s\n",
				 NAME_PREFIX, __func__, map, OS_UPDT_MAGIC,
				 map->magic);
		return 0;
	}

	if (map->ipc_version != IPC_VERSION) {
		CSP_PRINTF_ERROR
		    ("%s: %s: ipc_version doesn't match: AP %d, Chub: %d\n",
		     NAME_PREFIX, __func__, IPC_VERSION, map->ipc_version);
		return 0;
	}

	CSP_PRINTF_INFO("%s enter: version:%x, bootarg_size:%d\n",
		__func__, map->ipc_version, sizeof(struct chub_bootargs));
	if (sizeof(struct chub_bootargs) > MAP_INFO_MAX_SIZE) {
		CSP_PRINTF_ERROR
		    ("%s: %s: chub_bootargs size bigger than max %d > %d", NAME_PREFIX,
		     __func__, sizeof(struct chub_bootargs), MAP_INFO_MAX_SIZE);
		return 0;
	}

	if (sizeof(struct ipc_map_area) > CIPC_START_OFFSET) {
		CSP_PRINTF_ERROR
		    ("%s: %s: ipc_map_area size bigger than max %d > %d", NAME_PREFIX,
		     __func__, sizeof(struct ipc_map_area), CIPC_START_OFFSET);
		return 0;
	}
#endif

	ipc.sram_base = sram_base;

	ipc_set_info(IPC_REG_BL, sram_base, map->bl_end - map->bl_start, "bl");
	ipc_set_info(IPC_REG_BL_MAP, map, sizeof(struct chub_bootargs),
		     "bl_map");
	ipc_set_info(IPC_REG_OS, sram_base + map->code_start,
		     map->code_end - map->code_start, "os");
	ipc_set_info(IPC_REG_IPC, sram_base + map->ipc_start,
		     map->ipc_end - map->ipc_start, "ipc");
	ipc_set_info(IPC_REG_RAM, sram_base + map->ram_start,
		     map->ram_end - map->ram_start, "ram");
	ipc_set_info(IPC_REG_DUMP, sram_base + map->dump_start,
		     map->dump_end - map->dump_start, "dump");

	ipc.ipc_map = ipc.ipc_addr[IPC_REG_IPC].base;

#ifdef AP_IPC
	if (ipc_get_size(IPC_REG_IPC) < sizeof(struct ipc_map_area)) {
		CSP_PRINTF_INFO
		    ("%s: fails. ipc size (0x%x) should be increase to 0x%x\n",
		     __func__, ipc_get_size(IPC_REG_IPC),
		     sizeof(struct ipc_map_area));
		return 0;
	}
	memset_io(ipc_get_base(IPC_REG_IPC), 0, ipc_get_size(IPC_REG_IPC));
#endif

	ipc.ipc_map->logbuf.size = LOGBUF_TOTAL_SIZE;
	ipc_set_info(IPC_REG_LOG, &ipc.ipc_map->logbuf,
		     sizeof(struct ipc_logbuf), "log");
	ipc_set_info(IPC_REG_PERSISTBUF, &ipc.ipc_map->persist,
		     sizeof(struct chub_persist), "persist");
	ipc_set_info(IPC_REG_IPC_SENSORINFO, &ipc.ipc_map->sensormap,
		     sizeof(struct sensor_map), "sensorinfo");

#ifdef CHUB_IPC
	strncpy(&ipc.ipc_map->magic[0], CHUB_IPC_MAGIC, sizeof(CHUB_IPC_MAGIC));

#ifdef SEOS
	ipc.ipc_map->logbuf.loglevel = ipc_get_chub_rtlogmode();
	if (!ipc_have_sensor_info(&ipc.ipc_map->sensormap)) {
		CSP_PRINTF_INFO("%s:ipc set sensormap and magic:%p\n", __func__,
				&ipc.ipc_map->sensormap);
		memset(&ipc.ipc_map->sensormap, 0, sizeof(struct sensor_map));
		strncpy(&ipc.ipc_map->sensormap.magic[0], SENSORMAP_MAGIC,
			sizeof(SENSORMAP_MAGIC));
	}
#endif
	if ((CHUB_DFS_SIZE * sizeof(u32)) <
	    (sizeof(struct sampleTimeTable) * MAX_SENS_NUM)) {
		CSP_PRINTF_ERROR
		    ("%s: dfs persist size %d should be bigger than %d\n",
		     NAME_PREFIX, CHUB_DFS_SIZE,
		     (sizeof(struct sampleTimeTable) * MAX_SENS_NUM));
		return NULL;
	}
#endif

	/* map printout */
	CSP_PRINTF_INFO("IPC %s: map info(v%u,bt:%d,rt:%d)\n",
			NAME_PREFIX, map->ipc_version, ipc_get_chub_bootmode(),
			ipc_get_chub_rtlogmode());
	CSP_PRINTF_INFO("dump       (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_DUMP),
			ipc_get_size(IPC_REG_DUMP));
	CSP_PRINTF_INFO("bl         (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_BL),
			ipc_get_size(IPC_REG_BL));
	CSP_PRINTF_INFO("os         (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_OS),
			ipc_get_size(IPC_REG_OS));
	CSP_PRINTF_INFO("ipc        (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_IPC),
			ipc_get_size(IPC_REG_IPC));
	CSP_PRINTF_INFO("sensormap  (0x%-5x %6x)\n",
			ipc_get_offset(IPC_REG_IPC_SENSORINFO),
			ipc_get_size(IPC_REG_IPC_SENSORINFO));
	CSP_PRINTF_INFO("persistbuf (0x%-5x %6x)\n",
			ipc_get_offset(IPC_REG_PERSISTBUF),
			ipc_get_size(IPC_REG_PERSISTBUF));
	CSP_PRINTF_INFO("log        (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_LOG),
			ipc_get_size(IPC_REG_LOG));
	CSP_PRINTF_INFO("ram        (0x%-5x %6x)\n", ipc_get_offset(IPC_REG_RAM),
			ipc_get_size(IPC_REG_RAM));
	return &ipc.ipc_map;
}

struct ipc_info *ipc_init(enum ipc_owner owner, enum ipc_mb_id mb_id,
			  void *sram_base, void *mb_base,
			  struct cipc_funcs *func)
{
	ipc.mb_base = mb_base;
	if (owner == IPC_OWN_CHUB || owner == IPC_OWN_AP) {
		ipc.owner = owner;
		ipc.my_mb_id = mb_id;
		ipc.opp_mb_id =
		    (mb_id == IPC_SRC_MB0) ? IPC_DST_MB1 : IPC_SRC_MB0;
		ipc_set_map(sram_base);
		if (!ipc.ipc_map) {
			CSP_PRINTF_ERROR("%s: ipc_map is NULL.\n", __func__);
			return NULL;
		}
	} else {
		return NULL;
	}

	CSP_PRINTF_INFO("%s: owner:%d, mb_id:%d, func:%p\n", __func__, owner,
			mb_id, func);

	ipc.cipc = chub_cipc_init(owner, sram_base, func);
	if (!ipc.cipc) {
		CSP_PRINTF_ERROR("%s: cipc_init fails\n", __func__);
		return NULL;
	}

	ipc.ipc_map->cipc_base = cipc_get_base(CIPC_REG_CIPC_BASE);
	CSP_PRINTF_INFO("%s: done. sram_base:+%x\n", __func__,
			ipc_get_offset_addr(sram_base));

	return &ipc;
}

