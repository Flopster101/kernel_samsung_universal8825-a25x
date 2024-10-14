/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/****************************************************************************
 * FILE
 *      bsmhcp.h  -  Bluetooth Shared Memory Host Controller Protocol
 *
 * DESCRIPTION
 *      This file specifies the layout of the Bluetooth Shared Memory
 *      Host Controller Protocol as described in SC-505753-DD
 */

#ifndef __BSMHCP_H__
#define __BSMHCP_H__

#define BSMHCP_VERSION_1                        (0) /* Support ACL and HCI */
#define BSMHCP_VERSION_2                        (1) /* Support ISO */
#define BSMHCP_VERSION                          BSMHCP_VERSION_2

#define BSMHCP_TRANSFER_RING_CMD_SIZE           (8)
#define BSMHCP_TRANSFER_RING_EVT_SIZE           (32)
#define BSMHCP_TRANSFER_RING_ACL_SIZE           (32)
#define BSMHCP_TRANSFER_RING_AVDTP_SIZE         (16)
#define BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE     (8)
#define BSMHCP_TRANSFER_RING_ISO_TX_SIZE        (32)
#define BSMHCP_TRANSFER_RING_ISO_RX_SIZE        (16)

/* Of the buffers in BSMHCP_TRANSFER_RING_ACL_SIZE, reserve a number for ULP
 * operation. */
#define BSMHCP_TRANSFER_RING_ACL_ULP_RESERVED   (10)
#define BSMHCP_TRANSFER_RING_SCO_SIZE           (0)
#define BSMHCP_TRANSFER_RING_TIMING_COUNT       (64)

#define BSMHCP_CMD_EVT_BUFFER_SIZE              (258)
#define BSMHCP_ACL_BUFFER_SIZE                  (1024)
#define BSMHCP_ISO_BUFFER_SIZE                  (512)
/* There can be a maximum of 82 I/Q samples */
#define BSMHCP_IQ_REPORT_BUFFER_SIZE            (2 * 82)
#define BSMHCP_IQ_REPORT_BUFFER_OFFSET          (BSMHCP_ACL_BUFFER_SIZE/2)

#define BSMHCP_ACL_PACKET_SIZE                  (1021)
#define BSMHCP_ULP_PACKET_SIZE                  BSMHCP_ACL_PACKET_SIZE
#define BSMHCP_SCO_PACKET_SIZE                  (0)
#define BSMHCP_ISO_PACKET_SIZE                  (512)
#define BSMHCP_MAX_PACKET_SIZE                  BSMHCP_ACL_PACKET_SIZE

#define BSMHCP_DATA_BUFFER_CMD_SIZE             BSMHCP_TRANSFER_RING_CMD_SIZE
#define BSMHCP_DATA_BUFFER_EVT_SIZE             BSMHCP_TRANSFER_RING_EVT_SIZE
#define BSMHCP_DATA_BUFFER_RX_ACL_SIZE          BSMHCP_TRANSFER_RING_ACL_SIZE

/* In order to ensure that it can be determined that the TX ACL/ISO circular
 * buffers are full or empty the size of the buffers reported to host
 * must be less than the sizeof the transfer ring
 */
#define BSMHCP_DATA_BUFFER_TX_ACL_SIZE          (BSMHCP_TRANSFER_RING_ACL_SIZE - 2)
#define BSMHCP_DATA_BUFFER_TX_ISO_SIZE          (BSMHCP_TRANSFER_RING_ISO_TX_SIZE - 1)
#define BSMHCP_DATA_BUFFER_RX_ISO_SIZE          BSMHCP_TRANSFER_RING_ISO_RX_SIZE

#define BSMHCP_EVENT_TYPE_NONE                  (0x00)
#define BSMHCP_EVENT_TYPE_CONNECTED             (0x01)
#define BSMHCP_EVENT_TYPE_DISCONNECTED          (0x02)
#define BSMHCP_EVENT_TYPE_IQ_REPORT_ENABLED     (0x03)
#define BSMHCP_EVENT_TYPE_IQ_REPORT_DISABLED    (0x04)

