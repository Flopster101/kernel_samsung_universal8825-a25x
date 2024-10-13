/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFINTRBIT_H
#define __MIFINTRBIT_H

#include <linux/spinlock.h>

/** MIF Interrupt Bit Handler prototype. */
typedef void (*mifintrbit_handler)(int which_bit, void *data);

struct mifintrbit; /* fwd - opaque pointer */

#if IS_ENABLED(CONFIG_SCSC_PCIE_PAEAN_X86) || IS_ENABLED(CONFIG_SOC_S5E9925)
#define MIFINTRBIT_NUM_INT	32
#else
#define MIFINTRBIT_NUM_INT      16
#endif

/** Reserve MIF interrupt bits 0 in the to-wlan and to-wpan registers for purpose of forcing panics  */
#define MIFINTRBIT_RESERVED_PANIC_R4       0
#define MIFINTRBIT_RESERVED_PANIC_WLAN     0
#define MIFINTRBIT_RESERVED_PANIC_WPAN     0
#define MIFINTRBIT_RESERVED_PANIC_M4       0
#define MIFINTRBIT_RESERVED_PANIC_FXM_1    0
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
#define MIFINTRBIT_RESERVED_PANIC_M4_1     0
#define MIFINTRBIT_RESERVED_PANIC_FXM_2    0
#endif

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
void mifintrbit_init(struct mifintrbit *intr, struct scsc_mif_abs *mif, enum scsc_mif_abs_target target);
void mifintrbit_deinit(struct mifintrbit *intr, enum scsc_mif_abs_target target);
#else
void mifintrbit_init(struct mifintrbit *intr, struct scsc_mif_abs *mif);
void mifintrbit_deinit(struct mifintrbit *intr);
#endif

/** Allocates TOHOST MIF interrupt bits, and associates handler for the AP bit.
 * Returns the bit index.*/
int mifintrbit_alloc_tohost(struct mifintrbit *intr, mifintrbit_handler handler, void *data);
/** Deallocates TOHOST MIF interrupt bits */
int mifintrbit_free_tohost(struct mifintrbit *intr, int which_bit);
/* Get an interrupt bit associated with the target (WLAN/WPAN) -FROMHOST direction
 * Function returns the IRQ bit associated , -EIO if error */
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
int mifintrbit_alloc_fromhost(struct mifintrbit *intr);
#else
int mifintrbit_alloc_fromhost(struct mifintrbit *intr, enum scsc_mif_abs_target target);
#endif
/* Free an interrupt bit associated with the target (WLAN/WPAN) -FROMHOST direction
 * Function returns the 0 if succedes , -EIO if error */
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
int mifintrbit_free_fromhost(struct mifintrbit *intr, int which_bit);
#else
int mifintrbit_free_fromhost(struct mifintrbit *intr, int which_bit, enum scsc_mif_abs_target target);
#endif


struct mifintrbit {
	void(*mifintrbit_irq_handler[MIFINTRBIT_NUM_INT]) (int irq, void *data);
	void                *irq_data[MIFINTRBIT_NUM_INT];
	struct scsc_mif_abs *mif;
	/* Use spinlock is it may be in IRQ context */
	spinlock_t          spinlock;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	/* Interrupt allocation bitmaps */
	DECLARE_BITMAP(bitmap_tohost, MIFINTRBIT_NUM_INT);
	DECLARE_BITMAP(bitmap_fromhost, MIFINTRBIT_NUM_INT);
	enum scsc_mif_abs_target target;
#else
	/* Interrupt allocation bitmaps */
	DECLARE_BITMAP(bitmap_tohost, MIFINTRBIT_NUM_INT);
	DECLARE_BITMAP(bitmap_fromhost_wlan, MIFINTRBIT_NUM_INT);
	DECLARE_BITMAP(bitmap_fromhost_fxm_1, MIFINTRBIT_NUM_INT);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	DECLARE_BITMAP(bitmap_fromhost_fxm_2, MIFINTRBIT_NUM_INT);
#endif
#endif /* CONFIG_SCSC_INDEPENDENT_SUBSYSTEM */
};


#endif
