/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * Tag-Length-Value encoding of WPAN Firmware File Header (BTHDR)
 */

#ifndef BTHDR_H__
#define BTHDR_H__

/* uses */
#include "fwhdr_if.h"

struct fwhdr_if *bhdr_create(void);
void bhdr_destroy(struct fwhdr_if *interface);

#endif /* BTHDR_H__ */