#define BSMHCP_ACL_BC_FLAG_BCAST_NON            (0x00)
#define BSMHCP_ACL_BC_FLAG_BCAST_ACTIVE         (0x40)
#define BSMHCP_ACL_BC_FLAG_BCAST_ALL            (0x80)
#define BSMHCP_ACL_BC_FLAG_BCAST_RSVD           (0xC0)
#define BSMHCP_ACL_BC_FLAG_BCAST_MASK           (0xC0)

#define BSMHCP_ACL_PB_FLAG_START_NONFLUSH       (0x00)
#define BSMHCP_ACL_PB_FLAG_CONT                 (0x10)
#define BSMHCP_ACL_PB_FLAG_START_FLUSH          (0x20)
#define BSMHCP_ACL_PB_FLAG_RSVD_3               (0x30)
#define BSMHCP_ACL_PB_FLAG_MASK                 (0x30)

#define BSMHCP_ACL_L2CAP_FLAG_NON               (0x00)
#define BSMHCP_ACL_L2CAP_FLAG_END               (0x01)
#define BSMHCP_ACL_L2CAP_FLAG_MASK              (0x01)

#define BSMHCP_ISO_PB_FLAG_FIRST                (0x00)
#define BSMHCP_ISO_PB_FLAG_CONTINUE             (0x10)
#define BSMHCP_ISO_PB_FLAG_COMPLETE             (0x20)
#define BSMHCP_ISO_PB_FLAG_LAST                 (0x30)
#define BSMHCP_ISO_PB_FLAG_MASK                 (0x30)

#define BSMHCP_ISO_TS_FLAG_TS_NON               (0x00)
#define BSMHCP_ISO_TS_FLAG_TS_INCL              (0x40)
#define BSMHCP_ISO_TS_FLAG_MASK                 (0x40)

#define BSMHCP_ISO_PACKET_STATUS_FLAG_VALID     (0x00)
#define BSMHCP_ISO_PACKET_STATUS_FLAG_INVALID   (0x40)
#define BSMHCP_ISO_PACKET_STATUS_FLAG_LOST      (0x80)
#define BSMHCP_ISO_PACKET_STATUS_FLAG_MASK      (0x80)

#define BSMHCP_SERVICE_BT_STATE_INACTIVE        (0x00)
#define BSMHCP_SERVICE_BT_STATE_ACTIVE          (0x01)

#define BSMHCP_HCI_CONNECTION_HANDLE_LOOPBACK   (0x4000)

#define BSMHCP_ALIGNMENT                        (32)

#define BSMHCP_FEATURE_LPA2DP                   (0x00000001)
#define BSMHCP_FEATURE_RESERVED                 (0x00000002)
#define BSMHCP_FEATURE_FW_INFORMATION           (0x00000004)
#define BSMHCP_FEATURE_AVDTP_TRANSFER_RING      (0x00000008)

#define BSMHCP_CONTROL_START_PANIC              (0x10DEAD01)
#define BSMHCP_CONTROL_STOP_PANIC               (0x0201DEAD)
#define BSMHCP_CONTROL_CONNECTION_PANIC         (0xDEAD2002)

#define BSMHCP_FW_INFO_USER_DEFINED_COUNT       (96)

#define BSMHCP_TIMING_SOURCE_ITIME              (0)
#define BSMHCP_TIMING_SOURCE_ITIME_L1           (1)
#define BSMHCP_TIMING_SOURCE_RADIO_TX           (2)
#define BSMHCP_TIMING_SOURCE_RADIO_RX           (3)
#define BSMHCP_TIMING_SOURCE_RADIO_LC           (4)
#define BSMHCP_TIMING_SOURCE_COUNT              (5)

#define BSMHCP_INCREASE_INDEX(index, limit) \
		((index) = ((index) + 1) % (limit))

#define BSMHCP_PREV_INDEX(index, limit) \
		(((index) == 0) ? ((limit) - 1) : ((index) - 1))

/*
 * For a ring where read == write indicates empty this returns false
 * if adding one more would cause it to see the ring as empty
 *        read = 3 % 5 = 3
 *        v
 * [X,X,_,X,X]
 *      ^
 *      write = 7 % 5 = 2
 */
#define BSMHCP_HAS_ROOM(write, read, limit) \
		((((write) + 1) % (limit)) != (read))

