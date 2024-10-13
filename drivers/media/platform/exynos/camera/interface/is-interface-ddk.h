/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_INTERFACE_LIBRARY_H
#define IS_INTERFACE_LIBRARY_H

#include "is-core.h"
#include "is-interface-library.h"
#include "is-param.h"
#include "is-config.h"

#define CHAIN_ID_MASK		0x0000000F
#define CHAIN_ID_SHIFT		0
#define INSTANCE_ID_MASK	0x000000F0
#define INSTANCE_ID_SHIFT	4
#define REPROCESSING_FLAG_MASK	0x00000F00
#define REPROCESSING_FLAG_SHIFT	8
#define INPUT_TYPE_MASK		0x0000F000
#define INPUT_TYPE_SHIFT	12
#define FRAME_TYPE_MASK		0x000F0000
#define FRAME_TYPE_SHIFT	16
#define POSITION_ID_MASK	0xF000000
#define POSITION_ID_SHIFT	28

/* num_buffer of shot */
#define SW_FRO_NUM_SHIFT	16
#define CURR_INDEX_SHIFT	24

struct is_lib_isp {
	void				*object;
	struct lib_interface_func	*func;
	ulong				tune_count;
};

enum lib_cb_event_type {
	LIB_EVENT_CONFIG_LOCK			= 1,
	LIB_EVENT_FRAME_START			= 2,
	LIB_EVENT_FRAME_END			= 3,
	LIB_EVENT_DMA_A_OUT_DONE		= 4,
	LIB_EVENT_DMA_B_OUT_DONE		= 5,
	LIB_EVENT_FRAME_START_ISR		= 6,
	LIB_EVENT_ERROR_CIN_OVERFLOW		= 7,
	LIB_EVENT_ERROR_CIN_COLUMN		= 8,
	LIB_EVENT_ERROR_CIN_LINE		= 9,
	LIB_EVENT_ERROR_CIN_TOTAL_SIZE		= 10,
	LIB_EVENT_ERROR_COUT_OVERFLOW		= 11,
	LIB_EVENT_ERROR_COUT_COLUMN		= 12,
	LIB_EVENT_ERROR_COUT_LINE		= 13,
	LIB_EVENT_ERROR_COUT_TOTAL_SIZE		= 14,
	LIB_EVENT_ERROR_CONFIG_LOCK_DELAY	= 15,
	LIB_EVENT_ERROR_COMP_OVERFLOW		= 16,
	LIB_EVENT_END
};

struct lib_callback_result {
	u32	fcount;
	u32	stripe_region_id;
	u32	reserved[4];
};

struct lib_callback_func {
	void (*camera_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id, void *data);
	void (*io_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id);
};

struct lib_tune_set {
	u32 index;
	ulong addr;
	u32 size;
	int decrypt_flag;
};

#define LIB_ISP_ADDR		(DDK_LIB_ADDR + LIB_ISP_OFFSET)
enum lib_func_type {
	LIB_FUNC_3AA = 1,
	LIB_FUNC_ISP,
	LIB_FUNC_TPU,
	LIB_FUNC_VRA,
	LIB_FUNC_DCP,
	LIB_FUNC_CLAHE,
	LIB_FUNC_YUVPP,
	LIB_FUNC_LME,

	LIB_FUNC_CSTAT,
	LIB_FUNC_BYRP,
	LIB_FUNC_RGBP,
	LIB_FUNC_MCFP,
	LIB_FUNC_YUVP,
	LIB_FUNC_TYPE_END
};

enum lib_cmd_type {
	LIB_CMD_OVERFLOW_RECOVERY = 1,
	LIB_CMD_TIMEOUT = 2,
};

enum lib_frame_type {
	/* Sensor HDR mode frame types */
	LIB_FRAME_HDR_SINGLE,
	LIB_FRAME_HDR_LONG,
	LIB_FRAME_HDR_SHORT,
};

enum is_hw_lib_mode {
	USE_ONLY_DDK	= 0,
	USE_DRIVER	= 1,
};

enum is_hw_event_id {
	/* Global events [1:99] */
	EVENT_FRAME_START = 1,
	EVENT_FRAME_END,
	EVENT_ID_END,
};

struct lib_system_config {
	enum lib_cmd_type cmd;
	u32 args[9];
};

typedef u32 (*register_lib_isp)(u32 type, void **lib_func);
static const register_lib_isp get_lib_func = (register_lib_isp)LIB_ISP_ADDR;

