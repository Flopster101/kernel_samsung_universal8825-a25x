// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "gnss_prj.h"
#include "gnss_link_device_memory.h"

void gnss_msq_reset(struct mem_status_queue *msq)
{
	unsigned long flags;
	spin_lock_irqsave(&msq->lock, flags);
	msq->out = msq->in;
	spin_unlock_irqrestore(&msq->lock, flags);
}

/**
 * gnss_msq_get_free_slot
 * @trq : pointer to an instance of mem_status_queue structure
 *
 * Succeeds always by dropping the oldest slot if a "msq" is full.
 */
struct mem_status *gnss_msq_get_free_slot(struct mem_status_queue *msq)
{
	int qsize = MAX_MEM_LOG_CNT;
	int in;
	int out;
	unsigned long flags;
	struct mem_status *stat;

	spin_lock_irqsave(&msq->lock, flags);

	in = msq->in;
	out = msq->out;

	if (circ_get_space(qsize, in, out) < 1) {
		/* Make the oldest slot empty */
		out++;
		msq->out = (out == qsize) ? 0 : out;
	}

	/* Get a free slot */
	stat = &msq->stat[in];

	/* Make it as "data" slot */
	in++;
	msq->in = (in == qsize) ? 0 : in;

	spin_unlock_irqrestore(&msq->lock, flags);

	return stat;
}

struct mem_status *gnss_msq_get_data_slot(struct mem_status_queue *msq)
{
	int qsize = MAX_MEM_LOG_CNT;
	int in;
	int out;
	unsigned long flags;
	struct mem_status *stat;

	spin_lock_irqsave(&msq->lock, flags);

	in = msq->in;
	out = msq->out;

	if (in == out) {
		stat = NULL;
		goto exit;
	}

	/* Get a data slot */
	stat = &msq->stat[out];

	/* Make it "free" slot */
	out++;
	msq->out = (out == qsize) ? 0 : out;

exit:
	spin_unlock_irqrestore(&msq->lock, flags);
	return stat;
}
