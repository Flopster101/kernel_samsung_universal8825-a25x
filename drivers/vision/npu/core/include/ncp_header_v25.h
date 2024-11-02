/*
 * ncp.h
 *
 *  Created on: 2017. 7. 18.
 *      Author: kilyeon.im
 */

#ifndef NCP_H_
#define NCP_H_

/* please define these types if you don't have
 * typedef	signed char	s8;
 * typedef	signed short	s16;
 * typedef	signed int	s32;
 * typedef	unsigned char	u8;
 * typedef	unsigned short	u16;
 * typedef	unsigned int	u32;
 * typedef unsigned long	ulong;
 * typedef unsigned long long	u64;
 */

#define NCP_MAGIC1		0x0C0FFEE0
#define NCP_MAGIC2		0xC0DEC0DE
#define NCP_MODEL_NAME_LEN	20
#define NCP_VERSION		25

/**
 * @brief an element of address vector table
 */
struct address_vector {
	/**
	 * @brief index in address vector table
	 * @details producer : compiler
	 * @n consumer : driver, firmware
	 * @n description : this index is required for partial update of address vector(0 ~ n)
	 */
	u32 index;
	/**
	 * @brief master address (KITT side)
	 * @details producer : compiler, driver
	 * @n consumer : driver, firmware
	 * @n description : device virtual address or offset.
	 * this address can point feature map, weight, golden etc.
	 * if offset is provided by compiler in case of weight, feature map and golden,
	 * driver should replace this offset with a device virtual address.
	 * an offset value means offset from start of ncp header.
	 */
	u32 m_addr;
	/**
	 * @brief slave address (TURING side)
	 * @details producer : driver
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 s_addr;
	/**
	 * @brief size in byte
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : io feature map and intermediate feature map should have not zero size
	 */
	u32 size;
};

enum ncp_memory_type {
	MEMORY_TYPE_IN_FMAP, /* input feature map */
	MEMORY_TYPE_OT_FMAP, /* output feature map */
	MEMORY_TYPE_IM_FMAP, /* intermediate feature map */
	MEMORY_TYPE_OT_PIX0,
	MEMORY_TYPE_OT_PIX1,
	MEMORY_TYPE_OT_PIX2,
	MEMORY_TYPE_OT_PIX3,
	MEMORY_TYPE_WEIGHT,
	MEMORY_TYPE_WMASK,
	MEMORY_TYPE_LUT,
	MEMORY_TYPE_NCP,
	MEMORY_TYPE_GOLDEN,
	MEMORY_TYPE_CUCODE,
	MEMORY_TYPE_MAX
};

enum ncp_memory_pixel_format {
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M = 0x105,  /* HAL_PIXEL_FORMAT_YCbCr_420_SP */
	 /* 10-bit format (2 fd, 10bit, 2x byte) custom formats */
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M = 0x127,
	 /* SBWC format */
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC = 0x130,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC  = 0x131,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC = 0x132,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC  = 0x133,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC = 0x134,
	/* SBWC Lossy formats */
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L50 = 0x140,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L75 = 0x141,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L50  = 0x150,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L75  = 0x151,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L40 = 0x160,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L60 = 0x161,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L80 = 0x162,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L40  = 0x170,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L60  = 0x171,
	MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L80  = 0x172
};

/**
 * @brief an element of memory vector table
 */
struct memory_vector {
	/**
	 * @brief memory type
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : feature map, weight, weight mask, lut, ncp body. bias is included to ncp body.
	 */
	u32 type;
	/**
	 * @brief pixsel format
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : required pixel format (FOUR CC)
	 */
	u32 pixel_format;
	/**
	 * @brief pixel width
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 width;
	/**
	 * @brief pixel height
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 height;
	/**
	 * @brief channel count
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 channels;
	/**
	 * @brief width stride
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : this stride value can be input or output stride
	 */
	u32 wstride;
	/**
	 * @brief channel stride
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : this stride value can be input or output stride
	 */
	u32 cstride;
	/**
	 * @brief address vector ind
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : gp register index to be updated (0 ~ 15)
	 */
	u32 gpreg_index;
	/**
	 * @brief address vector index
	 * @details producer : framework
	 * @n consumer : firmware
	 * @n description : index of array of struct address_vector
	 */
	u32 address_vector_index;
};

/**
 * @brief an element of power eatimation vector table
 */
struct pwr_est_vector {
	/**
	 * @brief power estimation vector index
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : index of a power estimation vector array
	 */
	u32 index;
	/**
	 * @brief layer id
	 * @details producer : compiler
	 * @n consumer : compiler
	 * @n description : an internal layer id, which is generated in compiler for tracking
	 */
	s32 layer_id;
	/**
	 * @brief computational quantity
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : the amount of required mac by kilo op unit
	 */
	u32 computational_quantity;
	/**
	 * @brief io transfer size
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : io transfer size by kilo byte unit
	 */
	u32 io_transfer_size;
};