/*
 * Calculate the how many times you could increase the write pointer
 * without causing them to be seen as empty
 *        read = 3 % 5 = 3
 *        v
 * [X,_,_,X,X]
 *    ^
 *    write = 6 % 5 = 1
 *
 * BSMHCP_AMOUNT_FREE(6,3,5) == BSMHCP_AMOUNT_FREE(1,3,5) == 1
 *  You can add one more element before BSMHCP_HAS_ROOM would return false
 */
#define BSMHCP_AMOUNT_FREE(write, read, limit) \
		(((read) - (write + 1)) % (limit))

#define BSMHCP_USED_ENTRIES(write, read, limit) \
		((write) >= (read) ? (write - read) : (limit - read + write))

struct BSMHCP_TD_CONTROL {
	uint16_t length;
	uint8_t  data[BSMHCP_CMD_EVT_BUFFER_SIZE];
};

struct BSMHCP_TD_HCI_EVT {
	uint16_t length;
	uint16_t hci_connection_handle;
	uint16_t event_type;
	uint8_t  data[BSMHCP_CMD_EVT_BUFFER_SIZE];
};

/* Common part for ACL and ISO */
struct BSMHCP_TD_COMMON_TX {
	uint16_t length;                /* Payload length in octets          */
	uint8_t  buffer_index;          /* Index of TX buffer being used     */
	uint8_t  flags;                 /* ACL: PB and BS. ISO PB and TS     */
	uint16_t hci_connection_handle; /* Identifier of the logical channel */
	uint16_t l2cap_cid;             /* Only use by ACL                   */
};

/* Common part for ACL and ISO */
struct BSMHCP_TD_COMMON_RX {
	uint16_t hci_connection_handle; /* Identifier of the logical channel */
	uint16_t length;                /* Payload length in octets          */
	uint8_t  flag;                  /* ACL: BC_flag. ISO: TS_flag        */
	uint8_t  pb_flag;               /* The packet boundary flag          */
	uint8_t  disconnected;          /* true if disconnected              */
	uint8_t  reserved;              /* RFU                               */
};

struct BSMHCP_TD_ACL_RX {
	struct BSMHCP_TD_COMMON_RX header;
	uint8_t  data[BSMHCP_ACL_BUFFER_SIZE];
};

struct BSMHCP_TD_ACL_TX_DATA {
	struct BSMHCP_TD_COMMON_TX header;
};

struct BSMHCP_TD_TX_FREE {
	uint8_t  buffer_index;
	uint8_t  reserved;
	uint16_t hci_connection_handle;
};

#define BSMHCP_TD_ACL_TX_FREE BSMHCP_TD_TX_FREE

struct BSMHCP_TD_AVDTP {
	uint32_t flags;
	uint16_t l2cap_cid;
	uint16_t hci_connection_handle;
	uint32_t reserved;
};

struct BSMHCP_TD_ISO_RX {
	struct BSMHCP_TD_COMMON_RX header;
	/* Note the offset to the data buffer SHALL be equal to the data buffer
	 * offset in BSMHCP_TD_ACL_RX in order to have the same alignment to data
	 */
	uint8_t  data[BSMHCP_ISO_BUFFER_SIZE]; /* Received iso data          */
	/* Note the fields packet_sequence_number, time_stamp, sdu_length and
	 * packet_status_flag are only valid if PB_Flag equals 0b00 or 0b10
	 */
	uint32_t time_stamp;            /* Only valid if TS_Flag equals 0b1  */
	uint16_t packet_sequence_number;/* The sequence number of an ISO_SDU */
	uint16_t sdu_length;            /* Length of ISO_SDU field in octets */
	uint8_t  packet_status_flag;    /* Status of received packet         */
	uint8_t  reserved;              /* RFU                               */
	uint16_t unused;                /* Ensure that data start aligned    */
};

struct BSMHCP_TD_ISO_TX_DATA {
	struct BSMHCP_TD_COMMON_TX header;
	/* Note the fields packet_sequence_number, time_stamp and sdu_length
	 * are only valid if PB_Flag equals 0b00 or 0b10
	 */
	uint16_t packet_sequence_number;/* The sequence number of an ISO_SDU */
	uint32_t time_stamp;            /* Only valid if TS_Flag equals 0b1  */
	uint16_t sdu_length;            /* Length of ISO_SDU field in octets */
	uint8_t  reserved;              /* RFU                               */
	uint8_t  unused;                /* Instead of empty struct padding   */
};

