#include <linux/devfreq.h>
#include <linux/kernel.h>

#ifdef CONFIG_DRM_SGPU_EXYNOS

#include "soc/samsung/cal-if.h"

#define TABLE_MAX                      (200)
#define MAX_GOVERNOR_NUM               (4)
#define VSYNC_Q_SIZE                   (16)
#define RENDERINGTIME_MAX_TIME         (999999)
#define RENDERINGTIME_MIN_FRAME        (4000)
#define RTP_DEFAULT_FRAMETIME          (16666)
#define DEADLINE_DECON_INUS            (1000)
#define SYSBUSY_FREQ_THRESHOLD         (605000)
#define SYSBUSY_UTIL_THRESHOLD         (60)

struct amigo_interframe_data {
	unsigned int nrq;
	ktime_t sw_vsync;
	ktime_t sw_start;
	ktime_t sw_end;
	ktime_t sw_total;
	ktime_t hw_vsync;
	ktime_t hw_start;
	ktime_t hw_end;
	ktime_t hw_total;
	ktime_t sum_pre;
	ktime_t sum_cpu;
	ktime_t sum_v2s;
	ktime_t sum_gpu;
	ktime_t sum_v2f;
	ktime_t max_pre;
	ktime_t max_cpu;
	ktime_t max_v2s;
	ktime_t max_gpu;
	ktime_t max_v2f;
	ktime_t vsync_interval;
	int sdp_next_cpuid;
	int sdp_cur_fcpu;
	int sdp_cur_fgpu;
	int sdp_next_fcpu;
	int sdp_next_fgpu;
	ktime_t cputime, gputime;
};

struct amigo_freq_estpower {
	int freq;
	int power;
};

int exynos_dvfs_preset(struct devfreq *df);
int exynos_dvfs_postclear(struct devfreq *df);

void gpu_dvfs_init_utilization_notifier_list(void);
int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb);
int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb);
void gpu_dvfs_notify_utilization(void);

int gpu_dvfs_init_table(struct dvfs_rate_volt *tb, int max_state);
int gpu_dvfs_get_step(void);
int *gpu_dvfs_get_freq_table(void);

int gpu_dvfs_get_clock(int level);
int gpu_dvfs_get_voltage(int clock);

int gpu_dvfs_get_max_freq(void);
int gpu_dvfs_set_max_freq(unsigned long freq);

int gpu_dvfs_get_min_freq(void);
int gpu_dvfs_set_min_freq(unsigned long freq);

int gpu_dvfs_get_cur_clock(void);

unsigned long gpu_dvfs_get_maxlock_freq(void);
int gpu_dvfs_set_maxlock_freq(unsigned long freq);

unsigned long gpu_dvfs_get_minlock_freq(void);
int gpu_dvfs_set_minlock_freq(unsigned long freq);

ktime_t gpu_dvfs_update_time_in_state(unsigned long freq);
ktime_t *gpu_dvfs_get_time_in_state(void);
ktime_t gpu_dvfs_get_tis_last_update(void);

uint64_t *gpu_dvfs_get_job_queue_count(void);
ktime_t gpu_dvfs_get_gpu_queued_last_updated(void);

unsigned int gpu_dvfs_get_polling_interval(void);
int gpu_dvfs_set_polling_interval(unsigned int value);

char *gpu_dvfs_get_governor(void);
int gpu_dvfs_set_governor(char* str_governor);

unsigned int gpu_dvfs_get_weight_table_idx0(void);
int gpu_dvfs_set_weight_table_idx0(unsigned int value);
unsigned int gpu_dvfs_get_weight_table_idx1(void);
int gpu_dvfs_set_weight_table_idx1(unsigned int value);

int gpu_dvfs_get_freq_margin(void);
int gpu_dvfs_set_freq_margin(int freq_margin);

int gpu_dvfs_get_utilization(void);

int gpu_tmu_notifier(struct notifier_block *notifier, unsigned long event, void *v);

int exynos_amigo_interframe_sw_update(ktime_t start, ktime_t end, ktime_t total);
int exynos_amigo_interframe_hw_update(ktime_t start, ktime_t end, bool update);
struct amigo_interframe_data *exynos_amigo_get_next_frameinfo(void);

int exynos_interface_init(struct devfreq *df);
int exynos_interface_deinit(struct devfreq *df);

void gpu_dvfs_set_amigo_governor(int mode);

void gpu_umd_min_clock_set(unsigned int value, unsigned int delay);

#endif
