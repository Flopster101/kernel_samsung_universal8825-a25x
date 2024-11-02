#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>

#include "dsp-dhcp.h"
#include "dsp-log.h"
#include "npu-hw-device.h"

#undef DHCP_TIGHT_SIZE_CHECK

struct dsp_dhcp_arr {
	u32	array[DSP_DHCP_IDX_MAX];
};

struct dsp_dhcp {
	struct device		*dev;
	void			*dhcp_ka;
	dma_addr_t		dhcp_iova;
	size_t			dhcp_size;
	struct dsp_dhcp_arr 	*data;	/* point to dhcp base, for future use */
};

void *dsp_dhcp_get_reg_addr(struct dsp_dhcp *dsp_dhcp, u32 offset)
{
#ifdef DHCP_TIGHT_SIZE_CHECK
	if (offset > sizeof(struct dsp_dhcp_arr))
#else
	if (offset >= dsp_dhcp->dhcp_size)
#endif
	{
		dsp_err("offset(%x) exceeds used size(%lx, %lx),"
				"starting at (%llx)\n", offset,
				sizeof(struct dsp_dhcp_arr),
				dsp_dhcp->dhcp_size,
				dsp_dhcp->dhcp_iova);
		BUG_ON(1);
	}

	return dsp_dhcp->dhcp_ka + offset;
}

/*
void *dsp_dhcp_get_reg_addr_dev(struct npu_device *rdev, u32 offset)
{
	struct dsp_dhcp *dsp_dhcp = dsp_sys_get_dsp_dhcp(rdev);

	if (!dsp_dhcp) {
		dsp_err("fail to get dhcp\n");
		return 0;
	}

	return dsp_dhcp_get_reg_addr(dsp_dhcp, offset);
}
*/

u32 dsp_dhcp_update_reg(struct dsp_dhcp *dsp_dhcp,
		u32 offset, u32 val, u32 mask)
{
	u32 v;
	void *reg_addr = dsp_dhcp_get_reg_addr(dsp_dhcp, offset);

	if (mask == 0xffffffff)
		v = val;
	else {
		v = readl(reg_addr);
		v = (v & (~mask)) | (val & mask);
	}

	if (mask)
		writel(v, reg_addr);

	return v;
}

/*
u32 dsp_dhcp_update_reg_dev(struct npu_device *rdev,
		u32 offset, u32 val, u32 mask)
{
	struct dsp_dhcp *dsp_dhcp = dsp_sys_get_dsp_dhcp(rdev);

	if (!dsp_dhcp) {
		dsp_err("fail to get dhcp\n");
		return 0;
	}

	return dsp_dhcp_update_reg(dsp_dhcp, offset, val, mask);
}
*/

static struct dsp_dhcp_mem_rg dsp_dhcp_mem_init[] = {
	DHCP_MEM_ENTTRY("fwmbox",
			DSP_DHCP_TO_CC_MBOX_MEMORY_IOVA,
			DSP_DHCP_TO_CC_MBOX_MEMORY_SIZE),
	DHCP_MEM_ENTTRY("fwmem",
			DSP_DHCP_UNKNOWN,
			DSP_DHCP_FW_RESERVED_SIZE),
	DHCP_MEM_ENTTRY("fwlog",
			DSP_DHCP_FW_LOG_MEMORY_IOVA,
			DSP_DHCP_FW_LOG_MEMORY_SIZE),
	DHCP_MEM_ENTTRY("ivp_pm",
#if 1 // Pamir
			DSP_DHCP_IVP_PM_IOVA0,
#else // Orange
			DSP_DHCP_IVP_PM_IOVA,
#endif
			DSP_DHCP_IVP_PM_SIZE),
	DHCP_MEM_ENTTRY("ivp_dm",
			DSP_DHCP_IVP_DM_IOVA,
			DSP_DHCP_IVP_DM_SIZE),
	DHCP_MEM_ENTTRY("dl_out",
			DSP_DHCP_DL_OUT_IOVA,
			DSP_DHCP_DL_OUT_SIZE),
};
/*
static void dsp_dhcp_prepare_mem_info(struct dsp_dhcp *dhcp)
{
	struct dsp_dhcp_mem_rg *mem_rg = dsp_dhcp_mem_init;
	struct dsp_mem_prop *mem_prop;
	int i;

	for (i = 0; i < ARRAY_SIZE(dsp_dhcp_mem_init); i++, mem_rg++) {
		mem_prop = dsp_mem_get_mem_prop(dhcp->dev,
				mem_rg->mem_name);
		if (!mem_prop || mem_prop->unused)
			mem_rg->enabled = false;
		else {
			mem_rg->enabled = true;
			dsp_info_fl("%d. mem(%s) DHCP enabled\n",
					i, mem_rg->mem_name);
		}
	}
}
*/
void dsp_dhcp_update_mem_info(struct dsp_dhcp *dhcp,
		struct dsp_dhcp_mem_rg *mem_rg, u32 iova, u32 size)
{
	if (!mem_rg)
		return;