/**
 * @brief an element of interruption vector table
 */
struct interruption_vector {
	/**
	 * @brief index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : 0 <= index <= n
	 */
	u32 index;
	/**
	 * @brief the offset of cmdq isa
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : isa address =  body + isa offset
	 */
	u32 isa_offset;
	/**
	 * @brief the size of cmdq isa
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : the leftover cmdq isa size to be processed
	 */
	u32 isa_size;
	/**
	 * @brief the offset of weight
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : weight address =  body + weight offset
	 */
	u32 weight_offset;
	/**
	 * @brief the size of weight
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : the leftover const size to be processed
	 */
	u32 weight_size;
	/**
	 * @brief the offset of const data
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : const address =  body + const offset
	 */
	u32 const_offset;
	/**
	 * @brief the size of const data size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : the leftover const size to be processed
	 */
	u32 const_size;
};

enum group_type {
	GROUP_TYPE_THREAD,
	GROUP_TYPE_SLOT,
	GROUP_TYPE_DUMMY,
	GROUP_TYPE_END
};

enum group_status {
	GROUP_STATUS_STANDBY, /* not started yet */
	GROUP_STATUS_START, /* right before start */
	GROUP_STATUS_STOP, /* stopped on work */
	GROUP_STATUE_DONE, /* process done */
	GROUP_STATUS_END
};

enum group_flags {
	GROUP_FLAG_SKIP, /* this group should be skipped to process */
	GROUP_FLAG_NOTIFY, /* notify flag look forward to notifying a completion of group process by status update */
	GROUP_FLAG_COMPARE, /* this flag look forward to comparing between output and golden */
	GROUP_FLAG_LAST, /* last flag means last group in chunk it's contained */
	GROUP_FLAG_WFS, /* group have to wait for standby status(WFS) */
	GROUP_FLAG_END
};

/**
 * @brief an element of group vector table
 */
