#ifndef __DSP_DHCP_H__
#define __DSP_DHCP_H__

#include <linux/device.h>
#include <linux/platform_device.h>

#include "npu-device.h"

#define DSP_DHCP_OFFSET(n)	(0x4 * (n))
#define DSP_O3_DHCP_IDX(n)	DSP_DHCP_OFFSET(n)

#if 1 // Pamir
enum dsp_dhcp_idx {
	DSP_DHCP_UNKNOWN		= -1,
	DSP_DHCP_TO_CC_MBOX		= 0,
	DSP_DHCP_TO_HOST_MBOX		= 8,
	DSP_DHCP_LOG_QUEUE		= 16,
	DSP_DHCP_TO_CC_INT_STATUS	= 24,
	DSP_DHCP_TO_HOST_INT_STATUS,
	DSP_DHCP_FW_RESERVED_SIZE,
	DSP_DHCP_IVP_PM_IOVA0,
	DSP_DHCP_IVP_PM_IOVA1,
	DSP_DHCP_IVP_PM_IOVA2,
	DSP_DHCP_IVP_PM_IOVA3,
	DSP_DHCP_IVP_PM_SIZE,
	DSP_DHCP_IVP_DM_IOVA,
	DSP_DHCP_IVP_DM_SIZE,
	DSP_DHCP_IAC_PM_IOVA,
	DSP_DHCP_IAC_PM_SIZE,
	DSP_DHCP_IAC_DM_IOVA,
	DSP_DHCP_IAC_DM_SIZE,
	DSP_DHCP_MAILBOX_VERSION,
	DSP_DHCP_MESSAGE_VERSION,
	DSP_DHCP_FW_LOG_MEMORY_IOVA,
	DSP_DHCP_FW_LOG_MEMORY_SIZE,
	DSP_DHCP_TO_CC_MBOX_MEMORY_IOVA,
	DSP_DHCP_TO_CC_MBOX_MEMORY_SIZE,
	DSP_DHCP_TO_CC_MBOX_POOL_IOVA,
	DSP_DHCP_TO_CC_MBOX_POOL_SIZE,
	DSP_DHCP_RESERVED3,
	DSP_DHCP_KERNEL_MODE,
	DSP_DHCP_DL_OUT_IOVA,
	DSP_DHCP_DL_OUT_SIZE,
	DSP_DHCP_DEBUG_LAYER_START,
	DSP_DHCP_DEBUG_LAYER_END,
	DSP_DHCP_CHIPID_REV,
	DSP_DHCP_PRODUCT_ID,
	DSP_DHCP_NPUS_FREQUENCY,
	DSP_DHCP_VPD_FREQUENCY,
	DSP_DHCP_PWR_CTRL,
	DSP_DHCP_CORE_CTRL,
	DSP_DHCP_INTERRUPT_MODE,
	DSP_DHCP_DRIVER_VERSION,
	DSP_DHCP_FIRMWARE_VERSION,
	DSP_DHCP_FIRMWARE_NPU_TIME,
	DSP_DHCP_FIRMWARE_DSP_TIME,
	DSP_DHCP_USED_COUNT,
	DSP_DHCP_IDX_MAX = DSP_DHCP_USED_COUNT
};
#else // Orange
enum dsp_dhcp_idx {
	DSP_DHCP_UNKNOWN		= -1,
	DSP_DHCP_TO_CC_MBOX		= 0,		/* not used anymore */
	DSP_DHCP_TO_HOST_MBOX		= 8,		/* not used anymore */
	DSP_DHCP_LOG_QUEUE		= 16,		/* not used anymore */
	DSP_DHCP_TO_CC_INT_STATUS	= 24,
	DSP_DHCP_TO_HOST_INT_STATUS,
	DSP_DHCP_FW_RESERVED_SIZE,
	DSP_DHCP_IVP_PM_IOVA,
	DSP_DHCP_RESERVED0,
	DSP_DHCP_IVP_PM_SIZE,
	DSP_DHCP_IVP_DM_IOVA,
	DSP_DHCP_IVP_DM_SIZE,
	DSP_DHCP_IAC_PM_IOVA,
	DSP_DHCP_IAC_PM_SIZE,
	DSP_DHCP_IAC_DM_IOVA,
	DSP_DHCP_IAC_DM_SIZE,
	DSP_DHCP_RESERVED1,
	DSP_DHCP_RESERVED2,
	DSP_DHCP_MAILBOX_VERSION,
	DSP_DHCP_MESSAGE_VERSION,
	DSP_DHCP_FW_LOG_MEMORY_IOVA,
	DSP_DHCP_FW_LOG_MEMORY_SIZE,
	DSP_DHCP_TO_CC_MBOX_MEMORY_IOVA,
	DSP_DHCP_TO_CC_MBOX_MEMORY_SIZE,
	DSP_DHCP_TO_CC_MBOX_POOL_IOVA,
	DSP_DHCP_TO_CC_MBOX_POOL_SIZE,
	DSP_DHCP_RESERVED3,
	DSP_DHCP_KERNEL_MODE,
	DSP_DHCP_DL_OUT_IOVA,
	DSP_DHCP_DL_OUT_SIZE,
	DSP_DHCP_DEBUG_LAYER_START,
	DSP_DHCP_DEBUG_LAYER_END,
	DSP_DHCP_CHIPID_REV,
	DSP_DHCP_PRODUCT_ID,
	DSP_DHCP_NPUS_FREQUENCY,
	DSP_DHCP_VPD_FREQUENCY,
	DSP_DHCP_RESERVED4,
	DSP_DHCP_RESERVED5,
	DSP_DHCP_INTERRUPT_MODE,
	DSP_DHCP_DRIVER_VERSION,
	DSP_DHCP_FIRMWARE_VERSION,
	DSP_DHCP_USED_COUNT,
	DSP_DHCP_IDX_MAX = DSP_DHCP_USED_COUNT
};
#endif

