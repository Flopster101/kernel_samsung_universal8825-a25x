/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "pcie_mbox_shared_data_defs.h"

#define PCI_DEVICE_ID_SAMSUNG_SCSC 0x7011
#define PCI_DEVICE_ID_SAMSUNG_SCSC_PAEAN 0x0a46
#define DRV_NAME "scscPCIe"

/* Max amount of memory allocated by dma_alloc_coherent */
#define PCIE_MIF_PREALLOC_MEM   (16 * 1024 * 1024)
#define PCIE_MIF_ALLOC_MEM (PCIE_MIF_PREALLOC_MEM)
