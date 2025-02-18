/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2015, 2019, 2021
 */

#ifndef EXYNOS_PERF_INTERNAL_H
#define EXYNOS_PERF_TNTERNAL_H __FILE__

#ifndef CONFIG_VENDOR_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_VENDOR_NR_CPUS   1
#endif

#define VENDOR_NR_CPUS	CONFIG_VENDOR_NR_CPUS
#define MAX_CLUSTER	3

struct freq {
	unsigned int freq;
	unsigned int volt;
	unsigned int dyn_power;
	unsigned int sta_power;
};

struct my_cluster {
	/* cluster info */
	int first_cpu;
	int last_cpu;
	int coeff;
	unsigned int cal_id;
	struct cpumask *siblings;

	/* cpufreq */
	struct freq *freqs;
	unsigned int freq_size;
	unsigned int maxfreq;
	unsigned int minfreq;
	unsigned int multi_idx;
};

extern int cl_cnt;
extern struct my_cluster cls[MAX_CLUSTER];

/* monitor */
extern uint cpu_util_avgs[VENDOR_NR_CPUS];

/* emstune */
#define EMSTUNE_NORMAL_MODE 0
#define EMSTUNE_HIGH_MODE 6
#define EMSTUNE_DEFAULT_LEVEL 0

#define for_each_cluster(cluster)			\
	for ((cluster) = 0; (cluster) < cl_cnt; (cluster)++)

extern int xperf_prof_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_core_init(struct kobject *kobj);
extern int xperf_memcpy_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_gmc_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_pago_init(struct platform_device *pdev, struct kobject *kobj);

extern int get_cl_idx(int cpu);
extern int get_f_idx(int cl_idx, int freq);

#endif /* EXYNOS_PERF_TNTERNAL_H */
