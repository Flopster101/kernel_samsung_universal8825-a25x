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

#ifndef __SHUB_KFIFO_BUF_H__
#define __SHUB_KFIFO_BUF_H__

struct iio_buffer *shub_iio_kfifo_allocate(void);
void shub_iio_kfifo_free(struct iio_buffer *r);

#endif /* __SHUB_KFIFO_BUF_H__ */
