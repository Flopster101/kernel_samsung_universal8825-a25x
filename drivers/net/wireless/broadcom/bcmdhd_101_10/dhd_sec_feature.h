/*
 * Customer HW 4 dependant file
 *
 * Copyright (C) 2021, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_sec_feature.h$
 */

/* XXX This File managed by Samsung */

/*
 * ** Desciption ***
 * 1. Module vs COB
 *    If your model's WIFI HW chip is COB type, you must add below feature
 *    - #undef USE_CID_CHECK
 *    - #define READ_MACADDR
 *    Because COB type chip have not CID and Mac address.
 *    So, you must add below feature to defconfig file.
 *    - CONFIG_WIFI_BROADCOM_COB
 *
 * 2. PROJECTS
 *    If you want add some feature only own Project, you can add it in 'PROJECTS' part.
 *
 * 3. Region code
 *    If you want add some feature only own region model, you can use below code.
 *    - 100 : EUR OPEN
 *    - 101 : EUR ORG
 *    - 200 : KOR OPEN
 *    - 201 : KOR SKT
 *    - 202 : KOR KTT
 *    - 203 : KOR LGT
 *    - 300 : CHN OPEN
 *    - 400 : USA OPEN
 *    - 401 : USA ATT
 *    - 402 : USA TMO
 *    - 403 : USA VZW
 *    - 404 : USA SPR
 *    - 405 : USA USC
 *    You can refer how to using it below this file.
 *    And, you can add more region code, too.
 */

#ifndef _dhd_sec_feature_h_
#define _dhd_sec_feature_h_

#include <linuxver.h>

/* For COB type feature */
#ifdef CONFIG_WIFI_BROADCOM_COB
#undef USE_CID_CHECK
#define READ_MACADDR
#ifdef CONFIG_BCMDHD_PCIE
/* Added multiple chips definition for not to return error while FW DL */
#define SUPPORT_MULTIPLE_CHIPS
#endif /* CONFIG_BCMDHD_PCIE */
#endif  /* CONFIG_WIFI_BROADCOM_COB */

#if defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_ARCH_MSM8994) || \
	defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_SOC_EXYNOS8890)
#define SUPPORT_MULTIPLE_MODULE_CIS
#endif /* CONFIG_MACH_UNIVERSAL7420 || CONFIG_ARCH_MSM8994 ||
	* CONFIG_ARCH_MSM8996 || CONFIG_SOC_EXYNOS8890
	*/

#if defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_SOC_EXYNOS8890)
#define SUPPORT_BCM4359_MIXED_MODULES
#endif /* CONFIG_ARCH_MSM8996 || CONFIG_SOC_EXYNOS8890 */

#if defined(CONFIG_ARGOS)
#if defined(CONFIG_SPLIT_ARGOS_SET)
#define ARGOS_IRQ_WIFI_TABLE_LABEL "WIFI TX"
#define ARGOS_WIFI_TABLE_LABEL "WIFI RX"
#else /* CONFIG_SPLIT_ARGOS_SET */
#define ARGOS_IRQ_WIFI_TABLE_LABEL "WIFI"
#define ARGOS_WIFI_TABLE_LABEL "WIFI"
#endif /* CONFIG_SPLIT_ARGOS_SET */
#define ARGOS_P2P_TABLE_LABEL "P2P"
#endif /* CONFIG_ARGOS */

/* PROJECTS START */

#if defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_SOC_EXYNOS8890) || \
	defined(CONFIG_SOC_EXYNOS8895)
#undef CUSTOM_SET_CPUCORE
#define PRIMARY_CPUCORE 0
#define DPC_CPUCORE 4
#define RXF_CPUCORE 5
#define TASKLET_CPUCORE 5
#define ARGOS_CPU_SCHEDULER
#define ARGOS_RPS_CPU_CTL

#ifdef CONFIG_SOC_EXYNOS8895
#define ARGOS_DPC_TASKLET_CTL
#endif /* CONFIG_SOC_EXYNOS8895 */

