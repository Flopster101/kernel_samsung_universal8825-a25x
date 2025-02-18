#ifndef _EXYNOS_GPU_API_H_
#define _EXYNOS_GPU_API_H_

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x)  ((void)(x))
#endif /* CSTD_UNUSED */

#if IS_ENABLED(CONFIG_DRM_SGPU) || IS_ENABLED(CONFIG_MALI_MIDGARD)
extern unsigned long *gpu_dvfs_get_freq_table(void);

extern int gpu_dvfs_get_max_freq(void);
extern int gpu_dvfs_get_min_freq(void);
extern int gpu_dvfs_get_cur_clock(void);
extern unsigned long gpu_dvfs_get_maxlock_freq(void);
extern unsigned long gpu_dvfs_get_minlock_freq(void);

extern int gpu_dvfs_get_step(void);
extern ktime_t *gpu_dvfs_get_time_in_state(void);
extern ktime_t gpu_dvfs_get_tis_last_update(void);

extern uint64_t *gpu_dvfs_get_job_queue_count(void);
extern ktime_t gpu_dvfs_get_job_queue_last_updated(void);

extern void gpu_dvfs_set_amigo_governor(int mode);

extern unsigned int gpu_dvfs_get_weight_table_idx0(void);
extern int gpu_dvfs_set_weight_table_idx0(unsigned int value);
extern unsigned int gpu_dvfs_get_weight_table_idx1(void);
extern int gpu_dvfs_set_weight_table_idx1(unsigned int value);

extern int gpu_dvfs_get_freq_margin(void);
extern int gpu_dvfs_set_freq_margin(int freq_margin);

extern int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb);

extern void exynos_stats_get_run_times(u64 *times);
extern void exynos_stats_set_vsync(ktime_t ktime_us);
extern void exynos_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
extern void exynos_migov_set_targetframetime(int us);
extern void exynos_migov_set_targettime_margin(int us);
extern void exynos_migov_set_util_margin(int percentage);
extern void exynos_migov_set_decon_time(int us);
extern void exynos_migov_set_comb_ctrl(int enable);
extern int exynos_gpu_stc_config_show(int page_size, char *buf);
extern int exynos_gpu_stc_config_store(const char *buf);
#else
static inline unsigned long *gpu_dvfs_get_freq_table(void) { return 0; };

static inline int gpu_dvfs_get_max_freq(void) { return 0; };
static inline int gpu_dvfs_get_min_freq(void) { return 0; };
static inline int gpu_dvfs_get_cur_clock(void) { return 0; };
static inline unsigned long gpu_dvfs_get_maxlock_freq(void) { return 0; };
static inline unsigned long gpu_dvfs_get_minlock_freq(void) { return 0; };

static inline int gpu_dvfs_get_step(void) { return 0; };
static inline ktime_t *gpu_dvfs_get_time_in_state(void) { return 0; };
static inline ktime_t gpu_dvfs_get_tis_last_update(void) { return 0; };

static inline uint64_t *gpu_dvfs_get_job_queue_count(void) { return 0; };
static inline ktime_t gpu_dvfs_get_job_queue_last_updated(void) { return 0; }

static inline void gpu_dvfs_set_amigo_governor(int mode) { CSTD_UNUSED(mode); };

static inline unsigned int gpu_dvfs_get_weight_table_idx0(void) { return 0; };
static inline int gpu_dvfs_set_weight_table_idx0(unsigned int value) { CSTD_UNUSED(value); return 0; };
static inline unsigned int gpu_dvfs_get_weight_table_idx1(void) { return 0; };
static inline int gpu_dvfs_set_weight_table_idx1(unsigned int value) { CSTD_UNUSED(value); return 0; };

static inline int gpu_dvfs_get_freq_margin(void) { return 0; };
static inline int gpu_dvfs_set_freq_margin(int freq_margin) { CSTD_UNUSED(freq_margin); return 0; };

extern void gpu_dvfs_init_utilization_notifier_list(void);
extern int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb)
{
	CSTD_UNUSED(nb);
	return 0;
}

static void exynos_stats_get_run_times(u64 *times) { CSTD_UNUSED(times); };
static void exynos_stats_set_vsync(ktime_t ktime_us) { CSTD_UNUSED(ktime_us); };
static void exynos_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	CSTD_UNUSED(nrframe);
	CSTD_UNUSED(nrvsync);
	CSTD_UNUSED(delta_ms);
};
static void exynos_migov_set_targetframetime(int us) { CSTD_UNUSED(us); };
static void exynos_migov_set_targettime_margin(int us) { CSTD_UNUSED(us); };
static void exynos_migov_set_util_margin(int percentage) { CSTD_UNUSED(percentage); };
static void exynos_migov_set_decon_time(int us) { CSTD_UNUSED(us); };
static void exynos_migov_set_comb_ctrl(int enable) { CSTD_UNUSED(enable); };
static int exynos_gpu_stc_config_show(int page_size, char *buf) { CSTD_UNUSED(page_size); CSTD_UNUSED(buf); return 0; };
static int exynos_gpu_stc_config_store(const char *buf) { CSTD_UNUSED(buf); return 0; };
#endif /* CONFIG_DRM_gpu_dvfs_EXYNOS */
#endif /* _EXYNOS_GPU_API_H_ */
