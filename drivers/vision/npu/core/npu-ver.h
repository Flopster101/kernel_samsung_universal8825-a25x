
#ifndef _NPU_VER_H_
#define _NPU_VER_H_

#include "vs4l.h"

#define MAX_FW_VERSION_LEN       7
#define MAX_FW_HASH_LEN       121

int npu_ver_probe(struct npu_device *npu_device);
int npu_ver_release(struct npu_device *npu_device);
int npu_ver_info(struct npu_device *npu_device, struct vs4l_version *version);
int npu_ver_fw_info(struct npu_device *npu_device, struct vs4l_version *version);

#endif
