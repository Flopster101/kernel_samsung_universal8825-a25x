/*
 * Copyright (c) 2018-2021 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#ifndef _NGKSM_RATE_LIMIT_H
#define _NGKSM_RATE_LIMIT_H

extern int __init ngksm_rate_limit_init(void);
extern int ngksm_check_message_rate_limit(void);

#endif /* _NGKSM_RATE_LIMIT_H */