struct lib_interface_func {
	int (*chain_create)(u32 chain_id, ulong addr, u32 offset,
		const struct lib_callback_func *cb);
	int (*object_create)(void **pobject, u32 obj_info, void *hw);
	int (*chain_destroy)(u32 chain_id);
	int (*object_destroy)(void *object, u32 sensor_id);
	int (*stop)(void *object, u32 instance_id, u32 immediate);
	int (*recovery)(u32 instance_id);
	int (*set_param)(void *object, void *param_set);
	int (*set_ctrl)(void *object, u32 instance_id, u32 frame_number,
		struct camera2_shot *shot);
	int (*shot)(void *object, void *param_set, struct camera2_shot *shot, u32 num_buffers);
	int (*get_meta)(void *object, u32 instance_id, u32 frame_number,
		struct camera2_shot *shot);
#if IS_ENABLED(USE_DDK_INTF_CAP_META)
	int (*get_cap_meta)(void *object, u32 instance_id, u32 frame_number,
		u32 size, ulong addr);
#endif
	int (*create_tune_set)(void *isp_object, u32 instance_id, struct lib_tune_set *set);
	int (*apply_tune_set)(void *isp_object, u32 instance_id, u32 index, u32 scenario_idx);
	int (*delete_tune_set)(void *isp_object, u32 instance_id, u32 index);
#if defined(USE_DDK_INTF_LIC_OFFSET)
	int (*set_line_buffer_offset)(u32 index, u32 num_set, u32 *offset);
#else
	int (*set_line_buffer_offset)(u32 index, u32 num_set, u32 *offset, u32 trigger_context);
#endif
	int (*change_chain)(void *isp_object, u32 instance_id, u32 chain_id);
	int (*load_cal_data)(void *isp_object, u32 instance_id, ulong kvaddr);
	int (*get_cal_data)(void *isp_object, u32 instance_id,
		struct cal_info *data, int type);
	int (*sensor_info_mode_chg)(void *isp_object, u32 instance_id,
		struct camera2_shot *shot);
	int (*sensor_update_ctl)(void *isp_object, u32 instance_id,
		u32 frame_count, struct camera2_shot *shot);
	int (*set_system_config)(struct lib_system_config *config);
#if defined(USE_DDK_INTF_LIC_OFFSET)
	int (*set_line_buffer_trigger)(u32 trigger_value, u32 trigger_event);
#endif
	int (*event_notifier)(int hw_id, int instance_id, u32 fcount,
			int event_id, u32 strip_index, void *data);
#if defined(USE_OFFLINE_PROCESSING)
	int (*get_offline_data)(void *isp_object, u32 instance,
		struct offline_data_desc *data_desc, int fcount);
#endif
};

int is_lib_isp_chain_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_object_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 f_type);
void is_lib_isp_chain_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
void is_lib_isp_object_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_set_param(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param);
int is_lib_isp_set_ctrl(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame);
int is_lib_isp_shot(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param_set, struct camera2_shot *shot);
int is_lib_isp_get_meta(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame);
int is_lib_isp_get_cap_meta(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 fcount, u32 size, ulong addr);
void is_lib_isp_stop(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 immediate);
int is_lib_isp_create_tune_set(struct is_lib_isp *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id);
int is_lib_isp_apply_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id, u32 scenario_idx);
int is_lib_isp_delete_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id);
int is_lib_isp_change_chain(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 next_id);
int is_lib_isp_load_cal_data(struct is_lib_isp *this,
	u32 index, ulong addr);
int is_lib_isp_get_cal_data(struct is_lib_isp *this,
	u32 instance_id, struct cal_info *c_info, int type);
int is_lib_isp_sensor_info_mode_chg(struct is_lib_isp *this,
	u32 instance_id, struct camera2_shot *shot);
int is_lib_isp_sensor_update_control(struct is_lib_isp *this,
	u32 instance_id, u32 frame_count, struct camera2_shot *shot);
#if IS_ENABLED(ENABLE_VRA)
int is_lib_isp_convert_face_map(struct is_hardware *hardware,
	struct taa_param_set *param_set, struct is_frame *frame);
#endif
void is_lib_isp_configure_algorithm(void);
int is_lib_isp_reset_recovery(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_event_notifier(struct is_hw_ip *hw_ip, struct is_lib_isp *this,
	int instance, u32 fcount, int event_id, u32 strip_index, void *data);
int is_lib_set_sram_offset(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_get_offline_data(struct is_lib_isp *this, u32 instance,
	void *data_desc, int fcount);
int is_lib_notify_timeout(struct is_lib_isp *this, u32 instance);
#define CALL_LIBOP(lib, op, args...)					\
	({								\
		int ret_call_libop;					\
									\
		is_fpsimd_get_func();					\
		ret_call_libop = ((lib)->func->op ?			\
				(lib)->func->op(args) : -EINVAL);	\
		is_fpsimd_put_func();					\
									\
	ret_call_libop; })

#define CALL_LIBOP_NO_FPSIMD(lib, op, args...)				\
	((lib)->func->op ? (lib)->func->op(args) : -EINVAL)

#define CALL_LIBOP_ISR(lib, op, args...)				\
	({								\
		int ret_call_libop;					\
									\
		is_fpsimd_get_isr();					\
		ret_call_libop = ((lib)->func->op ?			\
				(lib)->func->op(args) : -EINVAL);	\
		is_fpsimd_put_isr();					\
									\
	ret_call_libop; })
#endif