#ifdef CONFIG_MACH_UNIVERSAL7420
#define EXYNOS_PCIE_DEBUG
#endif /* CONFIG_MACH_UNIVERSAL7420 */
#endif /* CONFIG_MACH_UNIVERSAL7420  || CONFIG_SOC_EXYNOS8890 || CONFIG_SOC_EXYNOS8895 */

#if defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9820) || \
	defined(CONFIG_SOC_EXYNOS9830) || defined(CONFIG_SOC_EXYNOS2100) || \
	defined(CONFIG_SOC_EXYNOS1000) || defined(CONFIG_SOC_S5E9925)
#define PCIE_IRQ_CPU_CORE 5
#endif /* CONFIG_SOC_EXYNOS9810 || CONFIG_SOC_EXYNOS9820 || defined(CONFIG_SOC_EXYNOS9830 */

#if defined(DHD_LB)
#if defined(CONFIG_ARCH_SM8150) || defined(CONFIG_ARCH_KONA) || \
	defined(CONFIG_ARCH_LAHAINA) || defined(CONFIG_ARCH_WAIPIO)
#define DHD_LB_PRIMARY_CPUS     (0x70)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#elif defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9820) || \
	defined(CONFIG_SOC_EXYNOS9830) || defined(CONFIG_SOC_EXYNOS2100) || \
	defined(CONFIG_SOC_EXYNOS1000) || defined(CONFIG_SOC_S5E9925)
#define DHD_LB_PRIMARY_CPUS     (0x70)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#elif defined(CONFIG_SOC_EXYNOS8890)
/*
 * Removed core 6~7 from NAPI CPU mask.
 * Exynos 8890 disabled core 6~7 by default.
 */
#define DHD_LB_PRIMARY_CPUS     (0x30)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#elif defined(CONFIG_SOC_EXYNOS8895)
/* using whole big core with NAPI mask */
#define DHD_LB_PRIMARY_CPUS     (0xF0)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#elif defined(CONFIG_ARCH_MSM8998)
#define DHD_LB_PRIMARY_CPUS     (0x20)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#elif defined(CONFIG_ARCH_MSM8996)
#define DHD_LB_PRIMARY_CPUS     (0x0C)
#define DHD_LB_SECONDARY_CPUS   (0x03)
#else /* Default LB masks */
/* using whole big core with NAPI mask */
#define DHD_LB_PRIMARY_CPUS     (0xF0)
#define DHD_LB_SECONDARY_CPUS   (0x0E)
#endif /* CONFIG_SOC_EXYNOS8890 */
#else /* !DHD_LB */
#define ARGOS_DPC_TASKLET_CTL
#endif /* !DHD_LB */

#if defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) || \
	defined(CONFIG_SOC_EXYNOS9820)
#if defined(CONFIG_BCMDHD_PCIE)
#define BCMPCIE_DISABLE_ASYNC_SUSPEND
#endif /* CONFIG_BCMDHD_PCIE */
#endif /* under EXYNOS9830 platforms */
/* PROJECTS END */

/* REGION CODE START */

/*
 * remove flags related to region
 * #define GAN_LITE_NAT_KEEPALIVE_FILTER : not used for EUR
 * #define SUPPORT_MULTIPLE_BOARD_REV_FROM_HW : temp used for 43454
 * #undef WRITE_MACADDR: only for Tizen(43013) or SLP
 * #undef USE_INITIAL_2G_SCAN : only for Tizen(43013) or SLP
 * #define DISABLE_HE_ENAB : Beyond P OS for JPN
 */
#if (CONFIG_WLAN_REGION_CODE >= 200) && (CONFIG_WLAN_REGION_CODE < 300) /* KOR */
#ifndef READ_MACADDR
#define READ_MACADDR
#endif /* READ_MACADDR */
#endif /* CONFIG_WLAN_REGION_CODE >= 200 && CONFIG_WLAN_REGION_CODE < 300 */
/* REGION CODE END */

#if !defined(READ_MACADDR) && !defined(WRITE_MACADDR)
#define GET_MAC_FROM_OTP
#define SHOW_NVRAM_TYPE
#endif /* !READ_MACADDR && !WRITE_MACADDR */

#define WRITE_WLANINFO

#endif /* _dhd_sec_feature_h_ */
