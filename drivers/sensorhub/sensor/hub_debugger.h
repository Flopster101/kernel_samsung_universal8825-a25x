/*
 *  Copyright (C) 2023, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_HUB_DEBUGGER_COMMON_H__
#define __SHUB_HUB_DEBUGGER_COMMON_H__

/*
 * Hub debugger rule
 *
 * version 0 : only log for sensor hub [length:2, data]
 * version 1 : add command [length:2, buffer[cmd:1, data]]
 *    HUB_DEBUGGER_SUBCMD_LOG       : log for sensor hub
 *    HUB_DEBUGGER_SUBCMD_FIFO_DATA : light fifo data for light test app (req. hw)
 */

#define HUB_DUBBGER_SUBCMD_LOG		126
#define HUB_DUBBGER_SUBCMD_FIFO_DATA	127

u8 *get_hub_debugger_fifo_data(void);

#endif /* __SHUB_HUB_DEBUGGER_COMMON_H_ */