struct dsp_dhcp;

struct dsp_dhcp_mem_rg {
	const char		*mem_name;
	enum dsp_dhcp_idx	iova_idx;
	enum dsp_dhcp_idx	size_idx;
	bool			enabled;
};

union dsp_dhcp_pwr_ctl {
	u32	value;
	struct {
		u16	dsp_pm;
		u16	npu_pm;
	};
};

union dsp_dhcp_core_ctl {
	u32	value;
	struct {
		/* number of supported npu cores */
		u8	npu_cores_nr;
		/* active npu core bitmap */
		u8	npu_active_bm;
		/* number  of supported dsp cores */
		u8	dsp_cores_nr;
		/* active dsp core bitmap */
		u8	dsp_active_bm;
	};
};

#define DHCP_MEM_ENTTRY(name, iv_idx, sz_idx)			\
	{							\
		.mem_name	= name,				\
		.iova_idx	= iv_idx,			\
		.size_idx	= sz_idx,			\
	}

extern void *dsp_dhcp_get_reg_addr(struct dsp_dhcp *dsp_dhcp, u32 offset);
static inline void *dsp_dhcp_get_reg_addr_idx(struct dsp_dhcp *dsp_dhcp,
		enum dsp_dhcp_idx idx)
{
	return dsp_dhcp_get_reg_addr(dsp_dhcp, DSP_DHCP_OFFSET(idx));
}
/*
extern void *dsp_dhcp_get_reg_addr_dev(struct npu_device *rdev, u32 offset);
static inline void *dsp_dhcp_get_reg_addr_idx_dev(struct npu_device *rdev,
		enum dsp_dhcp_idx idx)
{
	return dsp_dhcp_get_reg_addr_dev(rdev, DSP_DHCP_OFFSET(idx));
}
*/

extern u32 dsp_dhcp_update_reg(struct dsp_dhcp *dsp_dhcp,
		u32 offset, u32 val, u32 mask);