struct group_vector {
	/**
	 * @brief group index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this index should be monotonously increased by one
	 * range : 0 <= index < ncp_header->group_vector_cnt
	 */
	u32 index;
	/**
	 * @brief group id
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this id should be monotonously increased by one
	 * range : 0 <= id < maximum group id in a thread
	 */
	u32 id;
	/**
	 * @brief group type
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : thread, slot
	 */
	u32 type;
	/**
	 * @brief the size of group
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 size;
	/**
	 * @brief the status of group
	 * @details producer : firmware
	 * @n consumer : driver
	 * @n description : this shows progress of group
	 */
	u32 status;
	/**
	 * @brief flags
	 * @details producer : all
	 * @n consumer : firmware
	 * @n description : bit flag can be orred, each bit corresponds to each flag.
	 */
	u32 flags;
	/**
	 * @brief iteration count
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : iteration count is used for repeatation of isa only
	 * if this value is zero then just be done one time.
	 */
	u32 batch_n;
	/**
	 * @brief the offset of intrinsic
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : instrinsic address =  body + intrinsic offset
	 */
	u32 intrinsic_offset;
	/**
	 * @brief the size of intrinsic set
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 intrinsic_size;
	/**
	 * @brief the offset of ISA
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : isa address =  body + isa offset
	 */
	u32 isa_offset;
	/**
	 * @brief the size of command set
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 isa_size;
};

/**
 * @brief an element of thread vector table
 */
struct thread_vector {
	/**
	 * @brief vector index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this index should be monotonously increased by one
	 * range : 0 <= index < ncp_header->thread_vector_cnt
	 */
	u32 index;
	/**
	 * @brief workload
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this value is a propotion(0 ~ 100) that a thread occupies on the total workload
	 */
	u32 workload;
	/**
	 * @brief the number of required dma virtual channels
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this describes the number of required vc being used in a thread
	 */
	u32 required_dma_vc_cnt;
	/**
	 * @brief group start index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this points the start index of group vector
	 * range : 0 <= index < ncp_header-group_vector_cnt
	 */
	u32 group_str_idx;
	/**
	 * @brief group end index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this points the end index of group vector
	 * range : 0 <= index < ncp_header->group_vector_cnt
	 */
	u32 group_end_idx;
	/**
	 * @brief interruption start index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : 0 <= index < ncp_header->interruption_vector_cnt
	 */
	u32 interruption_str_idx;
	/**
	 * @brief rq interruption end index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : 0 <= index < ncp_header->interruption_vector_cnt
	 */
	u32 interruption_end_idx;
};

struct llc_vector {
	/**
	 * @brief cacheable buffer size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : cacheable buffer size(C) in MB
	 */
	u32 cacheable_buffer_size;
	/**
	 * @brief remaining dram io
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : dram io with C cacheable buffer(D)
	 */
	u32 remaining_dram_io;
	/**
	 * @brief remaining dram io ratio
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : % = (D * 100) / total dram io
	 */
	u32 remaining_dram_io_ratio;
};

/**
 * @brief ncp header to describe image structure
 */
struct ncp_header {
	/**
	 * @brief magic number at start
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : for detecting memory invasion
	 */
	u32 magic_number1;
	/**
	 * @brief ncp header version
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : the format of version number is decimal
	 */
	u32 hdr_version;
	/**
	 * @brief ncp header size
	 * @details producer : compiler & driver
	 * @n consumer : all
	 * @n description : total ncp header size in byte, this size includes address, memory, group, chunk vector
	 */
	u32 hdr_size;
	/**
	 * @brief intrinsic version
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : firmware should check intrinsic version before doing ncp
	 */
	u32 intrinsic_version;
	/**
	 * @brief model name
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : model name in a string
	 */
	s8 model_name[NCP_MODEL_NAME_LEN];
	/**
	 * @brief priority
	 * @details producer : framework
	 * @n consumer : firmware
	 * @n description : the process order
	 */
	u32 priority;
	/**
	 * @brief flags
	 * @details producer : framework
	 * @n consumer : all
	 * @n description : debug flag, and son on
	 */
	u32 flags;
	/**
	 * @brief periodic time
	 * @details producer : framework
	 * @n consumer : driver, firmware
	 * @n description : a duty time for done in micro second
	 */
	u32 periodicity;
	/**
	 * @brief workload
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : computational complexity of operation in cycle
	 */
	u32 workload;
	/**
	 * @brief computational workload
	 * @details producer : compiler
	 * @n consumer : driver and firmware
	 * @n description : total computational workload in kilo op
	 */
	u32 computational_workload;
	/**
	 * @brief io workload
	 * @details producer : compiler
	 * @n consumer : driver and firmware
	 * @n description : total sdma transfer size in kilo byte
	 */
	u32 io_workload;
	/**
	 * @brief address vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : address vector offset in byte from header(zero base)
	 */
	u32 address_vector_offset;
	/**
	 * @brief the number of address vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 address_vector_cnt;
	/**
	 * @brief memory vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : memory vector offset in byte from header(zero base)
	 */
	u32 memory_vector_offset;
	/**
	 * @brief the number of memory vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 memory_vector_cnt;
	/**
	 * @brief power estimation vector offset
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : pwrest vector offset in byte from header(zero base)
	 */
	u32 pwrest_vector_offset;
	/**
	 * @brief power estimation vector count
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : the number of power estimation vectors, which is equivalent with the total layer number
	 */
	u32 pwrest_vector_cnt;
	/**
	 * @brief interruption vector offset
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : interruption vector offset in byte from header(zero base)
	 */
	u32 interruption_vector_offset;
	/**
	 * @brief rq vector size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : n
	 */
	u32 interruption_vector_cnt;
	/**
	 * @brief group vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : group vector offset in byte from header(zero base)
	 */
	u32 group_vector_offset;
	/**
	 * @brief the number of group vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 group_vector_cnt;
	/**
	 * @brief thread vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : thread vector offset in byte from header(zero base)
	 */
	u32 thread_vector_offset;
	/**
	 * @brief the number of thread vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 thread_vector_cnt;
	/**
	 * @brief ncp body version
	 * @details producer : compiler
	 * @n consumer : driver, firmware
	 * @n description :
	 */
	u32 body_version;
	/**
	 * @brief ncp body offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : body offset in byte from header(zero base)
	 */
	u32 body_offset;
	/**
	 * @brief ncp body size
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : ncp body size in byte
	 */
	u32 body_size;
	/**
	 * @brief io vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : io vector offset in byte from header(zero base)
	 */
	u32 io_vector_offset;
	/**
	 * @brief the number of io vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 io_vector_cnt;
	/**
	 * @brief rq vector offset
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : rq vector offset in byte from header(zero base)
	 * this offset should be aligned with 64 bytes
	 */
	u32 rq_vector_offset;
	/**
	 * @brief rq vector size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this size can be calculated by the number of io vector * 360 bytes
	 */
	u32 rq_vector_size;
	/**
	 * @brief unique id
	 * @details producer : framework
	 * @n consumer : firmware
	 * @n description : framework assigns an unique op id to each unified op node belonging to a NNC
	 */
	u32 unique_id;
	/**
	 * @brief llc vector offset
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : llc vector offset in byte from header(zero base)
	 */
	u32 llc_vector_offset;
	/**
	 * @brief the total number of llc vectors
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : n
	 */
	u32 llc_vector_cnt;
	/**
	 * @brief reserved field
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : compatibility for the future
	 */
	u32 reserved[5];
	/**
	 * @brief magic number
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : correction for header version
	 */
	u32 magic_number2;
};

#endif /* NCP_H_ */