#define BSMHCP_TD_ISO_TX_FREE BSMHCP_TD_TX_FREE

struct BSMHCP_ACL_TR_DRV_INDEX {
	uint32_t read_free;
	uint32_t write_data;
};

struct BSMHCP_ACL_TR_CTRL_INDEX {
	uint32_t read_data;
	uint32_t write_free;
};

struct BSMHCP_INDEX {
	uint32_t read;
	uint32_t write;
};

struct BSMHCP_TIMING_PACKET {
	uint16_t source;
	uint16_t sequence_number;
	uint32_t interrupt_enter;
	uint32_t critical_section_enter;
	uint32_t time[4];
	uint32_t critical_section_leave;
	uint32_t interrupt_leave;
};

struct BSMHCP_FW_INFO {
	uint32_t r4_from_ap_interrupt_count;
	uint32_t m4_from_ap_interrupt_count;
	uint32_t r4_to_ap_interrupt_count;
	uint32_t m4_to_ap_interrupt_count;
	uint32_t bt_deep_sleep_time_total;
	uint32_t bt_deep_sleep_wakeup_duration;
	uint32_t sched_n_messages;
	uint32_t user_defined_count;
	uint32_t user_defined[BSMHCP_FW_INFO_USER_DEFINED_COUNT];
};

struct BSMHCP_TD_IQ_REPORTING_EVT {
	uint8_t  subevent_code;
	uint8_t  packet_status;
	uint16_t connection_handle;
	uint16_t sync_handle;
	uint8_t  rx_phy;
	uint8_t  channel_index;
	int16_t  rssi;
	uint8_t  rssi_antenna_id;
	uint8_t  cte_type;
	uint8_t  slot_durations;
	uint8_t  sample_count;
	uint16_t event_count;
	uint8_t  data[BSMHCP_IQ_REPORT_BUFFER_SIZE];
};

struct BSMHCP_HEADER {
	/* AP RW - M4/R4 RO - 64 octets */
	uint32_t                        magic_value;                /* 0x00 */
	uint16_t                        reserved2_u16;              /* 0x04 */
	uint8_t                         service_request;            /* 0x06 */
	uint8_t                         reserved1;                  /* 0x07 */
	uint32_t                        acl_buffer_size;            /* 0x08 */
	uint32_t                        cmd_evt_buffer_size;        /* 0x0C */
	uint32_t                        acl_tx_buffers;             /* 0x10 */
	uint16_t                        ap_to_bg_int_src;           /* 0x14 */
	uint16_t                        ap_to_fg_int_src;           /* 0x16 */
	uint16_t                        bg_to_ap_int_src;           /* 0x18 */
	uint16_t                        reserved3_u16;              /* 0x1A */
	uint32_t                        mailbox_offset;             /* 0x1C */
	uint32_t                        reserved1_u32;              /* 0x20 */
	uint32_t                        mailbox_hci_cmd_write;      /* 0x24 */
	uint32_t                        mailbox_hci_evt_read;       /* 0x28 */
	uint32_t                        mailbox_acl_tx_write;       /* 0x2C */
	uint32_t                        mailbox_acl_free_read;      /* 0x30 */
	uint32_t                        mailbox_acl_rx_read;        /* 0x34 */
	uint32_t                        abox_offset;                /* 0x38 */
	uint32_t                        abox_length;                /* 0x3C */

	/* AP RO - R4 RW - M4 NA - 32 octets */
	uint16_t                        panic_deathbed_confession;  /* 0x40 */
	uint16_t                        panic_diatribe_value;       /* 0x42 */
	uint32_t                        mailbox_hci_cmd_read;       /* 0x44 */
	uint32_t                        mailbox_hci_evt_write;      /* 0x48 */
	uint32_t                        controller_flags;           /* 0x4C */
	uint32_t                        firmware_features;          /* 0x50 */
	uint16_t                        reserved_u16;               /* 0x54 */
	uint8_t                         service_state;              /* 0x56 */
	uint8_t                         reserved_u8;                /* 0x57 */
	uint32_t                        reserved4_u32;              /* 0x58 */
	uint32_t                        mailbox_avdtp_read;         /* 0x5C */

