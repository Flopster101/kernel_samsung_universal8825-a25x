// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/miscdevice.h>

#include "is-interface-ddk.h"
#include "pablo-obte.h"

#define JSON_SIZE			0x00800000

#define LIB_ISP_TUNING_ADDR		(DDK_LIB_ADDR + 0x000000c0)
#define PABLO_SHARE_BASE_OFFSET		0x01FC0000
#define PABLO_SHARE_BASE_SIZE		0x00010000
#define AF_LOG_V5_SIZE			0x00020000 /* 128KB for each stream */

#define UNKNOWN_INSTANCE		0xFFFFFFFF
#define INVALID_ORDER_BAYER		4 /* There are four types of Bayer(0,1,2,3) */

#define BLOCK_MAX			64
#define BLOCK_REGISTER_MAX		640
#define BLOCK_MODULE_NAME_MAX		64
#define MAX_STRIP_NUM		3

#define TUNING_DRV_MAGIC_NUMBER		0x20191218

#define TOTALCOUNT_SSX_ID		8
#define TOTALCOUNT_MODULE		8

#define TUNE_ID_REGISTER_DUMP_BY_OBTE	0x20210317
#define MAXSIZE_BUF_NAME		32
#define MAXSIZE_CONVERSION_TABLE 16

#define PABLO_OBTE_INIT	_IOW('S', 0x01, int)
#define PABLO_OBTE_WRITE	_IOW('S', 0x02, int)
#define PABLO_SET_TUNEID	_IOW('S', 0x03, int)
#define PABLO_OBTE_READ	_IOR('S', 0x04, int)
#define PABLO_SET_ISPID	_IOW('S', 0x05, int)
#define PABLO_SHAREBASE_READ	_IOR('S', 0x06, int)
#define PABLO_DEBUGBASE_READ	_IOR('S', 0x07, int)
#define PABLO_SENSOR_INFO_CHG	_IOW('S', 0x08, int)
#define PABLO_DDK_VERSION	_IOR('S', 0x09, int)
#define PABLO_AFLOG_READ	_IOR('S', 0x0A, int)
#define PABLO_SETFILE_VERSION	_IOR('S', 0x0B, int)
#define PABLO_TUNE_STREAM_TYPE	_IOR('S', 0x0C, int)
#define PABLO_TUNE_MODE_TYPE	_IOR('S', 0x0D, int)
#define PABLO_TUNE_EMULATOR_TYPE	_IOR('S', 0x0E, int)
#define PABLO_TUNE_SENSOR_POSITION	_IOR('S', 0x0F, int)
#define PABLO_TUNE_SET_DUMP_STAT_INFO	_IOW('S', 0x10, int)
#define PABLO_TUNE_GET_DUMP_STAT_INFO	_IOR('S', 0x11, int)
#define PABLO_TUNE_SET_NFD_INFO_PARAM	_IOW('S', 0x12, int)
#define PABLO_OBTE_READ_UPPER_BUFFER	_IOR('S', 0x13, int)
#define PABLO_TUNE_SET_DUMP_REGISTERS_INFO	_IOR('S', 0x14, int)
#define PABLO_GET_DEBUG_SIZE_INFO	_IOR('S', 0x15, int)
#define PABLO_SET_VMALLOC_MEMORY	_IOW('S', 0x16, int)

enum tune_type {
	TUNE_MAIN_STREAM,
	TUNE_REPROCESS_STREAM,
	TUNE_MAX,
};

enum tune_mode_type {
	TUNE_MODE_NORMAL                    = 0,
	TUNE_MODE_STRIPEPROCESS             = 1,
	TUNE_MODE_SUPERNIGHTSHOT_DRC_OFF    = 2,
	TUNE_MODE_SUPERNIGHTSHOT_DRC_ON     = 3,
	TUNE_MODE_CAPTURETNRPROCESS         = 4,
	TUNE_MODE_VIDEOHDR                  = 5,
	TUNE_MODE_ZSL_STRIPEPROCESS         = 6,
	TUNE_MODE_RGB_INPUT_TO_ITP          = 7,
	TUNE_MODE_PREVIEWTNRPROCESS         = 8, /* == TUNE_MODE_VIDEOTNRPROCESS*/
	TUNE_MODE_HF_DECOMPOSITION          = 9,
	TUNE_MODE_VIDEOTNRPROCESS_WITH_HF   = 10,
	TUNE_MODE_VIDEOTNRPROCESS_WITH_ZSL_STRIPEPROCESS   = 11,
	UNKNOWN_TUNE_MODE_TYPE              = 0xFF,
	TUNE_MODE_MAX,
};

enum tune_emulator_type {
	TUNE_EMULATOR_OFF,
	TUNE_EMULATOR_ON,
	TUNE_EMULATOR_MAX,
};

enum json_type {
	JASON_TYPE_READ,
	JASON_TYPE_WRITE,
	JASON_TYPE_MAX,
};

enum reg_dump_type {
	REG_DUMP_LEFT = 0,
	REG_DUMP_MID,
	REG_DUMP_RIGHT,
	REG_DUMP_MAX,
};

enum buf_id_pablo_obte {
	BUF_ID_READ,
	BUF_ID_WRITE,
	BUF_ID_SHAREBASE,
	BUF_ID_AF_TUNING,

	BUF_ID_CRDUMP_CSTAT,
	BUF_ID_CRDUMP_BYRP,
	BUF_ID_CRDUMP_RGBP,
	BUF_ID_CRDUMP_DNS,
	BUF_ID_CRDUMP_MCFP0,
	BUF_ID_CRDUMP_MCFP1,
	BUF_ID_CRDUMP_YUVP,
	BUF_ID_CRDUMP_MCSC,
	BUF_ID_CRDUMP_NORMAL_IP,

	TOTALCOUNT_BUF_ID,
	UNKNOWN_BUF_ID	=	0xFFFF,
};

