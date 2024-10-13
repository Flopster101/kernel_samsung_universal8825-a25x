
/* Get NCP_VERSION value from platform specific configuration */
#if (CONFIG_NPU_NCP_VERSION == 9)
#include "ncp_header_v9.h"
#elif (CONFIG_NPU_NCP_VERSION == 21)
#include "ncp_header_v21.h"
#elif (CONFIG_NPU_NCP_VERSION == 23)
#include "ncp_header_v23.h"
#elif (CONFIG_NPU_NCP_VERSION == 24)
#include "ncp_header_v24.h"
#elif (CONFIG_NPU_NCP_VERSION == 25)
#include "ncp_header_v25.h"
#else
#error NCP_VERSION is not defined or not supported
#endif