	/* AP RO - R4 NA - M4 RW - 32 octets */
	uint32_t                        mailbox_acl_tx_read;        /* 0x60 */
	uint32_t                        mailbox_acl_free_write;     /* 0x64 */
	uint32_t                        mailbox_acl_rx_write;       /* 0x68 */
	uint32_t                        mailbox_timing_write;       /* 0x6C */
	uint32_t                        mailbox_iq_report_write;    /* 0x70 */
	uint32_t                        reserved13_u32;             /* 0x74 */
	uint8_t                         unused_1[0x08];             /* 0x78 */


	/* AP RO - R4/M4 RW - 32 octets */
	uint32_t                        reserved6_u32;              /* 0x80 */
	uint8_t                         unused_2[0x1C];             /* 0x84 */

	/* AP RW - M4/R4 RO */
	uint32_t                        mailbox_timing_read;        /* 0xA0 */
	uint32_t                        mailbox_avdtp_write;        /* 0xA4 */
	uint32_t                        mailbox_iq_report_read;     /* 0xA8 */
	uint32_t                        reserved9_u32;              /* 0xAC */
	uint32_t                        reserved10_u32;             /* 0xB0 */
	uint32_t                        reserved11_u32;             /* 0xB4 */
	uint32_t                        reserved12_u32;             /* 0xB8 */
	uint16_t                        info_ap_to_bg_int_src;      /* 0xBC */
	uint16_t                        info_bg_to_ap_int_src;      /* 0xBE */
	uint32_t                        btlog_enables0_low;         /* 0xC0 */
	uint32_t                        firmware_control;           /* 0xC4 */
	uint32_t                        reserved14_u32;             /* 0xC8 */
	uint32_t                        btlog_enables0_high;        /* 0xCC */
	uint32_t                        btlog_enables1_low;         /* 0xD0 */
	uint32_t                        btlog_enables1_high;        /* 0xD4 */
	uint8_t                         bsmhcp_version;             /* 0xD8 */
	uint8_t                         unused_3[0x13];             /* 0xD9 */

	/* Obsolete region - not used */
	uint32_t                        smm_debug_read;             /* 0xEC */
	uint32_t                        smm_debug_write;            /* 0xF0 */
	uint32_t                        smm_exception;              /* 0xF4 */
	uint32_t                        avdtp_detect_stream_id;     /* 0xF8 */
	uint32_t                        smm_int_ready;              /* 0xFE */
};

struct BSMHCP_HEADER_2 {
	/* Offsets are relative to this header BSMHCP_HEADER_2 */
	/* AP RW - FW RO - 128 octets */
	uint32_t                        magic_value;                /* 0x00 */
	uint32_t                        mailbox_iso_tx_write;       /* 0x04 */
	uint32_t                        mailbox_iso_free_read;      /* 0x08 */
	uint32_t                        mailbox_iso_rx_read;        /* 0x0C */
	uint8_t                         unused_1[0x70];             /* 0x10 */

	/* AP RO - FW RW - 128 octets */
	uint32_t                        mailbox_iso_tx_read;        /* 0x80 */
	uint32_t                        mailbox_iso_free_write;     /* 0x84 */
	uint32_t                        mailbox_iso_rx_write;       /* 0x88 */
	uint8_t                         unused_2[0x74];             /* 0x8C */
};

struct BSMHCP_PROTOCOL {
	/* header offset: 0x00000000 */
	volatile struct BSMHCP_HEADER header;

	/* from AP */
	struct BSMHCP_TD_CONTROL /* offset: 0x00000100 */
		hci_cmd_transfer_ring[BSMHCP_TRANSFER_RING_CMD_SIZE];

	/* AVDTP detection */
	struct BSMHCP_TD_AVDTP /* offset: 0x00000920 */
		avdtp_transfer_ring[BSMHCP_TRANSFER_RING_AVDTP_SIZE];

	uint8_t /* offset: 0x000009E0 */
		to_air_unused[0x1FC0];

	struct BSMHCP_TD_ACL_TX_DATA /* offset: 0x000029A0 */
		acl_tx_data_transfer_ring[BSMHCP_TRANSFER_RING_ACL_SIZE];

