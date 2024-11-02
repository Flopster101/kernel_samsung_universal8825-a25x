#ifndef PMU_PRIVATE_H
#define PMU_PRIVATE_H

#ifdef FEEDIRAM_MODE
#define PMU_RAMRP_BASE (0x0000000c)
#else
#define PMU_RAMRP_BASE (0x00000000)
#endif
/** RAMRP usage.
 *
 * RAMRP is divided into four main regions.
 *
 * Region 1: RAMRP Host interface.
 * Start address: RAMRP base (0x60000000)
 * This region provides buffers for communication
 * between the PCIe host and the various subsystems.
 * The size of this region can grow as more functionality
 * is added.
 *
 * Region 2: PMU API
 * Start address: RAMRP base + sizeof(Region 0)
 * This region is used to implement the internal PMU API
 * for sleep management purposes.
 *
 * Region 3: PMU code/data
 * Start address: RAMRP base + sizeof(Region 1)
 * This region contains the rest of the PMU runtime
 * code and data including the vector table.
 *
 * Region 4: Stack
 * Start address: RAMRP end - 1K
 * This region provides the stack space for
 * Cortex M0+ and is placed at end of the RAMRP.
 */

/* RAMRP host interface.
 *
 * These buffers are mapped into one of the PCIe BAR after
 * the second stage boot.
 */

/* Host IF buffer sizes are compile time configurable. */
#ifndef RAMRP_PMU_HOSTIF_BUF_SIZE_WORDS
#define RAMRP_PMU_HOSTIF_BUF_SIZE_WORDS (10)
#endif

#ifndef RAMRP_WPAN_HOSTIF_BUF_SIZE_WORDS
#define RAMRP_WPAN_HOSTIF_BUF_SIZE_WORDS (1)
#endif

#ifndef RAMRP_WLAN_HOSTIF_BUF_SIZE_WORDS
#define RAMRP_WLAN_HOSTIF_BUF_SIZE_WORDS (1)
#endif

/* Mailbox buffers between host and device for request/response
 * exchange.
 */
typedef struct {
	uint32_t from_host;
	uint32_t to_host;
} __attribute__((packed, aligned(4))) ramrp_mbox_t;

/* Host I/F buffers in RAMRP */
typedef struct {
	uint32_t pmu_hif[RAMRP_PMU_HOSTIF_BUF_SIZE_WORDS];
	uint32_t wpan_hif[RAMRP_WPAN_HOSTIF_BUF_SIZE_WORDS];
	uint32_t wlan_hif[RAMRP_WLAN_HOSTIF_BUF_SIZE_WORDS];
} __attribute__((packed, aligned(4))) ramrp_hostif_buf_t;

typedef struct {
	ramrp_mbox_t pmu_mbox;
	ramrp_mbox_t wlan_mbox;
	ramrp_mbox_t wpan_mbox;
	ramrp_hostif_buf_t rhif_buf;
} __attribute__((packed, aligned(4))) ramrp_hostif_t;

#define RAMRP_HOSTIF_SIZE (sizeof(ramrp_hostif_t))
#define RAMRP_HOSTIF_OFFSET(ofs)                                               \
	((PMU_RAMRP_BASE + (offsetof(ramrp_hostif_t, ofs))))

#define RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR                                    \
	RAMRP_HOSTIF_OFFSET(pmu_mbox.from_host)
#define RAMRP_HOSTIF_WLAN_MBOX_FROM_HOST_PTR                                   \
	RAMRP_HOSTIF_OFFSET(wlan_mbox.from_host)
#define RAMRP_HOSTIF_WPAN_MBOX_FROM_HOST_PTR                                   \
	RAMRP_HOSTIF_OFFSET(wpan_mbox.from_host)

#define RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR RAMRP_HOSTIF_OFFSET(pmu_mbox.to_host)
#define RAMRP_HOSTIF_WLAN_MBOX_TO_HOST_PTR                                     \
	RAMRP_HOSTIF_OFFSET(wlan_mbox.to_host)
#define RAMRP_HOSTIF_WPAN_MBOX_TO_HOST_PTR                                     \
	RAMRP_HOSTIF_OFFSET(wpan_mbox.to_host)

#define RAMRP_HOSTIF_PMU_BUF_PTR RAMRP_HOSTIF_OFFSET(rhif_buf.pmu_hif[0])
#define RAMRP_HOSTIF_WPAN_BUF_PTR RAMRP_HOSTIF_OFFSET(rhif_buf.wpan_hif[0])
#define RAMRP_HOSTIF_WLAN_BUF_PTR RAMRP_HOSTIF_OFFSET(rhif_buf.wlan_hif[0])

/* 32 different interrupts can be supported
 * from host to PMU.
 *
 * Add any new PMU interrupts from host here.
 */
typedef enum {
	PMU_INT_FROM_HOST_SS_BOOT_START_MASK = 0x00000001,
	PMU_INT_FROM_HOST_MBOX_REQ_MASK = 0x00000002,
	PMU_INT_FROM_HOST_MAX_MASK
} pmu_int_from_host_mask_t;

/* Second Stage boot status sent back to PMU
 * via rhif.pmu_mbox.to_host
 */
typedef enum {
	PMU_SS_BOOT_STATUS_SUCCESS = 0x0,
	PMU_SS_BOOT_STATUS_FAILURE
} pmu_ss_boot_status_t;

/* Host requests sent in the PMU MBOX
 * along with PMU_INT_FROM_HOST_MBOX_REQ
 */