enum hw_reg_dump_id_type {
	HW_REG_DUMP_ID_INVALID	=	-1,
	HW_REG_DUMP_ID_CSTAT	=	0,
	HW_REG_DUMP_ID_IPP	=	HW_REG_DUMP_ID_CSTAT,
	HW_REG_DUMP_ID_BYRP,
	HW_REG_DUMP_ID_ITP	=	HW_REG_DUMP_ID_BYRP,
	HW_REG_DUMP_ID_RGBP,
	HW_REG_DUMP_ID_DNS,
	HW_REG_DUMP_ID_MCFP,
	HW_REG_DUMP_ID_MCFP0	=	HW_REG_DUMP_ID_MCFP,
	HW_REG_DUMP_ID_MCFP1,
	HW_REG_DUMP_ID_YUVP,
	HW_REG_DUMP_ID_MCSC,

	HW_REG_DUMP_ID_MAX,
};

typedef struct lib_tuning_config {
	/*
	 * update_shared_region
	 * 0: do not update on shared region address (shared_addr),
	 * 1: update on shared region address (shared_addr)
	 */
	u32 update_shared_region;
	uintptr_t   shared_addr;
	uintptr_t   af_tuning_addr;
	/* emul_repro_flag for capture tuning emulation composed of preview path */
	u32 emul_repro_flag;
	u32 reserved[4];
} lib_tuning_config_t;

typedef struct lib_interface_func_for_tuning {
	int (*json_readwrite_ctl)(void *ispObject, u32 instance_id,
				  u32 json_type, u32 tuning_id, ulong addr, u32 *size_bytes);
	int (*set_tuning_config)(lib_tuning_config_t *tuning_config);

	int (*reserved_func_ptr_1)(void);

	int magic_number; /* must set TUNING_DRV_MAGIC_NUMBER */

	int (*reserved_cb_func_ptr_1)(void);
	int (*reserved_cb_func_ptr_2)(void);
	int (*reserved_cb_func_ptr_3)(void);
	int (*reserved_cb_func_ptr_4)(void);
} lib_interface_func_for_tuning_t;

typedef struct __module_info {
	u32 id; /* module_id */
	u32 order; /* sensor bayer order */
} module_into_t;

typedef struct __obte_init_param {
	/*
	 * version
	 * old version(supported u32 data): 0 or 1,
	 * new version(supported struct param): 2 or more
	 */
	u32 version;
	/* emul_repro_flag for capture tuning emulation composed of preview path */
	u32 emul_repro_flag;
	u32 module_count;
	module_into_t module_info[TOTALCOUNT_MODULE];
	u32 dumpbuffer_maxsize;
} obte_init_param_t;

typedef struct __dump_block_info {
	u32 register_count;
	char block_name[BLOCK_MODULE_NAME_MAX];
	u32 register_left_value[BLOCK_REGISTER_MAX];
	u32 register_mid_value[BLOCK_REGISTER_MAX];
	u32 register_right_value[BLOCK_REGISTER_MAX];
	u32 register_address[BLOCK_REGISTER_MAX];
} dump_register_block;

typedef struct __dump_ip_info {
	u32 ip_id;
	u32 buf_type;   /* reference to REG_DUMP_BUF_TYPE */
	u32 stripe_num;
	u32 block_count;
	u32 json_size;
	u32 base_address;
	u32 address_range;
	dump_register_block block_info[BLOCK_MAX];
} dump_register_ip_kernel;

typedef struct __pablo_ssx_status {
	bool enable;
	s32 status;
	u32 count_called;
} pablo_ssx_status_t;

typedef struct __pablo_debug_size_info {
	u32 size_of_camera2_node;
	u32 size_of_camera2_shot;
} pablo_debug_size_info_t;

struct base_address_conversion_table_t {
	u32 base_address;
	void __iomem *dump_address;
};

static const register_lib_isp get_lib_tuning_func = (register_lib_isp)LIB_ISP_TUNING_ADDR;
static const char pablo_obte_devname[] = "is_simmian";
static lib_interface_func_for_tuning_t *lib_obte_interface;
static obte_init_param_t obte_init_param;
static u32 tune_instance = UNKNOWN_INSTANCE;
static u32 tune_instance_rep = UNKNOWN_INSTANCE;
static unsigned char *_pablo_obte_buf[TOTALCOUNT_BUF_ID];
static char _pablo_obte_buf_name[TOTALCOUNT_BUF_ID][MAXSIZE_BUF_NAME];
static bool obte_status;
static u32 tune_id;
static u32 tune_stream;
static u32 tune_mode;
static u32 tune_emulator;
static u32 tune_sensor_position;
static u32 readsize;
static u32 readsize_upperbuffer;
static pablo_ssx_status_t pablo_ssx_status[TOTALCOUNT_SSX_ID];
static lib_tuning_config_t sharebase;
static spinlock_t slock_json;
static bool use_vmalloc_memory; /* use on memory lack */
static bool init_alloc_memory;
static u32 addr_conv_tbl_num;
static struct base_address_conversion_table_t base_addr_conv_tbl[MAXSIZE_CONVERSION_TABLE];

bool pablo_obte_is_running(void)
{
	return obte_status;
}

char *pablo_obte_buf_name(unsigned int id)
{
	if (id >= TOTALCOUNT_BUF_ID) {
		err_obte("unknown buf id (%d), return null\n", id);
		return NULL;
	}

	return &_pablo_obte_buf_name[id][0];
}

void pablo_obte_buf_setaddr(unsigned int id, unsigned char *addr)
{
	if (id >= TOTALCOUNT_BUF_ID)
		err_obte("unknown buf id (%d)\n", id);
	else
		_pablo_obte_buf[id] = addr;
}

unsigned char *pablo_obte_buf_getaddr(unsigned int id)
{
	if (id >= TOTALCOUNT_BUF_ID) {
		err_obte("unknown buf id (%d)\n", id);
		return NULL;
	}

	return _pablo_obte_buf[id];
}

