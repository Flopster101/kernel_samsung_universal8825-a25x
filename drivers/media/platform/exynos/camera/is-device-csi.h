#ifndef IS_DEVICE_CSI_H
#define IS_DEVICE_CSI_H

#include <linux/interrupt.h>
#include <media/v4l2-device.h>
#include "is-hw.h"
#include "is-type.h"
#include "is-framemgr.h"
#include "is-subdev-ctrl.h"
#include "is-work.h"
#include "is-device-camif-dma.h"
#include "pablo-device-camif-subblks.h"

#define CSI_IRQ_NAME_LENGTH	16

#define CSI_NOTIFY_VSYNC	10
#define CSI_NOTIFY_VBLANK	11

#define CSI_LINE_RATIO		14	/* 70% */
#define CSI_ERR_COUNT		10  	/* 10frame */

#define CSI_WAIT_ABORT_TIMEOUT	(HZ / 4)

#define CSI_VALID_ENTRY_TO_CH(id) ((id) >= ENTRY_SSVC0 && (id) <= ENTRY_SSVC3)
#define CSI_ENTRY_TO_CH(id) ({BUG_ON(!CSI_VALID_ENTRY_TO_CH(id));id - ENTRY_SSVC0;}) /* range : vc0(0) ~ vc3(3) */
#define CSI_CH_TO_ENTRY(id) (id + ENTRY_SSVC0) /* range : ENTRY_SSVC0 ~ ENTRY_SSVC3 */

/* for use multi buffering */
#define CSI_GET_PREV_FRAMEPTR(frameptr, num_frames, offset) \
	((frameptr) == 0 ? (num_frames) - offset : (frameptr) - offset)
#define CSI_GET_NEXT_FRAMEPTR(frameptr, num_frames) \
	(((frameptr) + 1) % num_frames)

/* For frame id decoder */
#define BUF_SWAP_CNT		2	/* for frame_id decoder & FRO mode: double buffering */

/* for ebuf */
#define EBUF_MANUAL_MODE	0
#define EBUF_AUTO_MODE		1

enum is_csi_state {
	/* csis join ischain */
	CSIS_JOIN_ISCHAIN,
	/* one the fly output */
	CSIS_OTF_WITH_3AA,
	/* If it's dummy, H/W setting can't be applied */
	CSIS_DUMMY,
	/* WDMA flag */
	CSIS_DMA_ENABLE,
	CSIS_DMA_FLUSH_WAIT,
	CSIS_START_STREAM,
	/* runtime buffer done state for error */
	CSIS_BUF_ERR_VC0,
	CSIS_BUF_ERR_VC1,
	CSIS_BUF_ERR_VC2,
	CSIS_BUF_ERR_VC3,
	/* csi vc multibuffer setting state */
	CSIS_SET_MULTIBUF_VC0,
	CSIS_SET_MULTIBUF_VC1,
	CSIS_SET_MULTIBUF_VC2,
	CSIS_SET_MULTIBUF_VC3,
	/* use csi_dma disable on vvalid */
	CSIS_DMA_DISABLING,
	CSIS_LINE_IRQ_ENABLE,
};

struct is_csis_state_cnt {
	u32				err;
	u32				str;
	u32				end;
};

struct is_device_csi {
	u32				instance;	/* logical stream id */
	u32				ch;		/* physical CSI channel */
	u32				device_id;	/* pyysical sensor node id */

	u32 __iomem			*base_reg;
	u32 __iomem			*phy_reg;
	u32 __iomem			*fro_reg;
	resource_size_t			regs_start;
	resource_size_t			regs_end;
	int				irq;
	char				irq_name[CSI_IRQ_NAME_LENGTH];

	struct is_camif_wdma		*wdma;
	int				wdma_ch;
	unsigned int			wdma_ch_hint; /* use designated ch, if it is in DT. */

	struct is_camif_wdma		*stat_wdma;
	int				stat_wdma_ch;

	struct pablo_camif_mcb		*mcb;
	struct pablo_camif_ebuf		*ebuf;
	struct pablo_camif_bns		*bns;

	/* debug */
	struct hw_debug_info		debug_info[DEBUG_FRAME_COUNT];

	/* for settle time */
	struct is_sensor_cfg	*sensor_cfg;

	/* image configuration */
	struct is_image		image;

	unsigned long			state;

	/* for DMA feature */
	struct is_framemgr		*framemgr;
	u32				overflow_cnt;
	u32				sw_checker;
	atomic_t			fcount;
	u32				hw_fcount;
	struct tasklet_struct		tasklet_csis_end;
	struct tasklet_struct		tasklet_csis_line;

	struct workqueue_struct		*workqueue;
	struct work_struct		wq_csis_dma[CSI_VIRTUAL_CH_MAX];
	struct is_work_list		work_list[CSI_VIRTUAL_CH_MAX];

	struct work_struct		wq_ebuf_reset;

	int				pre_dma_enable[CSI_VIRTUAL_CH_MAX];
	int				cur_dma_enable[CSI_VIRTUAL_CH_MAX];

	/* subdev slots for dma */
	struct is_subdev		*dma_subdev[CSI_VIRTUAL_CH_MAX];

	/* pointer address from device sensor */
	struct v4l2_subdev		**subdev;
	struct phy			*phy;
	struct phy_setfile_table	*phy_sf_tbl;

	ulong error_id[CSI_VIRTUAL_CH_MAX];
	u32 error_id_last[CSI_VIRTUAL_CH_MAX];
	u32 error_count;
	u32 error_count_vc_overlap;
	u32 tasklet_csis_end_count;

	atomic_t                        vblank_count; /* Increase at CSI frame end */
	atomic_t			vvalid; /* set 1 while vvalid period */

	char				name[IS_STR_LEN];
	u32				use_cphy;
	bool				potf;
	bool				f_id_dec; /* For frame id decoder in FRO mode */
	atomic_t			bufring_cnt; /* For double buffering in FRO mode */
	u32				dma_batch_num;
	u32				otf_batch_num;
	spinlock_t			dma_seq_slock;
	spinlock_t			dma_irq_slock;
	struct mutex			dma_rta_lock;

	wait_queue_head_t		dma_flush_wait_q;
	bool				crc_flag;

	struct is_csis_state_cnt	state_cnt;
};

void csi_frame_start_inline(struct is_device_csi *csi);
void csi_hw_cdump_all(struct is_device_csi *csi);
void csi_hw_dump_all(struct is_device_csi *csi);

int __must_check is_csi_probe(void *parent, u32 instance, u32 ch, int wdma_ch_hint);
int __must_check is_csi_open(struct v4l2_subdev *subdev, struct is_framemgr *framemgr);
int __must_check is_csi_close(struct v4l2_subdev *subdev);
/* for DMA feature */
extern u32 __iomem *last_fcount0;
extern u32 __iomem *last_fcount1;

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_csi_func {
	void (*csi_s_multibuf_addr)(struct is_device_csi *csi,
		struct is_frame *frame, u32 index, u32 vc);
	void (*wq_csis_dma_vc[4])(struct work_struct *data);
	void (*tasklet_csis_line)(struct tasklet_struct *t);
	void (*csi_hw_cdump_all)(struct is_device_csi *csi);
	void (*csi_hw_dump_all)(struct is_device_csi *csi);
};

struct pablo_kunit_csi_func *pablo_kunit_get_csi_test(void);
#endif

#endif
