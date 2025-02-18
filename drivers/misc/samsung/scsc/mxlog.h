/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _MXLOG_H
#define _MXLOG_H

#include <linux/firmware.h>

#define MX_LOG_PHASE_4		4
#define MX_LOG_PHASE_5		5

#define SYNC_VALUE_PHASE_4	(0xA55A)
#define SYNC_VALUE_PHASE_5	(0x9669)

#define MXLOG_BUFFER_SIZE	512

#define MINIMUM_MXLOG_MSG_LEN_BYTES	(sizeof(u32) * 2)
#define MXLOG_ELEMENT_SIZE		(sizeof(u32))
#define MAX_SPARE_FMT			256
#define TSTAMP_LEN			9
#define MAX_MX_LOG_ARGS			8
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#define MX_LOG_LOGSTRINGS_PATH	"Internal FW" /* in f/w image */
#else
#define MX_LOG_LOGSTRINGS_PATH	"common/log-strings.bin" /* in f/w debug dir */
#endif
#define MXLOG_SEXT(x)	(((x) & 0x80000000) ? ((x) | 0xffffffff00000000) : (x))

#define MXLS_DATA(mx)	((mx)->logstrings->data)
#define MXLS_SZ(mx)	((mx)->logstrings->size)
#define MXLOG_DEFSTR	"<<%s OFFSET OUT OF RANGE. Check log-strings.bin>>"
#define MXLOG_STR_SANE(x, base, size, cast) \
	(((x) < (size)) ? (typeof(cast))((base) + (x)) : (typeof(cast))(MXLOG_DEFSTR))

#if defined (__aarch64__) || defined(__x86_64__)
/**
 * ARM64
 * -----
 * We must process MXLOG messages 32bit-args coming from FW that have
 * a different fmt string interpretation in Kernel:
 *
 *		FW              KERN	MXLOG_CAST
 * ---------------------------------------------------------
 * %d		s32		s32	(s32)
 * %u %x	u32		u32	(u32)
 * %ld		s32		s64	(SIGN_EXT((s64)))
 * %lu		u32		u64	(u64)
 *
 * Additionally we take care to skip any %s using defstr, a char pointer,
 * as def value for the argument; we casted it to u64 (sizeof(char *)) to fool
 * cond expr compilation warnings about types.
 */
#define MXLOG_CAST(x, p, smap, lmap, strmap, base, size)	 \
	(((strmap) & 1 << (p)) ? MXLOG_STR_SANE(x, base, size, u64) : \
	 (((smap) & 1 << (p)) ? \
	  (((lmap) & 1 << (p)) ? MXLOG_SEXT((s64)(x)) : (s32)(x)) : \
	  (((lmap) & 1 << (p)) ? (u64)(x) : (u32)(x))))
#else /* __arm__ */
/**
 * ARM32
 * -----
 * We must process MXLOG messages 32bit-args coming from FW BUT in
 * ARM 32bit iMX6 they should have the same fmt string interpretation:
 *
 *		FW              KERN	MXLOG_CAST
 * ---------------------------------------------------------
 * %d		s32		s32	(s32)
 * %u %x	u32		u32	(u32)
 * %ld		s32		s32	(s32)
 * %lu		u32		u32	(u32)
 *
 * So here we ignore long modifiers and ONLY take care to skip any %s using
 * defstr, a char pointer, as def value for the argument; we casted it to
 * u32 (sizeof(char *) to fool cond expr compilation warnings about types.
 */
#define MXLOG_CAST(x, p, smap, lmap, strmap, base, size)	 \
	(((strmap) & 1 << (p)) ? MXLOG_STR_SANE(x, base, size, u32) : \
	 (((smap) & 1 << (p)) ? ((s32)(x)) : (u32)(x)))
#endif /* __arch64__ */

struct mxlog_event_log_msg {
	u32 timestamp;
	u32 offset;
} __packed;

struct mxlog;

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
void mxlog_init(struct mxlog *mxlog, struct scsc_mx *mx, char *fw_build_id, enum scsc_mif_abs_target target);
void mxlog_load_log_strings(struct mxlog *mxlog, const void *data, size_t len);
void mxlog_unload_log_strings(struct mxlog *mxlog);
#else
void mxlog_init(struct mxlog *mxlog, struct scsc_mx *mx, char *fw_build_id);
#endif
void mxlog_release(struct mxlog *mxlog);

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlog_logstring {
	size_t size;
	const u8 *data;
};
#endif
struct mxlog {
	struct scsc_mx *mx;
	u8             buffer[MXLOG_BUFFER_SIZE];
	u16            index;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	enum scsc_mif_abs_target target;
	struct mxlog_logstring *logstrings;
#else
	struct firmware *logstrings;
#endif
};

#endif /* _MXLOG_H */