bool pablo_obte_buf_alloc(unsigned int id, char *buf_name, unsigned long size)
{
	unsigned char *new_addr;
	char *new_name = pablo_obte_buf_name(id);

	if (use_vmalloc_memory)
		new_addr = vzalloc(size);
	else
		new_addr = kzalloc(size, GFP_KERNEL);

	if (!new_addr) {
		err_obte("failed pablo_obte_alloc(%s)\n", buf_name);
		return false;
	}

	pablo_obte_buf_setaddr(id, new_addr);
	if (new_name)
		snprintf(new_name, MAXSIZE_BUF_NAME - 1, "%s", buf_name);
	else
		err_obte("failed new_name(%d/%s)\n", id, buf_name);

	dbg_obte("alloc('%s')\n", pablo_obte_buf_name(id));

	return true;
}

void pablo_obte_buf_free(unsigned int id)
{
	unsigned char *addr = pablo_obte_buf_getaddr(id);

	if (addr) {
		if (use_vmalloc_memory)
			vfree(addr);
		else
			kfree(addr);

		pablo_obte_buf_setaddr(id, NULL);
		dbg_obte("free('%s')\n", pablo_obte_buf_name(id));
	}
}

void pablo_obte_buf_init(void)
{
	unsigned int id;

	for (id = 0; id < TOTALCOUNT_BUF_ID; id++) {
		_pablo_obte_buf[id] = NULL;
		memset(&_pablo_obte_buf_name[id][0], 0, MAXSIZE_BUF_NAME);
	}

	sharebase.shared_addr = (ulong)0;
	sharebase.af_tuning_addr = (ulong)0;
}

void pablo_obte_buf_free_all(void)
{
	int i;

	for (i = 0; i < TOTALCOUNT_BUF_ID; i++)
		pablo_obte_buf_free(i);

	pablo_obte_buf_init();
	init_alloc_memory = false;
}

bool pablo_obte_buf_alloc_all(void)
{
	if (init_alloc_memory) {
		dbg_obte("already allocated pablo_obte buffer\n");
		return true;
	}

	if (!pablo_obte_buf_alloc(BUF_ID_READ, __stringify_1(BUF_ID_READ),
				  JSON_SIZE / 2))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_WRITE, __stringify_1(BUF_ID_WRITE),
				  JSON_SIZE / 2))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_SHAREBASE, __stringify_1(BUF_ID_SHAREBASE),
				  PABLO_SHARE_BASE_SIZE))
		goto error_pablo_obte_buf_alloc_all;
	else
		sharebase.shared_addr = (ulong)pablo_obte_buf_getaddr(BUF_ID_SHAREBASE);

	if (!pablo_obte_buf_alloc(BUF_ID_AF_TUNING, __stringify_1(BUF_ID_AF_TUNING),
				  AF_LOG_V5_SIZE))
		goto error_pablo_obte_buf_alloc_all;
	else
		sharebase.af_tuning_addr = (ulong)pablo_obte_buf_getaddr(BUF_ID_AF_TUNING);

	/* dump register by obte buffer */
	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_CSTAT, __stringify_1(BUF_ID_CRDUMP_CSTAT),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_BYRP, __stringify_1(BUF_ID_CRDUMP_BYRP),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_RGBP, __stringify_1(BUF_ID_CRDUMP_RGBP),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_MCFP0, __stringify_1(BUF_ID_CRDUMP_MCFP0),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_MCFP1, __stringify_1(BUF_ID_CRDUMP_MCFP1),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_DNS, __stringify_1(BUF_ID_CRDUMP_DNS),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_YUVP, __stringify_1(BUF_ID_CRDUMP_YUVP),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_MCSC, __stringify_1(BUF_ID_CRDUMP_MCSC),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	if (!pablo_obte_buf_alloc(BUF_ID_CRDUMP_NORMAL_IP, __stringify_1(BUF_ID_CRDUMP_NORMAL_IP),
				  sizeof(dump_register_ip_kernel)))
		goto error_pablo_obte_buf_alloc_all;

	dbg_obte("allocated pablo_obte buffer\n");
	init_alloc_memory = true;

	return true;

error_pablo_obte_buf_alloc_all:
	pablo_obte_buf_free_all();

	return false;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static lib_interface_func_for_tuning_t *pablo_kunit_obte_interface;
void pablo_kunit_obte_set_interface(void *itf)
{
	pablo_kunit_obte_interface = itf;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_obte_set_interface);
#endif

int __nocfi pablo_obte_get_ddk_tuning_interface(void)
{
	if (lib_obte_interface) {
		dbg_obte("already register\n");
		return 0;
	}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
	lib_obte_interface = pablo_kunit_obte_interface;
#else
	get_lib_tuning_func(LIB_FUNC_3AA, (void **)&lib_obte_interface);
#endif

	dbg_obte("register ddk tuning interface\n");
	lib_obte_interface->magic_number = TUNING_DRV_MAGIC_NUMBER;
	lib_obte_interface->reserved_cb_func_ptr_1 = NULL;
	lib_obte_interface->reserved_cb_func_ptr_2 = NULL;
	lib_obte_interface->reserved_cb_func_ptr_3 = NULL;
	lib_obte_interface->reserved_cb_func_ptr_4 = NULL;

	if (!pablo_obte_buf_alloc_all()) {
		err_obte("failed buffer allocation\n");
		lib_obte_interface = NULL;
		return -ENOMEM;
	}

	return 0;
}

int __nocfi pablo_obte_init_3aa(u32 instance, bool flag)
{
	int ret = 0;

	if (!lib_obte_interface)
		ret = pablo_obte_get_ddk_tuning_interface();

	/* reprocessing path */
	if (flag) {
		tune_instance_rep = instance;
		if (tune_instance == tune_instance_rep)
			err_obte("tune_instance == tune_instance_rep (%d)\n", instance);
	} else {
		tune_instance = instance;
	}

	dbg_obte("[%d] pablo_obte_init_3aa\n", instance);

	return ret;
}
KUNIT_EXPORT_SYMBOL(pablo_obte_init_3aa);

