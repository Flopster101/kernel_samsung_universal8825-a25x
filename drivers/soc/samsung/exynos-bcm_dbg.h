#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of_reserved_mem.h>
#include <soc/samsung/debug-snapshot.h>
#include <linux/sched/clock.h>

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#if defined(CONFIG_EXYNOS_ADV_TRACER) || defined(CONFIG_EXYNOS_ADV_TRACER_MODULE)
#include <soc/samsung/exynos-adv-tracer-ipc.h>
#endif
#include <soc/samsung/exynos-bcm_dbg.h>
#include <soc/samsung/exynos-bcm_dbg-dt.h>
#include <soc/samsung/exynos-bcm_dbg-dump.h>
#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
#include <soc/samsung/exynos-pd.h>
#endif
#include <soc/samsung/cal-if.h>
#if defined(CONFIG_EXYNOS_ITMON) || defined(CONFIG_EXYNOS_ITMON_MODULE)
#include <soc/samsung/exynos-itmon.h>
#endif
#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif

void __exynos_bcm_get_data(struct exynos_bcm_calc* bcm_calc, u64 *freq_stat0, u64 *freq_stat1, u64
		*freq_stat2, u64 *freq_stat3);

void __exynos_bcm_trace_mem_bw(unsigned int bandwidth, unsigned int time);