	if (mem_rg->iova_idx > DSP_DHCP_UNKNOWN &&
		mem_rg->iova_idx < DSP_DHCP_IDX_MAX)
		dsp_dhcp_write_reg_idx(dhcp, mem_rg->iova_idx, iova);

	if (mem_rg->size_idx > DSP_DHCP_UNKNOWN &&
		mem_rg->size_idx < DSP_DHCP_IDX_MAX)
		dsp_dhcp_write_reg_idx(dhcp, mem_rg->size_idx, size);
}

static void dsp_dhcp_init_mem_info(struct npu_system *system, struct dsp_dhcp *dhcp)
{
	struct dsp_dhcp_mem_rg *mem_rg = dsp_dhcp_mem_init;
	int			i;
	struct npu_memory_buffer *pmem;

	for (i = 0; i < ARRAY_SIZE(dsp_dhcp_mem_init); i++, mem_rg++) {
		//if (!mem_rg->enabled)
		//	continue;
		pmem = npu_get_mem_area(system, mem_rg->mem_name);
		if (pmem)
			dsp_dhcp_update_mem_info(dhcp, mem_rg, pmem->daddr, pmem->size);
	}
}

static void dsp_dhcp_deinit_mem_info(struct dsp_dhcp *dhcp)
{
	struct dsp_dhcp_mem_rg *mem_rg = dsp_dhcp_mem_init;
	int i;

	for (i = 0; i < ARRAY_SIZE(dsp_dhcp_mem_init); i++, mem_rg++) {
		//if (!mem_rg->enabled)
		//	continue;
		//dsp_mem_unprepare_mem_rgn(dhcp->dev,
		//		mem_rg->mem_name);
	}
}

int dsp_dhcp_update_pwr_status(struct npu_device *rdev, u32 type, bool on)
{
	int ret;
	struct dsp_dhcp *dhcp = rdev->system.dhcp;
	union dsp_dhcp_pwr_ctl pwr_ctl_b = {
		.value = dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_PWR_CTRL),
	};
	union dsp_dhcp_pwr_ctl pwr_ctl_a = pwr_ctl_b;

	switch (type) {
	case NPU_HWDEV_ID_DSP:
		pwr_ctl_a.dsp_pm = on;
		break;
	case NPU_HWDEV_ID_NPU:
		pwr_ctl_a.npu_pm = on;
		break;
	default:
		dsp_warn("not supported type(%d)\n", type);
		return -EINVAL;
	}

	if (pwr_ctl_a.value != pwr_ctl_b.value) {
		ret = 0;
		dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PWR_CTRL, pwr_ctl_a.value);
	} else
		ret = 1;	/* same as before */

	dsp_info("(prev:%#x -> current:%#x) / type(%d)\n",
			pwr_ctl_b.value, pwr_ctl_a.value, type);

	return ret;
}