void pablo_obte_deinit_3aa(u32 instance)
{
	lib_obte_interface = NULL;

	if (instance == tune_instance)
		tune_instance = UNKNOWN_INSTANCE;
	else if (instance == tune_instance_rep)
		tune_instance_rep = UNKNOWN_INSTANCE;
	else
		warn_obte("unknown %d - %d/%d)\n", instance, tune_instance, tune_instance_rep);

	dbg_obte("[%d] called 3aa deinit\n", instance);
}
KUNIT_EXPORT_SYMBOL(pablo_obte_deinit_3aa);

void pablo_obte_setcount_ssx_open(u32 id, s32 status)
{
	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("overflow ssx_id(%u), status(%d)\n", id, status);
		return;
	}

	pablo_ssx_status[id].enable = true;
	pablo_ssx_status[id].status = status;
	pablo_ssx_status[id].count_called++;

	dbg_obte("%d, %d\n", id, pablo_ssx_status[id].count_called);
}

void pablo_obte_setcount_ssx_close(u32 id)
{
	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("overflow ssx_id(%u)\n", id);
		return;
	}

	pablo_ssx_status[id].enable = false;
	pablo_ssx_status[id].count_called--;
	dbg_obte("%d, %d\n", id, pablo_ssx_status[id].count_called);
}

int pablo_obte_buf_getid(int reg_dump_id)
{
	int buf_id;

	switch (reg_dump_id) {
	case HW_REG_DUMP_ID_CSTAT:
		buf_id = BUF_ID_CRDUMP_CSTAT;
		break;
	case HW_REG_DUMP_ID_BYRP:
		buf_id = BUF_ID_CRDUMP_BYRP;
		break;
	case HW_REG_DUMP_ID_RGBP:
		buf_id = BUF_ID_CRDUMP_RGBP;
		break;
	case HW_REG_DUMP_ID_MCFP0:
		buf_id = BUF_ID_CRDUMP_MCFP0;
		break;
	case HW_REG_DUMP_ID_MCFP1:
		buf_id = BUF_ID_CRDUMP_MCFP1;
		break;
	case HW_REG_DUMP_ID_DNS:
		buf_id = BUF_ID_CRDUMP_DNS;
		break;
	case HW_REG_DUMP_ID_YUVP:
		buf_id = BUF_ID_CRDUMP_YUVP;
		break;
	case HW_REG_DUMP_ID_MCSC:
		buf_id = BUF_ID_CRDUMP_MCSC;
		break;
	default:
		err_obte("invalid reg dump id(%d)\n", reg_dump_id);
		buf_id = UNKNOWN_BUF_ID;
		break;
	}

	return buf_id;
}

dump_register_ip_kernel *pablo_obte_get_register_dump_buffer(u32 reg_dump_id)
{
	if (reg_dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("invalid reg dump id(%d)\n", reg_dump_id);
		return NULL;
	}

	return (dump_register_ip_kernel *)pablo_obte_buf_getaddr(
		       pablo_obte_buf_getid(reg_dump_id));
}

bool pablo_is_enable_lpzsl_stripe_mode(void)
{
	return (tune_mode  == TUNE_MODE_STRIPEPROCESS) ? true : false;
}

void pablo_obte_read_register(dump_register_ip_kernel *register_ip_info, u32 dump_type)
{
	int i, j;
	u32 reg_val;
	void __iomem *dump_address = NULL;

	if (!register_ip_info) {
		err_obte("register_ip_info is NULL\n");
		return;
	}

	for (i = 0; i < addr_conv_tbl_num; i++) {
		if (base_addr_conv_tbl[i].base_address == register_ip_info->base_address) {
			dump_address = base_addr_conv_tbl[i].dump_address;
			break;
		}
	}

	if (dump_address == NULL) {
		err_obte("dump_address is NULL: base addr(0x%x), table num(%d)\n",
			 register_ip_info->base_address, i);
		return;
	}

	if (register_ip_info->block_count > BLOCK_MAX) {
		err_obte("invalid block_count (%d)\n",
			 register_ip_info->block_count);
		return;
	}

	for (i = 0; i < register_ip_info->block_count; i++) {
		if (register_ip_info->block_info[i].register_count > BLOCK_REGISTER_MAX) {
			err_obte("invalid block_info: count(%d) type(%d) num(%d)\n",
				 register_ip_info->block_info[i].register_count,
				 register_ip_info->buf_type, i);
			continue;
		}

		for (j = 0; j < register_ip_info->block_info[i].register_count; j++) {
			reg_val = readl(dump_address +
					register_ip_info->block_info[i].register_address[j]);
			if (dump_type == REG_DUMP_LEFT)
				register_ip_info->block_info[i].register_left_value[j] = reg_val;
			else if (dump_type == REG_DUMP_MID)
				register_ip_info->block_info[i].register_mid_value[j] = reg_val;
			else if (dump_type == REG_DUMP_RIGHT)
				register_ip_info->block_info[i].register_right_value[j] = reg_val;
		}
	}
}

void dump_strip_register_info(u32 instance, u32 reg_dump_id, u32 stripe_idx, u32 stripe_num)
{
	dump_register_ip_kernel *register_ip_info;

	/* preview stream do not need to dump register. */
	if (pablo_is_enable_lpzsl_stripe_mode() && (instance != tune_instance_rep))
		return;

	if (reg_dump_id >= HW_REG_DUMP_ID_MAX) {
		err_obte("reg_dump_id is incorrect reg_dump_id: %d\n", reg_dump_id);
		return;
	}

	register_ip_info = pablo_obte_get_register_dump_buffer(reg_dump_id);
	if (!register_ip_info) {
		err_obte("register_ip_info is NULL\n");
		return;
	}

	register_ip_info->stripe_num = stripe_num;

	if (stripe_num > 1) {
		if (stripe_idx == 0)
			pablo_obte_read_register(register_ip_info, REG_DUMP_LEFT);
		else if (stripe_idx < stripe_num - 1)
			pablo_obte_read_register(register_ip_info, REG_DUMP_MID);
	}
}

