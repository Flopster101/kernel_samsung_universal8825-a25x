/*
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VTS_H
#define __VTS_H

#include <linux/platform_device.h>

enum vts_dmic_sel {
	DPDM,
	APDM,
};

enum vts_port_pad {
	VTS_PORT_PAD0	= 0,
	VTS_PORT_PAD1	= 1,
	VTS_PORT_PAD2	= 2,
	VTS_PORT_PAD_MAX,
};

enum vts_port_id {
	VTS_PORT_ID_VTS		= 0,
	VTS_PORT_ID_SLIF	= 1,
	VTS_PORT_ID_MAX,
};

enum vts_clk_src {
	VTS_CLK_SRC_RCO			= 0,
	VTS_CLK_SRC_AUD0		= 1,
	VTS_CLK_SRC_AUD1		= 2,
};

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
/**
 * Port Configure
 * @param[in]	dev		pointer to struct VTS device
 * @param[in]	pad		pad id
 * @param[in]	id		port id
 * @param[in]	gpio_func	port func
 * @return		error code if any
 */
extern int vts_port_cfg(struct device *dev,
		enum vts_port_pad pad,
		enum vts_port_id id,
		enum vts_dmic_sel gpio_func,
		bool enable);

/**
 * Set Clock Src
 * @param[in]	dev		pointer to struct VTS device
 * @param[in]	mode		clk mode
 * @return		error code if any
 */
extern int vts_set_clk_src(struct device *dev, enum vts_clk_src mode);

/**
 * Set Clock Mode
 * @param[in]	dev		pointer to struct VTS device
 * @return		error code if any
 */
extern int vts_chk_dmic_clk_mode(struct device *dev);

/**
 * Set aud rate
 * @param[in]	dev		pointer to struct VTS device
 * @param[in]	combination	combination info
 * @return		error code if any
 */
extern int vts_clk_aud_set_rate(struct device *dev,
		unsigned long combination);

/**
 * Set retention
 * @param[in]	retention	on / off
 * @return		error code if any
 */
extern void vts_pad_retention(bool retention);

/**
 * Set retention
 * @param[in]	dev		pointer to struct VTS device
 * @param[in]	enable		on / off
 * @return		error code if any
 */
extern void vts_set_sel_pad(struct device *dev, int enable);

/**
 * Acquire SRAM in VTS
 * @param[in]	pdev	pointer to struct platform_device of a VTS device
 * @param[in]	vts	1, if requester is vts. 0 for others.
 * @return		error code if any
 */
extern int vts_acquire_sram(struct platform_device *pdev, int vts);

/**
 * Release SRAM in VTS
 * @param[in]	pdev	pointer to struct platform_device of a VTS device
 * @param[in]	vts	1, if requester is vts. 0 for others.
 * @return		error code if any
 */
extern int vts_release_sram(struct platform_device *pdev, int vts);

/**
 * Clear SRAM in VTS
 * @param[in]	pdev	pointer to struct platform_device of a VTS device
 * @return		error code if any
 */
extern int vts_clear_sram(struct platform_device *pdev);

/**
 * Check VTS is on
 * @return		true if VTS is on, false on otherwise
 */
extern bool vts_is_on(void);
extern bool vts_is_recognitionrunning(void);

/**
 * Request or release dram during cpuidle (count based API)
 * @param[in]	dev		pointer to abox device
 * @param[in]	id		key which is used as unique handle
 * @param[in]	on		true for requesting, false on otherwise
 */
extern void vts_request_dram_on(struct device *dev, unsigned int id, bool on);
#else /* !CONFIG_SND_SOC_SAMSUNG_VTS */
static inline int vts_acquire_sram(struct platform_device *pdev, int vts)
{ return -ENODEV; }
static inline int vts_release_sram(struct platform_device *pdev, int vts)
{ return -ENODEV; }
static inline int vts_clear_sram(struct platform_device *pdev) { return -ENODEV; }
static inline bool vts_is_on(void) { return false; }
static inline bool vts_is_recognitionrunning(void) { return false; }
static inline void vts_request_dram_on(struct device *dev, unsigned int id, bool on) {}
static inline int vts_port_cfg(struct device *dev,
		enum vts_port_pad pad,
		enum vts_port_id id,
		enum vts_dmic_sel gpio_func,
		bool enable)
		{ return -ENODEV; }
static inline int vts_set_clk_src(struct device *dev,
		enum vts_clk_src mode)
		{ return -ENODEV; }
static inline int vts_chk_dmic_clk_mode(struct device *dev)
		{ return -ENODEV; }
static inline int vts_clk_aud_set_rate(struct device *dev,
		unsigned long combination)
		{ return -ENODEV; }
static inline void vts_pad_retention(bool en) {}
static inline void vts_set_sel_pad(struct device *dev, int enable) {}
#endif /* !CONFIG_SND_SOC_SAMSUNG_VTS */

#endif /* __VTS_H */
