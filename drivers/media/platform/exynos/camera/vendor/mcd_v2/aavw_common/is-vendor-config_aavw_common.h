#ifndef IS_VENDOR_CONFIG_AAVW_COMMON_H
#define IS_VENDOR_CONFIG_AAVW_COMMON_H

#include <linux/sec_detect.h>

/* Common lines */

#define CAMERA_EEPROM_SUPPORT_FRONT

#define USE_CAMERA_HEAP
#ifdef USE_CAMERA_HEAP
#define CAMERA_HEAP_NAME	"camera"
#define CAMERA_HEAP_NAME_LEN	6
#define CAMERA_HEAP_UNCACHED_NAME	"camera-uncached"
#define CAMERA_HEAP_UNCACHED_NAME_LEN	15
#endif

#ifdef USE_KERNEL_VFS_READ_WRITE
#define DUAL_CAL_DATA_PATH "/vendor/firmware/SetMultiCalInfo.bin"
#else
#define DUAL_CAL_DATA_PATH "/vendor/firmware/"
#define DUAL_CAL_DATA_BIN_NAME "SetMultiCalInfo.bin"
#endif
#define READ_DUAL_CAL_FIRMWARE_DATA // Not for gta4xls

#define DUAL_CAL_DATA_SIZE_DEFAULT (0x080C)

#define CAMERA_STANDARD_CAL_ISP_VERSION 'E'
#define CONFIG_SECURE_CAMERA_USE 1
#define CONFIG_SEC_CAL_ENABLE
#define USES_STANDARD_CAL_RELOAD
#define USE_PERSISTENT_DEVICE_PROPERTIES_FOR_CAL /* For cal reload */
#define VENDER_PATH

#define IS_REAR_MAX_CAL_SIZE (0x4970)
#define IS_FRONT_MAX_CAL_SIZE (0x3310)
#define IS_REAR2_MAX_CAL_SIZE (0x1AA0)
#define IS_REAR3_MAX_CAL_SIZE (0x1CA8)
#define IS_REAR4_MAX_CAL_SIZE (0x1A50)

#define WIDE_OIS_ROM_ID ROM_ID_REAR // Not for gta4xls, m33x

/* Unused */
#define USE_AP_PDAF
#define ENABLE_REMOSAIC_CAPTURE

/* a33x */
#define CAMERA_FRONT_FIXED_FOCUS // Also used by a53x
#define CAMERA_REAR2
#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3
#define CAMERA_REAR3_MODULEID
#define CAMERA_REAR4
#define CAMERA_REAR4_MODULEID
#define CAMERA_REAR_DUAL_CAL // Not gta4xls
#define CAMERA_UWIDE_DUALIZED // Also used by a53x
#define CONFIG_CHECK_HW_VERSION_FOR_MCU_FW_UPLOAD
#define DISABLE_DUAL_SYNC // Also used by m33x, a53x
#define USE_IMX258_13MP_FULL_SIZE // Also used by a25x
#define USE_LEDS_FLASH_CHARGING_VOLTAGE_CONTROL // Also used by a25x
#define USE_OIS_HALL_DATA_FOR_VDIS // Also used by a25x, a53x, m34x

/* a53x */
#define CAMERA_CAL_VERSION_GC5035B 'Q'
#define USE_GW1P_SETFILE
#define USE_HI1336C_SETFILE

/* a25x */
#define APPLY_MIRROR_VERTICAL_FLIP // Also used by m34x
#define FRONT_OTPROM_EEPROM
#define MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB // Also used by m33x, m34x
#define RELAX_OIS_GYRO_OFFSET_SPEC // Also used by m34x
#define SIMPLIFY_OIS_INIT // Also used by m34x
#define USE_CAMERA_ADAPTIVE_MIPI // Also used by m33x, m34x
#define USE_OIS_GYRO_TDK_ICM_42632M // Also used by m34x

/* m33x */
#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING // Also used by gta4xls

#endif /* IS_VENDOR_CONFIG_AAV_V53X_H */
