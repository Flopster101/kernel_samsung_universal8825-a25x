#ifndef IS_VENDOR_CONFIG_MMW_V34X_H
#define IS_VENDOR_CONFIG_MMW_V34X_H

//#define USE_AP_PDAF						/* Support sensor PDAF SW Solution */

// #define DISABLE_DUAL_SYNC

/***** HW DEFENDANT DEFINE *****/

#define VENDER_PATH

#define USE_CAMERA_HEAP
#ifdef USE_CAMERA_HEAP
#define CAMERA_HEAP_NAME	"camera"
#define CAMERA_HEAP_NAME_LEN	6
#define CAMERA_HEAP_UNCACHED_NAME	"camera-uncached"
#define CAMERA_HEAP_UNCACHED_NAME_LEN	15
#endif

#define CONFIG_SEC_CAL_ENABLE
#define IS_REAR_MAX_CAL_SIZE (0x4970)
#define IS_FRONT_MAX_CAL_SIZE (0x1C86)
#define IS_REAR2_MAX_CAL_SIZE (0x1AA0)
#define IS_REAR4_MAX_CAL_SIZE (0x1A50)

#define REAR_ULTRA_WIDE_CAMERA   (SENSOR_POSITION_REAR3)    /* Supported Camera Type for rear ultra wide */
#define REAR_MACRO_CAMERA        (SENSOR_POSITION_REAR4)    /* Supported Camera Type for rear ultra wide */

#define CAMERA_REAR2             (REAR_ULTRA_WIDE_CAMERA)   /* For Rear2 of SYSFS */
#define CAMERA_REAR3             (REAR_MACRO_CAMERA)        /* For Rear4 of SYSFS */

#define CAMERA_REAR_DUAL_CAL

#define APPLY_MIRROR_VERTICAL_FLIP			/*Flip register need to be set to avoid mirror/rotated preview */
// #define USE_CAMERA_ACT_DRIVER_SOFT_LANDING			/* Using NRC based softlanding */
#define READ_DUAL_CAL_FIRMWARE_DATA

#ifdef USE_KERNEL_VFS_READ_WRITE
#define DUAL_CAL_DATA_PATH "/vendor/firmware/SetMultiCalInfo.bin"
#else
#define DUAL_CAL_DATA_PATH "/vendor/firmware/"
#define DUAL_CAL_DATA_BIN_NAME "SetMultiCalInfo.bin"
#endif

#define DUAL_CAL_DATA_SIZE_DEFAULT (0x080C)

#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3_MODULEID
#define WIDE_OIS_ROM_ID ROM_ID_REAR
#define USE_OIS_GYRO_TDK_ICM_42632M
#define RELAX_OIS_GYRO_OFFSET_SPEC
#define SIMPLIFY_OIS_INIT
#define USE_OIS_HALL_DATA_FOR_VDIS

#define CAMERA_EEPROM_SUPPORT_FRONT
#define CAMERA_STANDARD_CAL_ISP_VERSION 'E'
#define USES_STANDARD_CAL_RELOAD
#define USE_PERSISTENT_DEVICE_PROPERTIES_FOR_CAL /* For cal reload */

#define CONFIG_SECURE_CAMERA_USE 1
#define ENABLE_REMOSAIC_CAPTURE
#define MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB

#define USE_CAMERA_ADAPTIVE_MIPI
#endif /* IS_VENDOR_CONFIG_MMW_V34X_H */