static inline u32 dsp_dhcp_read_reg(struct dsp_dhcp *dsp_dhcp, u32 offset)
{
	return dsp_dhcp_update_reg(dsp_dhcp, offset, 0x0, 0x0);
}
static inline u32 dsp_dhcp_write_reg(struct dsp_dhcp *dsp_dhcp,
		u32 offset, u32 val)
{
	return dsp_dhcp_update_reg(dsp_dhcp, offset, val, 0xffffffff);
}

static inline u32 dsp_dhcp_update_reg_idx(struct dsp_dhcp *dsp_dhcp,
		enum dsp_dhcp_idx idx, u32 val, u32 mask)
{
	return dsp_dhcp_update_reg(dsp_dhcp,
			DSP_DHCP_OFFSET(idx), val, mask);
}
static inline u32 dsp_dhcp_read_reg_idx(struct dsp_dhcp *dsp_dhcp,
		enum dsp_dhcp_idx idx)
{
	return dsp_dhcp_update_reg_idx(dsp_dhcp, idx, 0x0, 0x0);
}
static inline u32 dsp_dhcp_write_reg_idx(struct dsp_dhcp *dsp_dhcp,
		enum dsp_dhcp_idx idx, u32 val)
{
	return dsp_dhcp_update_reg_idx(dsp_dhcp, idx, val, 0xffffffff);
}

/*
extern u32 dsp_dhcp_update_reg_dev(struct npu_device *rdev,
		u32 offset, u32 val, u32 mask);
static inline u32 dsp_dhcp_read_reg_dev(struct npu_device *rdev, u32 offset)
{
	return dsp_dhcp_update_reg_dev(rdev, offset, 0x0, 0x0);
}
static inline u32 dsp_dhcp_write_reg_dev(struct npu_device *rdev,
		u32 offset, u32 val)
{
	return dsp_dhcp_update_reg_dev(rdev, offset, val, 0xffffffff);
}

static inline u32 dsp_dhcp_update_reg_idx_dev(struct npu_device *rdev,
		enum dsp_dhcp_idx idx, u32 val, u32 mask)
{
	return dsp_dhcp_update_reg_dev(rdev,
			DSP_DHCP_OFFSET(idx), val, mask);
}
static inline u32 dsp_dhcp_read_reg_idx_dev(struct npu_device *rdev,
		enum dsp_dhcp_idx idx)
{
	return dsp_dhcp_update_reg_idx_dev(rdev, idx, 0x0, 0x0);
}
static inline u32 dsp_dhcp_write_reg_idx_dev(struct npu_device *rdev,
		enum dsp_dhcp_idx idx, u32 val)
{
	return dsp_dhcp_update_reg_idx_dev(rdev, idx, val, 0xffffffff);
}
*/

extern void	dsp_dhcp_update_mem_info(struct dsp_dhcp *dhcp,
			struct dsp_dhcp_mem_rg *mem_rg, u32 iova, u32 size);
extern int dsp_dhcp_update_pwr_status(struct npu_device *rdev,
			u32 type, bool on);

extern int	dsp_dhcp_init(struct npu_device *rdev);
extern void	dsp_dhcp_deinit(struct dsp_dhcp *dhcp);

extern int	dsp_dhcp_probe(struct npu_device *dev);
extern void	dsp_dhcp_remove(struct dsp_dhcp *dhcp);
extern int dsp_dhcp_fw_time(struct npu_session *session, u32 type);

/*
static inline struct dsp_dhcp *dsp_sys_get_dsp_dhcp(struct npu_device *dev){
	return dev->system.dhcp;
}
*/

enum dsp_system_flag {
	DSP_SYSTEM_BOOT,
	DSP_SYSTEM_RESET,
};

enum dsp_to_host_int_num {
	DSP_TO_HOST_INT_BOOT,
	DSP_TO_HOST_INT_MAILBOX,
	DSP_TO_HOST_INT_RESET_DONE,
	DSP_TO_HOST_INT_RESET_REQUEST,
	DSP_TO_HOST_INT_NUM,
};

#endif