void pablo_obte_regdump(u32 instance_id, u32 hw_id, u32 stripe_idx, u32 stripe_num)
{
	u32 reg_dump_id = HW_REG_DUMP_ID_MAX;

	if (stripe_num > MAX_STRIP_NUM) {
		err_obte("invalid stripe_num(%d): MAX_STRIP_NUM(%d), hw_id(%d)\n",
			 stripe_num, MAX_STRIP_NUM, hw_id);
		return;
	}

	switch (hw_id) {
	case DEV_HW_BYRP:
		reg_dump_id = HW_REG_DUMP_ID_BYRP;
		break;
	case DEV_HW_RGBP:
		reg_dump_id = HW_REG_DUMP_ID_RGBP;
		break;
	case DEV_HW_MCFP:
		reg_dump_id = HW_REG_DUMP_ID_MCFP;
		break;
	case DEV_HW_YPP:
		reg_dump_id = HW_REG_DUMP_ID_YUVP;
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
		reg_dump_id = HW_REG_DUMP_ID_IPP;
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		reg_dump_id = HW_REG_DUMP_ID_ITP;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_MCFP0;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_MCFP1;
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
		reg_dump_id = HW_REG_DUMP_ID_DNS;
		break;
	case DEV_HW_MCSC0:
		reg_dump_id = HW_REG_DUMP_ID_MCSC;
		break;
	default:
		break;
	}

	if (reg_dump_id < HW_REG_DUMP_ID_MAX)
		dump_strip_register_info(instance_id, reg_dump_id, stripe_idx, stripe_num);
}

void pablo_obte_initdata_status(void)
{
	u32 id;

	for (id = 0; id < TOTALCOUNT_SSX_ID; id++) {
		pablo_ssx_status[id].enable = false;
		pablo_ssx_status[id].status = 0;
		pablo_ssx_status[id].count_called = 0;
	}
}

void pablo_obte_initdata(void)
{
	pablo_obte_initdata_status();
}

void pablo_obte_set_status(bool enable)
{
	obte_status = enable;
}

int __nocfi pablo_obte_open(struct inode *ip, struct file *fp)
{
	dbg_obte("pablo_obte_open\n");

	pablo_obte_initdata();
	pablo_obte_set_status(true);

	return 0;
}

static int __nocfi pablo_obte_release(struct inode *ip, struct file *fp)
{
	dbg_obte("pablo_obte_release\n");

	pablo_obte_set_status(false);
	pablo_obte_initdata();

	return 0;
}

ssize_t __nocfi pablo_user_write_json(struct file *file, const char *buf,
				      size_t count, loff_t *ppos)
{
	int ret;
	u32 instance;
	unsigned char *writebuffer;

	pr_debug("pablo_user_write_json\n");
	if (!lib_obte_interface) {
		err_obte("lib_obte_interface is NULL\n");
		return -EINVAL;
	}

	writebuffer = pablo_obte_buf_getaddr(BUF_ID_WRITE);

	if (!writebuffer) {
		err_obte("writebuffer is NULL\n");
		return -EINVAL;
	}

	if (count > JSON_SIZE / 2) {
		err_obte("count size is too large (max size:%d, required size:%d)\n",
			 JSON_SIZE / 2, count);
		return -EINVAL;
	}

	ret = copy_from_user(writebuffer, buf, count);
	if (tune_stream == TUNE_MAIN_STREAM)
		instance = tune_instance;
	else
		instance = tune_instance_rep;

	spin_lock(&slock_json);
	lib_obte_interface->json_readwrite_ctl(NULL, instance, JASON_TYPE_WRITE, 0,
					       (ulong)writebuffer, (u32 *)&count);
	spin_unlock(&slock_json);

	return ret;
}

ssize_t __nocfi pablo_user_read_json(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret;
	u32 instance;
	unsigned char *readbuffer;
	dump_register_ip_kernel *reg_dump_temp_info;
	dump_register_ip_kernel *reg_dump_ip_info;

	pr_debug("pablo_user_read_json\n");
	if (!lib_obte_interface) {
		err_obte("lib_obte_interface is NULL\n");
		return -EINVAL;
	}

	readsize = JSON_SIZE / 2;
	readsize_upperbuffer = 0;
	readbuffer = pablo_obte_buf_getaddr(BUF_ID_READ);

	if (!readbuffer) {
		err_obte("readbuffer is NULL\n");
		return -EINVAL;
	}

	pr_debug("tune_instance(%d) tune_instance_rep(%d) tune_id(%d)\n",
		 tune_instance, tune_instance_rep, tune_id);
	pr_debug("readbuffer(%p) readsize(%d) readsize_upperbuffer(%d)\n",
		 readbuffer, readsize, readsize_upperbuffer);

	if (tune_stream == TUNE_MAIN_STREAM)
		instance = tune_instance;
	else
		instance = tune_instance_rep;

	if (tune_id == TUNE_ID_REGISTER_DUMP_BY_OBTE) {
		reg_dump_temp_info =
			(dump_register_ip_kernel *)pablo_obte_buf_getaddr(BUF_ID_CRDUMP_NORMAL_IP);
		if (!reg_dump_temp_info) {
			err_obte("reg_dump_temp_info is NULL: BUF_ID_CRDUMP_NORMAL_IP\n");
			return -EFAULT;
		}
		memset(reg_dump_temp_info, 0, sizeof(dump_register_ip_kernel));
		ret = copy_from_user(reg_dump_temp_info, (char *)buf,
				     sizeof(dump_register_ip_kernel));
		if (ret) {
			err_obte("failed copy_from_user(reg_dump_temp_info), ret(%d) size(%d)\n",
				 ret, sizeof(dump_register_ip_kernel));
			return ret;
		}

		reg_dump_ip_info =
			pablo_obte_get_register_dump_buffer(reg_dump_temp_info->buf_type);
		if (!reg_dump_ip_info) {
			err_obte("reg_dump_ip_info is NULL, buffer id: %d\n",
				 reg_dump_temp_info->buf_type);
			return -EFAULT;
		}

		pablo_obte_read_register(reg_dump_ip_info, REG_DUMP_RIGHT);
		ret = copy_to_user(buf, reg_dump_ip_info, sizeof(dump_register_ip_kernel));
	} else {
		/* Indirect access using ISP DDK */
		spin_lock(&slock_json);
		lib_obte_interface->json_readwrite_ctl(NULL, instance, JASON_TYPE_READ, tune_id,
						       (ulong)readbuffer, &readsize);
		spin_unlock(&slock_json);

		if (count > JSON_SIZE / 2) {
			err_obte("count size is too large (max size:%d, required size:%d)\n",
				 JSON_SIZE / 2, count);
			return -EINVAL;
		}

		ret = copy_to_user(buf, readbuffer, count);
	}

	return ret;
}

