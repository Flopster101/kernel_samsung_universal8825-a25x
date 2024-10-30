/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SHUB_LIGHT_IR_H__
#define __SHUB_LIGHT_IR_H__

#include <linux/types.h>

struct light_ir_event {
	u32 irData;
	u32 lightRed;
	u32 lightGreen;
	u32 lightBlue;
	u32 lightWhite;
	u16 irAgain;
	u16 irAtime;
} __attribute__((__packed__));


#endif /* __SHUB_LIGHT_IR_H_ */