int dsp_dhcp_init(struct npu_device *rdev)
{
#define STATIC_KERNEL	(1)
#define DYNAMIC_KERNEL	(2)

	u32 val = 0;
	struct dsp_dhcp *dhcp = rdev->system.dhcp;

	dsp_info("\n");
	pr_info("dsp_dhcp_init\n");

	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_DEBUG_LAYER_START, rdev->system.layer_start);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_DEBUG_LAYER_END, rdev->system.layer_end);

	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_TO_CC_MBOX, val);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_TO_CC_INT_STATUS,   0);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_TO_HOST_INT_STATUS,   0);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_MAILBOX_VERSION, 0);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_MESSAGE_VERSION, 4);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_KERNEL_MODE, DYNAMIC_KERNEL);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_FIRMWARE_VERSION, 0xdeadbeef);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PWR_CTRL, 0);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_CORE_CTRL, 0);
#if 1 // Pamir
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_IVP_PM_IOVA1, 0x33000000);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_IVP_PM_IOVA2, 0x33000000);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_IVP_PM_IOVA3, 0x33000000);
#else // Orange
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_RESERVED0, 0xdeadbeef);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_RESERVED1, 0xdeadbeef);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_RESERVED2, 0xdeadbeef);
#endif
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_RESERVED3, 0xdeadbeef);

	/* update mem info */
	dsp_dhcp_init_mem_info(&rdev->system, dhcp);
	return 0;
}

void dsp_dhcp_deinit(struct dsp_dhcp *dhcp)
{
	dsp_dhcp_deinit_mem_info(dhcp);
}

int dsp_dhcp_probe(struct npu_device *dev)
{
	struct npu_system *system = &dev->system;
	struct npu_memory_buffer *pmem;
	struct dsp_dhcp		*dhcp;
	pr_info("dsp_dhcp_probe\n");

	dhcp = devm_kmalloc(dev->dev, sizeof(*dhcp), GFP_KERNEL);
	if (!dhcp) {
		dsp_err("fail to alloc for ops\n");
		return 0;
	}

	pmem = npu_get_mem_area(system, "dhcp");
	if (!pmem) {
		dsp_err("fail to prepare dhcp_mem\n");
		goto err_get_dhcp_mem;
	}

	dhcp->dhcp_ka = pmem->vaddr;
	dhcp->dhcp_iova = pmem->daddr;
	dhcp->dhcp_size = pmem->size;

	if (dhcp->dhcp_size < sizeof(struct dsp_dhcp_arr)) {
		dsp_err("provide(%#zx) < used(%lx)\n",
				dhcp->dhcp_size, sizeof(struct dsp_dhcp_arr));
		goto err_dhcp_mem_sz;
	}

	dhcp->data = dhcp->dhcp_ka;
	dhcp->dev = dev->dev;

	system->dhcp = dhcp;
	/* other init routine */
	//dsp_dhcp_prepare_mem_info(dhcp);
	return 0;

err_dhcp_mem_sz:
err_get_dhcp_mem:
	devm_kfree(dev->dev, dhcp);
	return -1;
}

void dsp_dhcp_remove(struct dsp_dhcp *dhcp)
{
	if (!dhcp)
		return;

	//dsp_mem_unprepare_mem_rgn(dhcp->dev, "dhcp_mem");

	devm_kfree(dhcp->dev, dhcp);
}

int dsp_dhcp_fw_time(struct npu_session *session, u32 type)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct dsp_dhcp *dhcp;

	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	dhcp = device->system.dhcp;

	switch (type) {
	case NPU_HWDEV_ID_DSP:
		return dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_FIRMWARE_DSP_TIME);
	case NPU_HWDEV_ID_NPU:
		return dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_FIRMWARE_NPU_TIME);
	default:
		dsp_warn("not supported type(%d)\n", type);
		return -EINVAL;
	}
}