char *get_ioctl_name(unsigned int cmd)
{
	switch (cmd) {
	case PABLO_OBTE_INIT:
		return __stringify_1(PABLO_OBTE_INIT);
	case PABLO_OBTE_WRITE:
		return __stringify_1(PABLO_OBTE_WRITE);
	case PABLO_SET_TUNEID:
		return __stringify_1(PABLO_SET_TUNEID);
	case PABLO_TUNE_STREAM_TYPE:
		return __stringify_1(PABLO_TUNE_STREAM_TYPE);
	case PABLO_TUNE_MODE_TYPE:
		return __stringify_1(PABLO_TUNE_MODE_TYPE);
	case PABLO_TUNE_EMULATOR_TYPE:
		return __stringify_1(PABLO_TUNE_EMULATOR_TYPE);
	case PABLO_TUNE_SENSOR_POSITION:
		return __stringify_1(PABLO_TUNE_SENSOR_POSITION);
	case PABLO_OBTE_READ:
		return __stringify_1(PABLO_OBTE_READ);
	case PABLO_OBTE_READ_UPPER_BUFFER:
		return __stringify_1(PABLO_OBTE_READ_UPPER_BUFFER);
	case PABLO_SHAREBASE_READ:
		return __stringify_1(PABLO_SHAREBASE_READ);
	case PABLO_DEBUGBASE_READ:
		return __stringify_1(PABLO_DEBUGBASE_READ);
	case PABLO_SENSOR_INFO_CHG:
		return __stringify_1(PABLO_SENSOR_INFO_CHG);
	case PABLO_DDK_VERSION:
		return __stringify_1(PABLO_DDK_VERSION);
	case PABLO_AFLOG_READ:
		return __stringify_1(PABLO_AFLOG_READ);
	case PABLO_SETFILE_VERSION:
		return __stringify_1(PABLO_SETFILE_VERSION);
	case PABLO_TUNE_SET_DUMP_STAT_INFO:
		return __stringify_1(PABLO_TUNE_SET_DUMP_STAT_INFO);
	case PABLO_TUNE_GET_DUMP_STAT_INFO:
		return __stringify_1(PABLO_TUNE_GET_DUMP_STAT_INFO);
	case PABLO_TUNE_SET_NFD_INFO_PARAM:
		return __stringify_1(PABLO_TUNE_SET_NFD_INFO_PARAM);
	case PABLO_TUNE_SET_DUMP_REGISTERS_INFO:
		return __stringify_1(PABLO_TUNE_SET_DUMP_REGISTERS_INFO);
	case PABLO_GET_DEBUG_SIZE_INFO:
		return __stringify_1(PABLO_GET_DEBUG_SIZE_INFO);
	case PABLO_SET_VMALLOC_MEMORY:
		return __stringify_1(PABLO_SET_VMALLOC_MEMORY);
	default:
		return "UNKNOWN_OBTE_IOCTL_PARAM";
	}
}

int __nocfi pablo_obte_logdump(unsigned long arg)
{
	struct is_lib_support *lib = is_get_lib_support();
	size_t write_vptr, read_vptr;
	size_t read_cnt, read_cnt1, read_cnt2;
	void *read_ptr;
	int ret;
	struct is_debug *is_debug;

	ulong debug_kva = lib->minfo->kvaddr_debug;
	ulong ctrl_kva  = lib->minfo->kvaddr_debug_cnt;

	is_debug = is_debug_get();

	if (!test_bit(IS_DEBUG_OPEN, &is_debug->state))
		return 0;

	write_vptr = *((int *)(ctrl_kva));
	read_vptr = is_debug->read_vptr;

	if (write_vptr > DEBUG_REGION_SIZE)
		write_vptr = (read_vptr + DEBUG_REGION_SIZE) % (DEBUG_REGION_SIZE + 1);

	if (write_vptr >= read_vptr) {
		read_cnt1 = write_vptr - read_vptr;
		read_cnt2 = 0;
	} else {
		read_cnt1 = DEBUG_REGION_SIZE - read_vptr;
		read_cnt2 = write_vptr;
	}

	read_cnt = read_cnt1 + read_cnt2;
	dbg_obte("library log start(%zd)\n", read_cnt);

	if (read_cnt1) {
		read_ptr = (void *)(debug_kva + is_debug->read_vptr);
		ret = copy_to_user((void __user *)arg, (char *)read_ptr, read_cnt1);
		arg += read_cnt1;
		is_debug->read_vptr += read_cnt1;
	}

	if (is_debug->read_vptr >= DEBUG_REGION_SIZE) {
		if (is_debug->read_vptr > DEBUG_REGION_SIZE)
			err_obte("[DBG] read_vptr(%zd) is invalid", is_debug->read_vptr);

		is_debug->read_vptr = 0;
	}

	if (read_cnt2) {
		read_ptr = (void *)(debug_kva + is_debug->read_vptr);
		ret = copy_to_user((void __user *)arg, (char *)read_ptr, read_cnt2);
		is_debug->read_vptr += read_cnt2;
	}

	dbg_obte("library log end\n");

	return read_cnt;
}

