/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mem_bw

#if !defined(_TRACE_MEM_BW_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEM_BW_H

#include <linux/tracepoint.h>

TRACE_EVENT(mem_bw,

	TP_PROTO(unsigned int new_bw, unsigned int msec),

	TP_ARGS(new_bw, msec),

	TP_STRUCT__entry(
		__field(unsigned int, new_bw)
		__field(unsigned int, msec)
	),

	TP_fast_assign(
		__entry->new_bw = new_bw;
		__entry->msec = msec;
	),

	TP_printk("membw_MBps=%u time_msec=%u",
			__entry->new_bw, __entry->msec)
);

#endif /* _TRACE_MEM_BW_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

