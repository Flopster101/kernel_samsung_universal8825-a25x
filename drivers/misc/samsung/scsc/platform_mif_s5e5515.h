#ifndef _PLATFORM_MIF_5515_H_
#define _PLATFORM_MIF_5515_H_

#include "mif_reg_S5E5515.h"
#ifdef CONFIG_SCSC_SMAPPER
#include <linux/dma-mapping.h>
#include "mif_reg_smapper.h"
#endif
#include "miframman.h"
#include <scsc/scsc_logring.h>
#include "platform_mif.h"

struct platform_mif {
	struct scsc_mif_abs    interface;
	struct scsc_mbox_s     *mbox;
	struct platform_device *pdev;

	struct device          *dev;

	struct {
		int irq_num;
		int flags;
		atomic_t irq_disabled_cnt;
	} wlbt_irq[PLATFORM_MIF_NUM_IRQS];

	/* MIF registers preserved during suspend */
	struct {
		u32 irq_bit_mask;
		u32 irq_bit_mask_wpan;
	} mif_preserve;

	/* register MBOX memory space */
	void __iomem  *base;
	void __iomem  *base_wpan;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	size_t        mem_size_region2;
	void __iomem  *mem_region2;
#endif
	/* pmu syscon regmap */
	struct regmap *pmureg; /* 0x10860000 */
	struct regmap *i3c_apm_pmic;
	struct regmap *dbus_baaw;
	struct regmap *boot_cfg;
	struct regmap *pbus_baaw;
	struct regmap *wlbt_remap;
	/* Signalled when CFG_REQ IRQ handled */
	struct completion cfg_ack;

	/* State of CFG_REQ handler */
	enum wlbt_boot_state {
		WLBT_BOOT_IN_RESET = 0,
		WLBT_BOOT_WAIT_CFG_REQ,
		WLBT_BOOT_ACK_CFG_REQ,
		WLBT_BOOT_CFG_DONE,
		WLBT_BOOT_CFG_ERROR
	} boot_state;

	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t        mem_size;
	void __iomem  *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void          (*wlan_handler)(int irq, void *data);
	void          (*wpan_handler)(int irq, void *data);
	void          *irq_dev;
	void          *irq_dev_wpan;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	void          (*reset_request_handler)(int irq, void *data);
	void          *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
	/* QoS table */
	struct qos_table *qos;
	bool qos_enabled;
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct work_struct cfgreq_wq;
	struct workqueue_struct *cfgreq_workq;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dma_addr_t paddr;
	int (*platform_mif_set_mem_region2)
		(struct scsc_mif_abs *interface,
		void __iomem *_mem_region2,
		size_t _mem_size_region2);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Callback function to check recovery status */
	bool (*recovery_disabled)(void);
#endif
	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void          (*pmu_handler)(int irq, void *data);
	void          *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;
};

void platform_cfg_req_irq_clean_pending(struct platform_mif *platform);

#endif