bool pablo_obte_getstatus_ssx(u32 id, pablo_ssx_status_t *curr_status_ptr)
{
	if (curr_status_ptr == NULL) {
		err_obte("ssx[%d] 'curr_status_ptr == NULL'\n", id);
		return false;
	}

	if (id >= TOTALCOUNT_SSX_ID) {
		err_obte("invalid ssx_id(%d)\n", id);
		memset(curr_status_ptr, 0, sizeof(pablo_ssx_status_t));
		return false;
	}

	memcpy(curr_status_ptr, &pablo_ssx_status[id], sizeof(pablo_ssx_status_t));

	return true;
}

long __nocfi pablo_obte_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 data;
	char version[128];
	u32 ssx_id;
	int i = 0;
	void __iomem *dump_address;
	pablo_ssx_status_t curr_ssx_info;
	pablo_debug_size_info_t curr_debug_size;
	dump_register_ip_kernel *reg_dump_temp_info;
	dump_register_ip_kernel *reg_dump_ip_info;

	dbg_obte("cmd=%s\n", get_ioctl_name(cmd));

	switch (cmd) {
	case PABLO_OBTE_INIT:
		memset(&obte_init_param, 0, sizeof(obte_init_param_t));

		/* simmian is terminated abnormally. -> pablo_obte_deinit_3aa() is not called. */
		lib_obte_interface = NULL;
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_INIT: get_user\n");
			return -EFAULT;
		}

		if (data > 1) { /* old version: 0 or 1, new version: 2 or more */
			if (copy_from_user((char *)&obte_init_param,
					   (char __user *) arg, sizeof(obte_init_param_t))) {
				err_obte("PABLO_OBTE_INIT: copy_from_user\n");
				return -EFAULT;
			}

			dbg_obte("PABLO_OBTE_INIT\n");
			dbg_obte("-> version %d\n", obte_init_param.version);
			dbg_obte("-> emul_repro_flag %d\n", obte_init_param.emul_repro_flag);
		} else {
			obte_init_param.emul_repro_flag = data;
		}

		sharebase.update_shared_region = 1;
		sharebase.emul_repro_flag = obte_init_param.emul_repro_flag;

		pablo_obte_get_ddk_tuning_interface();
		if (lib_obte_interface)
			lib_obte_interface->set_tuning_config(&sharebase);
		else
			err_obte("PABLO_OBTE_INIT: invalid lib_obte_interface\n");

		dbg_obte("PABLO_OBTE_INIT %d\n", sharebase.emul_repro_flag);
		break;
	case PABLO_OBTE_WRITE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_WRITE: get_user\n");
			return -EFAULT;
		}
		dbg_obte("PABLO_OBTE_WRITE size %u\n", data);
		break;
	case PABLO_SET_TUNEID:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_SET_TUNEID: get_user\n");
			return -EFAULT;
		}
		tune_id = data;
		dbg_obte("PABLO_SET_TUNEID %x\n", tune_id);
		break;
	case PABLO_TUNE_STREAM_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_STREAM_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_stream = data;
		dbg_obte("PABLO_TUNE_STREAM_TYPE %d\n", tune_stream);
		break;
	case PABLO_TUNE_MODE_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_MODE_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_mode = data;
		dbg_obte("PABLO_TUNE_MODE_TYPE %d\n", tune_mode);
		break;
	case PABLO_TUNE_EMULATOR_TYPE:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_EMULATOR_TYPE: get_user\n");
			return -EFAULT;
		}
		tune_emulator = data;
		dbg_obte("PABLO_TUNE_EMULATOR_TYPE %d\n", tune_emulator);
		break;
	case PABLO_TUNE_SENSOR_POSITION:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_TUNE_SENSOR_POSITION: get_user\n");
			return -EFAULT;
		}
		tune_sensor_position = data;
		dbg_obte("PABLO_TUNE_SENSOR_POSITION %d\n", tune_sensor_position);
		break;
	case PABLO_OBTE_READ:
		data = readsize;
		if (put_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_READ: put_user\n");
			return -EFAULT;
		}
		dbg_obte("PABLO_OBTE_READ size %u\n", data);
		break;
	case PABLO_OBTE_READ_UPPER_BUFFER:
		data = readsize_upperbuffer;
		if (put_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_OBTE_READ_UPPER_BUFFER: put_user\n");
			return -EFAULT;
		}
		dbg_obte("PABLO_OBTE_READ_UPPER_BUFFER size %u\n", data);
		break;
	case PABLO_SHAREBASE_READ:
		dbg_obte("PABLO_SHAREBASE_READ\n");
		ret = copy_to_user((void __user *)arg,
				   (char *)sharebase.shared_addr, PABLO_SHARE_BASE_SIZE);
		if (ret) {
			err_obte("PABLO_SHAREBASE_READ: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_DEBUGBASE_READ:
		dbg_obte("PABLO_DEBUGBASE_READ\n");
		pablo_obte_logdump(arg);

		break;
	case PABLO_SENSOR_INFO_CHG:
		dbg_obte("PABLO_SENSOR_INFO_CHG\n");

		ret = copy_from_user((char *)&curr_ssx_info,
				     (u32 __user *) arg, sizeof(pablo_ssx_status_t));
		if (ret) {
			err_obte("PABLO_SENSOR_INFO_CHG: copy_from_user, ret %d\n", ret);
			return -EFAULT;
		}
		ssx_id = curr_ssx_info.status;
		pablo_obte_getstatus_ssx(ssx_id, &curr_ssx_info);

		ret = copy_to_user((void __user *)arg,
				   (char *)&curr_ssx_info, sizeof(pablo_ssx_status_t));
		if (ret) {
			err_obte("PABLO_SENSOR_INFO_CHG: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_DDK_VERSION:
		dbg_obte("PABLO_DDK_VERSION\n");
		strncpy(version, get_binary_version(IS_BIN_LIBRARY, IS_BIN_LIB_HINT_DDK), 60);
		ret = copy_to_user((void __user *)arg, (char *)version, 60);
		if (ret) {
			err_obte("PABLO_DDK_VERSION: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_AFLOG_READ:
		dbg_obte("PABLO_AFLOG_READ\n");
		ret = copy_to_user((u32 __user *)arg,
				   (char *)sharebase.af_tuning_addr, AF_LOG_V5_SIZE);
		if (ret) {
			err_obte("PABLO_AFLOG_READ: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_SETFILE_VERSION:
		dbg_obte("PABLO_SETFILE_VERSION\n");
		strncpy(version, get_binary_version(IS_BIN_SETFILE, tune_sensor_position), 60);
		ret = copy_to_user((void __user *)arg, (char *)version, 60);
		if (ret) {
			err_obte("PABLO_SETFILE_VERSION: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_TUNE_SET_DUMP_REGISTERS_INFO:
		reg_dump_temp_info =
			(dump_register_ip_kernel *)pablo_obte_buf_getaddr(BUF_ID_CRDUMP_NORMAL_IP);
		if (!reg_dump_temp_info) {
			err_obte("reg_dump_info is NULL\n");
			return -EFAULT;
		}

		ret = copy_from_user((char *)reg_dump_temp_info,
				     (char *) arg, sizeof(dump_register_ip_kernel));
		if (ret) {
			err_obte("PABLO_TUNE_SET_DUMP_REGISTERS_INFO: copy_from_user, ret %d\n",
				 ret);
			return ret;
		}

		reg_dump_ip_info =
			pablo_obte_get_register_dump_buffer(reg_dump_temp_info->buf_type);
		if (reg_dump_ip_info) {
			memcpy(reg_dump_ip_info, reg_dump_temp_info,
			       sizeof(dump_register_ip_kernel));

			if (reg_dump_ip_info->base_address && reg_dump_ip_info->address_range) {
				dump_address = ioremap(reg_dump_ip_info->base_address,
						       reg_dump_ip_info->address_range);
				if (!dump_address) {
					err_obte("ioremap failed\n");
					return -EFAULT;
				}
			} else {
				err_obte("register_ip_info: base addr(0x%x) addr range(0x%x)\n",
					 reg_dump_ip_info->base_address,
					 reg_dump_ip_info->address_range);
				return -EFAULT;
			}

			if (addr_conv_tbl_num < MAXSIZE_CONVERSION_TABLE) {
				for (i = 0; i <= addr_conv_tbl_num; i++) {
					if (base_addr_conv_tbl[i].base_address ==
					    reg_dump_ip_info->base_address)
						break;

					if (i == addr_conv_tbl_num) {
						base_addr_conv_tbl[addr_conv_tbl_num].base_address
							= reg_dump_ip_info->base_address;
						base_addr_conv_tbl[addr_conv_tbl_num].dump_address
							= dump_address;
						addr_conv_tbl_num++;
						break;
					}
				}
			} else {
				err_obte("base_addr_conv_tbl is full\n");
			}
		} else
			err_obte("reg_dump_ip_info buffer is NULL: buf_type(%d)\n",
				 reg_dump_temp_info->buf_type);

		break;
	case PABLO_GET_DEBUG_SIZE_INFO:
		curr_debug_size.size_of_camera2_node = sizeof(struct camera2_node);
		curr_debug_size.size_of_camera2_shot = sizeof(struct camera2_shot);
		ret = copy_to_user((void __user *)arg,
				   (char *)&curr_debug_size, sizeof(pablo_debug_size_info_t));
		if (ret) {
			err_obte("PABLO_GET_DEBUG_SIZE_INFO: copy_to_user, ret %d\n", ret);
			return -EFAULT;
		}
		break;
	case PABLO_SET_VMALLOC_MEMORY:
		if (get_user(data, (u32 __user *) arg)) {
			err_obte("PABLO_SET_VMALLOC_MEMORY: get_user\n");
			return -EFAULT;
		}
		use_vmalloc_memory = data ? true : false;
		dbg_obte("PABLO_SET_VMALLOC_MEMORY %d\n", use_vmalloc_memory);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static long __nocfi pablo_obte_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return pablo_obte_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}

static const struct file_operations pablo_obte_fops = {
	.owner = THIS_MODULE,
	.open = pablo_obte_open,
	.release = pablo_obte_release,
	.write = pablo_user_write_json,
	.read = pablo_user_read_json,
	.unlocked_ioctl = pablo_obte_ioctl,
	.compat_ioctl = pablo_obte_compat_ioctl,
};

static struct miscdevice pablo_obte_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = pablo_obte_devname,
	.fops = &pablo_obte_fops,
};

int pablo_obte_init(void)
{
	int ret;
	init_alloc_memory = false;

	ret = misc_register(&pablo_obte_device);
	spin_lock_init(&slock_json);

	if (ret)
		err_obte("misc_register is-simmian error (%d)\n", ret);

	return ret;
}

void pablo_obte_exit(void)
{
	pablo_obte_buf_free_all();
	addr_conv_tbl_num = 0;
	memset(base_addr_conv_tbl, 0, sizeof(base_addr_conv_tbl));

	misc_deregister(&pablo_obte_device);
	dbg_obte("misc_deregister is-simmian\n");
}

MODULE_AUTHOR("Keunhwi Koo<kh14.koo@samsung.com>");
MODULE_DESCRIPTION("Exynos PABLO OBTE driver");
MODULE_LICENSE("GPL");