typedef enum {
	PMU_HOSTIF_MBOX_REQ_NULL = 0x0,
	PMU_HOSTIF_MBOX_REQ_START_WLAN,
	PMU_HOSTIF_MBOX_REQ_START_WPAN,
	PMU_HOSTIF_MBOX_REQ_START_WLAN_WPAN,
	PMU_HOSTIF_MBOX_REQ_RESET_WLAN,
	PMU_HOSTIF_MBOX_REQ_RESET_WPAN,
	PMU_HOSTIF_MBOX_REQ_PCIE_ATU_CONFIG,
	PMU_HOSTIF_MBOX_REQ_MAX
} pmu_hostif_mbox_req_t;

typedef enum {
	PMU_HOSTIF_MBOX_RESPONSE_SUCCESS = 0x0,
	PMU_HOSTIF_MBOX_RESPONSE_FAILURE
} pmu_hostif_mbox_resp_t;

/* PMU API: Internal interface for communication between subsystems
 * and PMU for sleep management purposes.
 */
typedef enum { PMU_NONE = 0, PMU_READY, PMU_START } pmu_ctrl_t;

typedef struct {
	uint8_t pmu_wl_ctrl_req;
	uint8_t pmu_wl_ctrl_param1;
	uint8_t pmu_wl_ctrl_param2;
	uint8_t pmu_wl_ctrl_param3;
	uint32_t pmu_wl_wakeup_time_us;
	uint8_t pmu_wl_status;
	uint8_t pmu_wl_boot_ct;
	uint8_t pmu_wp_ctrl_req;
	uint8_t pmu_wp_ctrl_param1;
	uint8_t pmu_wp_ctrl_param2;
	uint8_t pmu_wp_ctrl_param3;
	uint32_t pmu_wp_wakeup_time_us;
	uint8_t pmu_wp_status;
	uint8_t pmu_wp_boot_ct;
	uint8_t pmu_ctrl_status;
	uint8_t pmu_wake_flags;
	uint8_t pmu_wake_cause0;
	uint8_t pmu_wake_cause1;
	uint32_t pmu_wl_scratch[5];
	uint32_t pmu_wp_scratch[5];
} __attribute__((packed, aligned(4))) pmu_data_t;

#define KRAM(ofs)                                                              \
	(*(volatile uint8_t *)(PMU_RAMRP_BASE + RAMRP_HOSTIF_SIZE +            \
			       (offsetof(pmu_data_t, ofs))))
#define PMU_DATA_PTR ((volatile uint8_t *)(PMU_RAMRP_BASE + RAMRP_HOSTIF_SIZE))

#define PMU_WL_CONTROL_REQUEST KRAM(pmu_wl_ctrl_req)
#define PMU_WL_CONTROL_PARAM1 KRAM(pmu_wl_ctrl_param1)
#define PMU_WL_CONTROL_PARAM2 KRAM(pmu_wl_ctrl_param2)
#define PMU_WL_CONTROL_PARAM3 KRAM(pmu_wl_ctrl_param3)
#define PMU_WL_WAKEUP_TIME_0_7 KRAM(pmu_wl_wakeup_time_0_7)
#define PMU_WL_WAKEUP_TIME_8_15 KRAM(pmu_wl_wakeup_time_8_15)
#define PMU_WL_WAKEUP_TIME_16_23 KRAM(pmu_wl_wakeup_time_16_23)
#define PMU_WL_WAKEUP_TIME_24_31 KRAM(pmu_wl_wakeup_time_24_31)
#define PMU_WL_STATUS KRAM(pmu_wl_status)

#define PMU_WP_CONTROL_REQUEST KRAM(pmu_wp_ctrl_req)
#define PMU_WP_CONTROL_PARAM1 KRAM(pmu_wp_ctrl_param1)
#define PMU_WP_CONTROL_PARAM2 KRAM(pmu_wp_ctrl_param2)
#define PMU_WP_CONTROL_PARAM3 KRAM(pmu_wp_ctrl_param3)
#define PMU_WP_WAKEUP_TIME_0_7 KRAM(pmu_wp_wakeup_time_0_7)
#define PMU_WP_WAKEUP_TIME_8_15 KRAM(pmu_wp_wakeup_time_8_15)
#define PMU_WP_WAKEUP_TIME_16_23 KRAM(pmu_wp_wakeup_time_16_23)
#define PMU_WP_WAKEUP_TIME_24_31 KRAM(pmu_wp_wakeup_time_24_31)
#define PMU_WP_STATUS KRAM(pmu_wp_status)

#define PMU_CONTROL_STATUS KRAM(pmu_ctrl_status)
#define PMU_WAKE_FLAGS KRAM(pmu_wake_flags)
#define PMU_WAKE_CAUSE0 KRAM(pmu_wake_cause0)
#define PMU_WAKE_CAUSE1 KRAM(pmu_wake_cause1)

#define PMU_ENTER_DEEP_SLEEP (0x1)
#define PMU_CANCEL_DEEP_SLEEP (0x2)
#define PMU_PIO_INT_REQ (0x3)
#define PMU_PIO_INT_ACK (0x4)
#define PMU_PING (0x5)
#define PMU_SET_UART_STATIC (0x6)
#define PMU_MIF_REQ (0x7)

/* Wakeup enables */
typedef enum {
	DEEP_SLEEP_WAKEUP_TIMER_MASK = 0x0001,
	DEEP_SLEEP_WAKEUP_MAX_MASK
} deep_sleep_wakeups_t;

typedef struct {
	uint64_t start_addr_0;
	uint64_t end_addr_0;
	uint64_t start_addr_1;
	uint64_t end_addr_1;
} pcie_atu_config_t;
#endif /* PMU_PRIVATE_H */
