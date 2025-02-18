/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef WHDR_H
#define WHDR_H

#define FW_BUILD_ID_SZ 128
#define FW_TTID_SZ 32
/* uses */
#include "fwhdr_if.h"

struct fwhdr_if *whdr_create(void);
void whdr_destroy(struct fwhdr_if *interface);

#define MX_FW_RUNTIME_LENGTH (1024 * 1024)

#endif /* WHDR_H */