	uint8_t /* offset: 0x00002AA0 */
		acl_tx_buffer[BSMHCP_DATA_BUFFER_TX_ACL_SIZE]
			     [BSMHCP_ACL_BUFFER_SIZE];

	/* Padding used to ensure minimum 32 octets between sections */
	uint8_t reserved[0x20]; /* offset: 0x0000A2A0 */

	/* to AP */
	struct BSMHCP_TD_HCI_EVT /* offset: 0x0000A2C0 */
		hci_evt_transfer_ring[BSMHCP_TRANSFER_RING_EVT_SIZE];

	struct BSMHCP_TIMING_PACKET /* offset: 0x0000C3C0 */
		timing_transfer_ring[BSMHCP_TRANSFER_RING_TIMING_COUNT];

	struct BSMHCP_FW_INFO /* offset: 0x0000CCC0 */
		information;

	uint8_t /* offset: 0x0000CCC0 + sizoef(struct BSMHCP_FW_INFO) */
		from_air_unused[0x11E0 - sizeof(struct BSMHCP_FW_INFO)];

	struct BSMHCP_TD_IQ_REPORTING_EVT /* offset: 0x0000DEA0 */
		iq_reporting_transfer_ring[BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE];

	struct BSMHCP_TD_ACL_RX /* offset: 0x0000E440 */
		acl_rx_transfer_ring[BSMHCP_TRANSFER_RING_ACL_SIZE];

	struct BSMHCP_TD_ACL_TX_FREE /* offset: 0x00016540 */
		acl_tx_free_transfer_ring[BSMHCP_TRANSFER_RING_ACL_SIZE];

	/* ---=== BSMHCP_VERSION_2 ===--- */

	volatile struct BSMHCP_HEADER_2 header_2; /* offset: 0x000165C0 */

	/* From AP */
	struct BSMHCP_TD_ISO_TX_DATA /* offset: 0x00000100 */
		iso_tx_data_transfer_ring[BSMHCP_TRANSFER_RING_ISO_TX_SIZE];

	uint8_t /* offset: 0x00000300 */

	/* For backward compatibility after HOST-14351 use
	 * BSMHCP_TRANSFER_RING_ISO_TX_SIZE instead of BSMHCP_DATA_BUFFER_TX_ISO_SIZE
	 */
		iso_tx_buffer[BSMHCP_TRANSFER_RING_ISO_TX_SIZE]
			     [BSMHCP_ISO_BUFFER_SIZE];

	/* Padding used to ensure minimum 32 octets between sections */
	uint8_t reserved_2[0x20]; /* offset: 0x00004300 */

	/* To AP */
	struct BSMHCP_TD_ISO_RX /* offset: 0x00004320 */
		iso_rx_transfer_ring[BSMHCP_TRANSFER_RING_ISO_RX_SIZE];

	struct BSMHCP_TD_ISO_TX_FREE /* offset: 0x00006460 */
		iso_tx_free_transfer_ring[BSMHCP_TRANSFER_RING_ISO_TX_SIZE];

	/* Total: 0x000165C0 + 0x000064E0 */
};


#define BSMHCP_TD_ACL_RX_CONTROL_SIZE  \
		(sizeof(struct BSMHCP_TD_ACL_RX) - BSMHCP_ACL_BUFFER_SIZE)

#define BSMHCP_TD_ISO_RX_CONTROL_SIZE  \
		(sizeof(struct BSMHCP_TD_ISO_RX) - BSMHCP_ISO_BUFFER_SIZE)

#define BSMHCP_PROTOCOL_MAGICVALUE \
		((BSMHCP_ACL_BUFFER_SIZE | BSMHCP_CMD_EVT_BUFFER_SIZE | \
		(offsetof(struct BSMHCP_PROTOCOL, acl_tx_buffer) << 15)) ^ \
		offsetof(struct BSMHCP_PROTOCOL, header_2))

#define BSMHCP_PROTOCOL_V2_MAGICVALUE \
		((BSMHCP_PROTOCOL_MAGICVALUE | BSMHCP_ISO_BUFFER_SIZE | \
		(offsetof(struct BSMHCP_PROTOCOL, iso_tx_buffer) << 15)) ^ \
		sizeof(struct BSMHCP_PROTOCOL))

#endif /* __BSMHCP_H__ */
