#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/percpu.h>
#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/cpuhotplug.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/pm_opp.h>

#include <asm/perf_event.h>

#include <trace/hooks/cpuidle.h>
#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-acme.h>
#include <soc/samsung/cal-if.h>

enum user_type {
	SYSFS,
	MIGOV,
	NUM_OF_USER,
};

enum cstate {
	ACTIVE,
	CLK_OFF,
	PWR_OFF,
	NUM_OF_CSTATE,
};

/* Structure for FREQ */
struct freq_table {
	u32	freq;		/* KHz */
	u32	volt;		/* uV */

	/*
	 * Equation for cost
	 * CPU/GPU : Dyn_Coeff/St_Coeff * F(MHz) * V(mV)^2
	 * MIF	   : Dyn_Coeff/St_Coeff * V(mV)^2
	 */
	u64	dyn_cost;
	u64	st_cost;
};

/*
 * It is free-run count
 * NOTICE: MUST guarantee no overflow
 */
struct freq_cstate {
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_snapshot {
	ktime_t	last_snap_time;
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_result {
	ktime_t	profile_time;

	ktime_t	*time[NUM_OF_CSTATE];
	u32	ratio[NUM_OF_CSTATE];
	u32	freq[NUM_OF_CSTATE];

	u64	dyn_power;
	u64	st_power;
};

/* Result during profile time */
struct profile_result {
	struct freq_cstate_result	fc_result;

	s32				cur_temp;
	s32				avg_temp;
};

struct cpu_profiler {
	raw_spinlock_t			lock;
	struct domain_profiler		*dom;

	int				enabled;

	s32				cur_cstate;		/* this cpu cstate */
	ktime_t				last_update_time;

	struct freq_cstate		fc;
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];
	struct profile_result		result[NUM_OF_USER];
};

struct domain_profiler {
	struct list_head		list;
	struct cpumask			cpus;		/* cpus in this domain */
	struct cpumask			online_cpus;	/* online cpus in this domain */

	int				enabled;

	s32				migov_id;
	u32				cal_id;

	/* DVFS Data */
	struct freq_table		*table;
	u32				table_cnt;
	u32				cur_freq_idx;	/* current freq_idx */
	u32				max_freq_idx;	/* current max_freq_idx */
	u32				min_freq_idx;	/* current min_freq_idx */

	/* Power Data */
	u32				dyn_pwr_coeff;
	u32				st_pwr_coeff;

	/* Thermal Data */
	const char			*tz_name;
	struct thermal_zone_device	*tz;

	/* Profile Result */
	struct profile_result		result[NUM_OF_USER];
};

static struct profiler {
	struct device_node		*root;

	struct list_head		list;	/* list for domain */
	struct cpu_profiler __percpu	*cpus;	/* cpu data for all cpus */

	struct work_struct		init_work;
	struct mutex			mode_change_lock;

	struct kobject			*kobj;
} profiler;

static inline u64 *alloc_state_in_freq(int size)
{
	u64 *state_in_state;

	state_in_state = kcalloc(size, sizeof(u64), GFP_KERNEL);
	if (!state_in_state)
		return NULL;

	return state_in_state;
}

static inline
int init_freq_cstate(struct freq_cstate *fc, int depth, int size)
{
	int state;

	for (state = 0; state < depth; state++) {
		ktime_t *time = alloc_state_in_freq(size);

		if (!time)
			return -ENOMEM;
		fc->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}
static inline
int init_freq_cstate_snapshot(struct freq_cstate_snapshot *fc_snap, int depth, int size)
{
	int state;

	for (state = 0; state < depth; state++) {
		ktime_t *time = alloc_state_in_freq(size);

		if (!time)
			return -ENOMEM;
		fc_snap->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}
static inline
int init_freq_cstate_result(struct freq_cstate_result *fc_result, int depth, int size)
{
	int state;

	for (state = 0; state < depth; state++) {
		ktime_t *time = alloc_state_in_freq(size);

		if (!time)
			return -ENOMEM;
		fc_result->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}

static inline
void sync_snap_with_cur(u64 *state_in_freq, u64 *snap, int size)
{
	memcpy(snap, state_in_freq, sizeof(u64) * size);
}

static inline
void make_snap_and_delta(u64 *state_in_freq,
			u64 *snap, u64 *delta, int size)
{
	int idx;

	for (idx = 0; idx < size; idx++) {
		u64 delta_time, cur_snap_time;

		if (!state_in_freq[idx])
			continue;

		/* get current time_in_freq */
		cur_snap_time = state_in_freq[idx];

		/* compute delta between currents snap and previous snap */
		delta_time = cur_snap_time - snap[idx];
		snap[idx] = cur_snap_time;
		delta[idx] = delta_time;
	}
}

static inline
void sync_fcsnap_with_cur(struct freq_cstate *fc,
		struct freq_cstate_snapshot *fc_snap, int size)
{
	int state;

	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc->time[state])
			continue;
		memcpy(fc_snap->time[state],
			fc->time[state], sizeof(ktime_t) * size);
	}
	/* update snapshot time */
	fc_snap->last_snap_time = ktime_get();
}

static inline
void make_snapshot_and_time_delta(struct freq_cstate *fc,
			struct freq_cstate_snapshot *fc_snap,
			struct freq_cstate_result *fc_result,
			int size)
{
	int state, idx;

	/* compute time_delta and make snapshot */
	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc->time[state])
			continue;

		for (idx = 0; idx < size; idx++) {
			u64 delta_time, cur_snap_time;

			/* get current time_in_freq */
			cur_snap_time = fc->time[state][idx];

			/* compute delta between currents snap and previous snap */
			delta_time = cur_snap_time - fc_snap->time[state][idx];
			fc_snap->time[state][idx] = cur_snap_time;
			fc_result->time[state][idx] = delta_time;
		}
	}

	/* save current time */
	fc_result->profile_time = ktime_get() - fc_snap->last_snap_time;
	fc_snap->last_snap_time = ktime_get();
}

#define RATIO_UNIT		1000
#define nsec_to_usec(time)	((time) / 1000)
#define khz_to_mhz(freq)	((freq) / 1000)
#define ST_COST_BASE_TEMP_WEIGHT	(70 * 70)
static inline
void compute_freq_cstate_result(struct freq_table *freq_table,
	struct freq_cstate_result *fc_result, int size, int cur_freq_idx, int temp)
{
	ktime_t total_time;
	u64 freq, ratio;
	u64 st_power = 0, dyn_power = 0;
	ktime_t profile_time = fc_result->profile_time;
	s32 state, idx;
	s32 temp_weight = temp ? (temp * temp) : 1;
	s32 temp_base_weight = temp ? ST_COST_BASE_TEMP_WEIGHT : 1;

	if (unlikely(!fc_result->profile_time))
		return;

	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc_result->time[state])
			continue;

		total_time = freq = ratio = 0;
		for (idx = 0; idx < size; idx++) {
			ktime_t time = fc_result->time[state][idx];

			total_time += time;
			freq += time * khz_to_mhz(freq_table[idx].freq);

			if (state == ACTIVE) {
				st_power += (time * freq_table[idx].st_cost) * temp_weight;
				dyn_power += (time * freq_table[idx].dyn_cost);
			}

			if (state == CLK_OFF)
				st_power += (time * freq_table[idx].st_cost) * temp_weight;
		}

		fc_result->ratio[state] = total_time * RATIO_UNIT / profile_time;
		fc_result->freq[state] = (RATIO_UNIT * freq) / total_time;

		if (!fc_result->freq[state])
			fc_result->freq[state] = freq_table[cur_freq_idx].freq;
	}
	fc_result->dyn_power = dyn_power / profile_time;
	fc_result->st_power = st_power / (profile_time * temp_base_weight);
}

#define RELATION_LOW	0
#define RELATION_HIGH	1
/*
 * Input table should be DECENSING-ORDER
 * RELATION_LOW : return idx of freq lower than or same with input freq
 * RELATOIN_HIGH: return idx of freq higher thant or same with input freq
 */
static inline u32 get_idx_from_freq(struct freq_table *table,
				u32 size, u32 freq, bool flag)
{
	int idx;

	if (flag == RELATION_HIGH) {
		for (idx = size - 1; idx >=  0; idx--) {
			if (freq <= table[idx].freq)
				return idx;
		}
	} else {
		for (idx = 0; idx < size; idx++) {
			if (freq >= table[idx].freq)
				return idx;
		}
	}
	return flag == RELATION_HIGH ? 0 : size - 1;
}

#define STATE_SCALE_CNT			(1 << 0)	/* ramainnig cnt even if freq changed */
#define STATE_SCALE_TIME		(1 << 1)	/* scaling up/down time with changed freq */
#define	STATE_SCALE_WITH_SPARE		(1 << 2)	/* freq boost with spare cap */
#define	STATE_SCALE_WO_SPARE		(1 << 3)	/* freq boost without spare cap */
#define	STATE_SCALE_WITH_ORG_CAP	(1 << 4)	/* freq boost with original capacity */
static inline
void compute_power_freq(struct freq_table *freq_table, u32 size, s32 cur_freq_idx,
		u64 *active_in_freq, u64 *clkoff_in_freq, u64 profile_time,
		s32 temp, u64 *dyn_power_ptr, u64 *st_power_ptr,
		u32 *active_freq_ptr, u32 *active_ratio_ptr)
{
	s32 idx;
	u64 st_power = 0, dyn_power = 0;
	u64 active_freq = 0, total_time = 0;
	s32 temp_weight = temp ? (temp * temp) : 1;
	s32 temp_base_weight = temp ? ST_COST_BASE_TEMP_WEIGHT : 1;

	if (unlikely(!profile_time))
		return;

	for (idx = 0; idx < size; idx++) {
		u64 time;

		if (likely(active_in_freq)) {
			time = active_in_freq[idx];

			dyn_power += (time * freq_table[idx].dyn_cost);
			st_power += (time * freq_table[idx].st_cost) * temp_weight;
			active_freq += time * khz_to_mhz(freq_table[idx].freq);
			total_time += time;
		}

		if (likely(clkoff_in_freq)) {
			time = clkoff_in_freq[idx];
			st_power += (time * freq_table[idx].st_cost) * temp_weight;
		}
	}

	if (active_ratio_ptr)
		*active_ratio_ptr = total_time * RATIO_UNIT / profile_time;

	if (active_freq_ptr) {
		*active_freq_ptr = active_freq * RATIO_UNIT / total_time;
		if (*active_freq_ptr == 0)
			*active_freq_ptr = khz_to_mhz(freq_table[cur_freq_idx].freq);
	}

	if (dyn_power_ptr)
		*dyn_power_ptr = dyn_power / profile_time;

	if (st_power_ptr)
		*st_power_ptr = st_power / (profile_time * temp_base_weight);
}

static inline
unsigned int get_boundary_with_spare(struct freq_table *freq_table,
			s32 max_freq_idx, s32 freq_delta_ratio, s32 cur_idx)
{
	int cur_freq = freq_table[cur_idx].freq;
	int max_freq = freq_table[max_freq_idx].freq;

	return (cur_freq * RATIO_UNIT - max_freq * freq_delta_ratio)
					/ (RATIO_UNIT - freq_delta_ratio);
}

static inline
unsigned int get_boundary_wo_spare(struct freq_table *freq_table,
					s32 freq_delta_ratio, s32 cur_idx)
{
	if (freq_delta_ratio <= 0)
		return freq_table[cur_idx + 1].freq * RATIO_UNIT
				/ (RATIO_UNIT + freq_delta_ratio);

	return freq_table[cur_idx].freq * RATIO_UNIT
				/ (RATIO_UNIT + freq_delta_ratio);
}

static inline
unsigned int get_boundary_freq(struct freq_table *freq_table,
		s32 max_freq_idx, s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	if (freq_delta_ratio <= 0)
		return get_boundary_wo_spare(freq_table, freq_delta_ratio, cur_idx);

	if (flag & STATE_SCALE_WITH_SPARE)
		return get_boundary_with_spare(freq_table, max_freq_idx,
						freq_delta_ratio, cur_idx);

	return get_boundary_wo_spare(freq_table, freq_delta_ratio, cur_idx);

}

static inline
int get_boundary_freq_delta_ratio(struct freq_table *freq_table,
		s32 min_freq_idx, s32 cur_idx, s32 freq_delta_ratio, u32 boundary_freq)
{
	int delta_pct;
	unsigned int lower_freq;
	unsigned int cur_freq = freq_table[cur_idx].freq;

	lower_freq = (cur_idx == min_freq_idx) ? 0 : freq_table[cur_idx + 1].freq;

	if (freq_delta_ratio > 0)
		delta_pct = (cur_freq - boundary_freq) * RATIO_UNIT
						/ (cur_freq - lower_freq);
	else
		delta_pct = (boundary_freq - lower_freq) * RATIO_UNIT
						/ (cur_freq - lower_freq);
	return delta_pct;
}

static inline
unsigned int get_freq_with_spare(struct freq_table *freq_table,
	s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio, s32 flag)
{
	if (flag & STATE_SCALE_WITH_ORG_CAP)
		max_freq_idx = 0;

	return cur_freq + ((freq_table[max_freq_idx].freq - cur_freq)
					* freq_delta_ratio / RATIO_UNIT);
}

static inline
unsigned int get_freq_wo_spare(struct freq_table *freq_table,
		s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio)
{
	return cur_freq + ((cur_freq * freq_delta_ratio) / RATIO_UNIT);
}

static inline
int get_scaled_target_idx(struct freq_table *freq_table,
			s32 min_freq_idx, s32 max_freq_idx,
			s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	int idx, cur_freq, target_freq;
	int target_idx;

	cur_freq = freq_table[cur_idx].freq;

	if (flag & STATE_SCALE_WITH_SPARE) {
		if (freq_delta_ratio > 0)
			target_freq = get_freq_with_spare(freq_table, max_freq_idx,
							cur_freq, freq_delta_ratio, flag);
		else
			target_freq = get_freq_wo_spare(freq_table, max_freq_idx,
							cur_freq, freq_delta_ratio);
	} else {
		target_freq = get_freq_wo_spare(freq_table, max_freq_idx,
						cur_freq, freq_delta_ratio);
	}

	if (target_freq == cur_freq)
		target_idx = cur_idx;

	if (target_freq > cur_freq) {
		for (idx = cur_idx; idx >= max_freq_idx; idx--) {
			if (freq_table[idx].freq >= target_freq) {
				target_idx = idx;
				break;
			}
			target_idx = 0;
		}
	} else {
		for (idx = cur_idx; idx <= min_freq_idx; idx++) {
			if (freq_table[idx].freq < target_freq) {
				target_idx = idx;
				break;
			}
			target_idx = min_freq_idx;
		}
	}

	if (abs(target_idx - cur_idx) > 1)
		target_idx = ((target_idx - cur_idx) > 0) ? cur_idx + 1 : cur_idx - 1;

	return target_idx;
}

static inline
void __scale_state_in_freq(struct freq_table *freq_table,
		s32 min_freq_idx, s32 max_freq_idx,
		u64 *active_state, u64 *clkoff_state,
		s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	int target_freq, cur_freq, boundary_freq;
	int real_freq_delta_ratio, boundary_freq_delta_ratio, target_idx;
	ktime_t boundary_time;

	target_idx = get_scaled_target_idx(freq_table, min_freq_idx, max_freq_idx,
						freq_delta_ratio, flag, cur_idx);
	if (target_idx == cur_idx)
		return;

	cur_freq = freq_table[cur_idx].freq;
	target_freq = freq_table[target_idx].freq;

	boundary_freq = get_boundary_freq(freq_table, max_freq_idx,
				freq_delta_ratio, flag, cur_idx);
	if (freq_delta_ratio > 0) {
		if (boundary_freq <= freq_table[cur_idx + 1].freq)
			boundary_freq = freq_table[cur_idx + 1].freq;
	} else {
		if (boundary_freq > freq_table[cur_idx].freq)
			boundary_freq = freq_table[cur_idx].freq;
	}

	boundary_freq_delta_ratio = get_boundary_freq_delta_ratio(freq_table,
					min_freq_idx, cur_idx, freq_delta_ratio, boundary_freq);
	real_freq_delta_ratio = (target_freq - cur_freq) / (cur_freq / RATIO_UNIT);

	boundary_time = active_state[cur_idx] * boundary_freq_delta_ratio / RATIO_UNIT;

	if (flag & STATE_SCALE_CNT)
		active_state[target_idx] += boundary_time;
	else
		active_state[target_idx] += (boundary_time * RATIO_UNIT)
					/ (RATIO_UNIT + real_freq_delta_ratio);
	active_state[cur_idx] -= boundary_time;
}
static inline
void scale_state_in_freq(struct freq_table *freq_table,
		s32 min_freq_idx, s32 max_freq_idx,
		u64 *active_state, u64 *clkoff_state,
		s32 freq_delta_ratio, s32 flag)
{
	int idx;

	if (freq_delta_ratio > 0)
		for (idx = max_freq_idx + 1; idx <= min_freq_idx; idx++)
			__scale_state_in_freq(freq_table,
				min_freq_idx, max_freq_idx,
				active_state, clkoff_state,
				freq_delta_ratio, flag, idx);
	else
		for (idx = min_freq_idx - 1; idx >= max_freq_idx; idx--)
			__scale_state_in_freq(freq_table,
				min_freq_idx, max_freq_idx,
				active_state, clkoff_state,
				freq_delta_ratio, flag, idx);
}

static inline
void get_power_change(struct freq_table *freq_table, s32 size,
	s32 cur_freq_idx, s32 min_freq_idx, s32 max_freq_idx,
	u64 *active_state, u64 *clkoff_state,
	s32 freq_delta_ratio, u64 profile_time, s32 temp, s32 flag,
	u64 *scaled_dyn_power, u64 *scaled_st_power, u32 *scaled_active_freq)
{
	u64 *scaled_active_state, *scaled_clkoff_state;

	scaled_active_state = kcalloc(size, sizeof(u64), GFP_KERNEL);
	scaled_clkoff_state = kcalloc(size, sizeof(u64), GFP_KERNEL);

	/* copy state-in-freq table */
	memcpy(scaled_active_state, active_state, sizeof(u64) * size);
	memcpy(scaled_clkoff_state, clkoff_state, sizeof(u64) * size);

	/* scaling table with freq_delta_ratio */
	scale_state_in_freq(freq_table, min_freq_idx, max_freq_idx,
			scaled_active_state, scaled_clkoff_state,
			freq_delta_ratio, flag);

	/* computing power with scaled table */
	compute_power_freq(freq_table, size, cur_freq_idx,
		scaled_active_state, scaled_clkoff_state, profile_time, temp,
		scaled_dyn_power, scaled_st_power, scaled_active_freq, NULL);

	kfree(scaled_active_state);
	kfree(scaled_clkoff_state);
}

extern int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size, char *tz_name);

#define PWR_COST_CFVV	0
#define PWR_COST_CVV	1
static inline u64 pwr_cost(u32 freq, u32 volt, u32 coeff, bool flag)
{
	u64 cost = coeff;
	/*
	 * Equation for power cost
	 * PWR_COST_CFVV : Coeff * F(MHz) * V^2
	 * PWR_COST_CVV  : Coeff * V^2
	 */
	volt = volt >> 10;
	cost = ((cost * volt * volt) >> 20);
	if (flag == PWR_COST_CFVV)
		return (cost * (freq >> 10)) >> 10;

	return cost;
}

static inline
int init_static_cost(struct freq_table *freq_table, int size, int weight,
			struct device_node *np, struct thermal_zone_device *tz)
{
	int volt_size, temp_size;
	int *table = NULL;
	int freq_idx, idx, ret = 0;

	ret = exynos_build_static_power_table(np, &table, &volt_size, &temp_size, tz->type);
	if (ret) {
		int cal_id, ratio, asv_group, static_coeff;
		int ratio_table[9] = { 5, 6, 8, 11, 15, 20, 27, 36, 48 };

		pr_info("%s: there is no static power table for %s\n", tz->type);

		// Make static power table from coeff and ids
		if (of_property_read_u32(np, "cal-id", &cal_id)) {
			if (of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id)) {
				pr_err("%s: Failed to get cal-id\n", __func__);
				return -EINVAL;
			}
		}

		if (of_property_read_u32(np, "static-power-coefficient", &static_coeff)) {
			pr_err("%s: Failed to get staic_coeff\n", __func__);
			return -EINVAL;
		}

		ratio = cal_asv_get_ids_info(cal_id);
		asv_group = cal_asv_get_grp(cal_id);


		if (asv_group < 0 || asv_group > 8)
			asv_group = 5;

		if (!ratio)
			ratio = ratio_table[asv_group];

		static_coeff *= ratio;

		pr_info("%s static power table through ids (weight=%d)\n", tz->type, weight);
		for (idx = 0; idx < size; idx++) {
			freq_table[idx].st_cost = pwr_cost(freq_table[idx].freq,
					freq_table[idx].volt, static_coeff, PWR_COST_CVV) / weight;
			pr_info("freq_idx=%2d freq=%d, volt=%d cost=%8d\n",
					idx, freq_table[idx].freq, freq_table[idx].volt, freq_table[idx].st_cost);
		}

		return 0;
	}

	pr_info("%s static power table from DTM (weight=%d)\n", tz->type, weight);
	for (freq_idx = 0; freq_idx < size; freq_idx++) {
		int freq_table_volt = freq_table[freq_idx].volt / 1000;

		for (idx = 1; idx <= volt_size; idx++) {
			int cost;
			int dtm_volt = *(table + ((temp_size + 1) * idx));

			if (dtm_volt < freq_table_volt)
				continue;

			cost = *(table + ((temp_size + 1) * idx + 1));
			cost = cost / weight;
			freq_table[freq_idx].st_cost = cost;
			pr_info("freq_idx=%2d freq=%d, volt=%d dtm_volt=%d, cost=%8d\n",
					freq_idx, freq_table[freq_idx].freq, freq_table_volt,
					dtm_volt, cost);
			break;
		}
	}

	kfree(table);

	return 0;
}

static inline s32 get_temp(struct thermal_zone_device *tz)
{
	s32 temp;

	thermal_zone_get_temp(tz, &temp);
	return temp / 1000;
}

/************************************************************************
 *				HELPER					*
 ************************************************************************/
static struct domain_profiler *get_dom_by_migov_id(int id)
{
	struct domain_profiler *dompro;

	list_for_each_entry(dompro, &profiler.list, list)
		if (dompro->migov_id == id)
			return dompro;
	return NULL;
}

static struct domain_profiler *get_dom_by_cpu(int cpu)
{
	struct domain_profiler *dompro;

	list_for_each_entry(dompro, &profiler.list, list)
		if (cpumask_test_cpu(cpu, &dompro->cpus))
			return dompro;
	return NULL;
}

/************************************************************************
 *				SUPPORT-MIGOV				*
 ************************************************************************/
u32 cpupro_get_table_cnt(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table_cnt;
}

u32 cpupro_get_freq_table(s32 id, u32 *table)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	int idx;
	for (idx = 0; idx < dompro->table_cnt; idx++)
		table[idx] = dompro->table[idx].freq;
	return idx;
}

u32 cpupro_get_max_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table[dompro->max_freq_idx].freq;
}

u32 cpupro_get_min_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table[dompro->min_freq_idx].freq;
}

u32 cpupro_get_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->result[MIGOV].fc_result.freq[ACTIVE];
}

void cpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	*dyn_power = dompro->result[MIGOV].fc_result.dyn_power;
	*st_power = dompro->result[MIGOV].fc_result.st_power;
}
void cpupro_get_power_change(s32 id, s32 freq_delta_ratio,
		u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	struct profile_result *result = &dompro->result[MIGOV];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int cpus_cnt = cpumask_weight(&dompro->online_cpus);
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_TIME);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	get_power_change(dompro->table, dompro->table_cnt,
			dompro->cur_freq_idx, dompro->min_freq_idx, dompro->max_freq_idx,
			fc_result->time[ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
			fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	*dyn_power = (*dyn_power) * cpus_cnt;
	*st_power = (*st_power) * cpus_cnt;
}

u32 cpupro_get_active_pct(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->result[MIGOV].fc_result.ratio[ACTIVE];
}
s32 cpupro_get_temp(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	if (unlikely(!dompro))
		return 0;
	return dompro->result[MIGOV].avg_temp;
}

void cpupro_set_margin(s32 id, s32 margin)
{
	return;
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled);
void cpupro_update_profile(struct domain_profiler *dompro, int user);
void exynos_sdp_set_busy_domain(int id);
u32 cpupro_update_mode(s32 id, int mode)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);

	if (dompro->enabled != mode) {
		cpupro_set_dom_profile(dompro, mode);
		return 0;
	}

	cpupro_update_profile(dompro, MIGOV);

	/* update busy domain between CL1 and CL2 */
	if (id == MIGOV_CL2) {
		struct freq_cstate_result *res;
		int cl1_ratio, cl2_ratio, busy_id;

		dompro = get_dom_by_migov_id(MIGOV_CL1);
		res = &dompro->result[MIGOV].fc_result;
		cl1_ratio = res->ratio[ACTIVE] * cpumask_weight(&dompro->cpus);

		dompro = get_dom_by_migov_id(MIGOV_CL2);
		res = &dompro->result[MIGOV].fc_result;
		cl2_ratio = res->ratio[ACTIVE] * cpumask_weight(&dompro->cpus);

		busy_id = cl1_ratio > cl2_ratio ? MIGOV_CL1 : MIGOV_CL2;
		exynos_sdp_set_busy_domain(busy_id);
	}

	return 0;
}

s32 cpupro_cpu_active_pct(s32 id, s32 *cpu_active_pct)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	int cpu, cnt = 0;

	for_each_cpu(cpu, &dompro->cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate_result *fc_result = &cpupro->result[MIGOV].fc_result;
		cpu_active_pct[cnt++] = fc_result->ratio[ACTIVE];
	}

	return 0;
};

s32 cpupro_cpu_asv_ids(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);

	return cal_asv_get_ids_info(dompro->cal_id);
}

struct private_fn_cpu cpu_pd_fn = {
	.cpu_active_pct		= &cpupro_cpu_active_pct,
	.cpu_asv_ids		= &cpupro_cpu_asv_ids,
};

struct domain_fn cpu_fn = {
	.get_table_cnt		= &cpupro_get_table_cnt,
	.get_freq_table		= &cpupro_get_freq_table,
	.get_max_freq		= &cpupro_get_max_freq,
	.get_min_freq		= &cpupro_get_min_freq,
	.get_freq		= &cpupro_get_freq,
	.get_power		= &cpupro_get_power,
	.get_power_change	= &cpupro_get_power_change,
	.get_active_pct		= &cpupro_get_active_pct,
	.get_temp		= &cpupro_get_temp,
	.set_margin		= &cpupro_set_margin,
	.update_mode		= &cpupro_update_mode,
};

/************************************************************************
 *			Gathering CPUFreq Information			*
 ************************************************************************/
static void cpupro_update_time_in_freq(int cpu, int new_cstate, int new_freq_idx)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct freq_cstate *fc = &cpupro->fc;
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	ktime_t cur_time, diff;
	unsigned long flag;

	if (!dompro)
		return;

	raw_spin_lock_irqsave(&cpupro->lock, flag);
	if (unlikely(!cpupro->enabled)) {
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);
		return;
	}

	cur_time = ktime_get();

	diff = ktime_sub(cur_time, cpupro->last_update_time);
	fc->time[cpupro->cur_cstate][dompro->cur_freq_idx] += diff;

	if (new_cstate > -1)
		cpupro->cur_cstate = new_cstate;

	if (new_freq_idx > -1)
		dompro->cur_freq_idx = new_freq_idx;

	cpupro->last_update_time = cur_time;

	raw_spin_unlock_irqrestore(&cpupro->lock, flag);
}

void cpupro_make_domain_freq_cstate_result(struct domain_profiler *dompro, int user)
{
	struct profile_result *result = &dompro->result[user];
	struct freq_cstate_result *fc_result = &dompro->result[user].fc_result;
	int cpu, state, cpus_cnt = cpumask_weight(&dompro->online_cpus);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	/* clear previous result */
	fc_result->profile_time = 0;
	for (state = 0; state < NUM_OF_CSTATE; state++)
		memset(fc_result->time[state], 0, sizeof(ktime_t) * dompro->table_cnt);

	/* gathering cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate_result *cpu_fc_result = &cpupro->result[user].fc_result;
		int idx;

		for (state = 0; state < NUM_OF_CSTATE; state++) {
			if (!cpu_fc_result->time[state])
				continue;

			for (idx = 0; idx < dompro->table_cnt; idx++)
				fc_result->time[state][idx] +=
					(cpu_fc_result->time[state][idx] / cpus_cnt);
		}
		fc_result->profile_time += (cpu_fc_result->profile_time / cpus_cnt);
	}

	/* compute power/freq/active_ratio from time_in_freq */
	compute_freq_cstate_result(dompro->table, fc_result,
			dompro->table_cnt, dompro->cur_freq_idx, result->avg_temp);

	/* should multiply cpu_cnt to power when computing domain power */
	fc_result->dyn_power = fc_result->dyn_power * cpus_cnt;
	fc_result->st_power = fc_result->st_power * cpus_cnt;
}

static void reset_cpu_data(struct domain_profiler *dompro, struct cpu_profiler *cpupro, int user)
{
	struct freq_cstate *fc = &cpupro->fc;
	struct freq_cstate_snapshot *fc_snap = &cpupro->fc_snap[user];
	int table_cnt = dompro->table_cnt;

	cpupro->cur_cstate = ACTIVE;
	cpupro->last_update_time = ktime_get();

	sync_fcsnap_with_cur(fc, fc_snap, table_cnt);
}

static int init_work_cpu;
static void cpupro_init_work(struct work_struct *work)
{
	int cpu = smp_processor_id();
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	unsigned long flag;

	if (!dompro)
		return;

	if (init_work_cpu != cpu)
		pr_warn("WARNING: init_work is running on wrong cpu(%d->%d)",
				init_work_cpu, cpu);

	/* Enable profiler */
	if (!dompro->enabled) {
		/* update cstate for running cpus */
		raw_spin_lock_irqsave(&cpupro->lock, flag);
		reset_cpu_data(dompro, cpupro, MIGOV);
		cpupro->enabled = true;
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);

		cpupro_update_time_in_freq(cpu, -1, -1);
	} else {
		/* Disable profiler */
		raw_spin_lock_irqsave(&cpupro->lock, flag);
		cpupro->enabled = false;
		cpupro->cur_cstate = PWR_OFF;
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);
	}
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled)
{
	struct cpufreq_policy *policy;
	int cpu;

	mutex_lock(&profiler.mode_change_lock);
	/* this domain is disabled */
	if (!cpumask_weight(&dompro->online_cpus))
		goto unlock;

	policy = cpufreq_cpu_get(cpumask_first(&dompro->online_cpus));
	if (!policy)
		goto unlock;

	/* update current freq idx */
	dompro->cur_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->cur, RELATION_LOW);
	cpufreq_cpu_put(policy);

	/* reset cpus of this domain */
	for_each_cpu(cpu, &dompro->online_cpus) {
		init_work_cpu = cpu;
		schedule_work_on(cpu, &profiler.init_work);
		flush_work(&profiler.init_work);
	}
	dompro->enabled = enabled;
unlock:
	mutex_unlock(&profiler.mode_change_lock);
}

void cpupro_update_profile(struct domain_profiler *dompro, int user)
{
	struct profile_result *result = &dompro->result[user];
	struct cpufreq_policy *policy;
	int cpu;

	/* this domain profile is disabled */
	if (unlikely(!dompro->enabled))
		return;

	/* there is no online cores in this domain  */
	if (!cpumask_weight(&dompro->online_cpus))
		return;

	/* update this domain temperature */
	if (dompro->tz) {
		int temp = get_temp(dompro->tz);
		result->avg_temp = (temp + result->cur_temp) >> 1;
		result->cur_temp = temp;
	}

	/* update cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate *fc = &cpupro->fc;
		struct freq_cstate_snapshot *fc_snap = &cpupro->fc_snap[user];
		struct freq_cstate_result *fc_result = &cpupro->result[user].fc_result;

		cpupro_update_time_in_freq(cpu, -1, -1);

		make_snapshot_and_time_delta(fc, fc_snap, fc_result, dompro->table_cnt);

		if (!fc_result->profile_time)
			continue;

		compute_freq_cstate_result(dompro->table, fc_result,
				dompro->table_cnt, dompro->cur_freq_idx, result->avg_temp);
	}

	/* update domain */
	cpupro_make_domain_freq_cstate_result(dompro, user);

	/* update max freq */
	policy = cpufreq_cpu_get(cpumask_first(&dompro->online_cpus));
	if (!policy)
		return;

	dompro->max_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->max, RELATION_LOW);
	dompro->min_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->min, RELATION_HIGH);
	cpufreq_cpu_put(policy);
}

void exynos_sdp_set_cur_freqlv(int id, int level);
static int cpupro_cpufreq_callback(struct notifier_block *nb,
		unsigned long flag, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct domain_profiler *dompro = get_dom_by_cpu(freq->policy->cpu);
	int cpu, new_freq_idx;

	if (!dompro)
		return NOTIFY_OK;

	if (!dompro->enabled)
		return NOTIFY_OK;

	if (flag != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	if (freq->policy->cpu != cpumask_first(&dompro->online_cpus))
		return NOTIFY_OK;

	/* update current freq */
	new_freq_idx = get_idx_from_freq(dompro->table,
			dompro->table_cnt, freq->new, RELATION_HIGH);

	if (dompro->migov_id == MIGOV_CL1 || dompro->migov_id == MIGOV_CL2)
		exynos_sdp_set_cur_freqlv(dompro->migov_id, new_freq_idx);

	for_each_cpu(cpu, &dompro->online_cpus)
		cpupro_update_time_in_freq(cpu, -1, new_freq_idx);

	return NOTIFY_OK;
}

static struct notifier_block cpupro_cpufreq_notifier = {
	.notifier_call  = cpupro_cpufreq_callback,
};

static int cpupro_pm_update(int flag)
{
	int cpu = raw_smp_processor_id();
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);

	if (!cpupro->enabled)
		return NOTIFY_OK;
	cpupro_update_time_in_freq(cpu, flag, -1);
	return NOTIFY_OK;
}

static void android_vh_cpupro_idle_enter(void *data, int *state,
		struct cpuidle_device *dev)
{
	int flag = (*state) ? PWR_OFF : CLK_OFF;
	cpupro_pm_update(flag);
}
static void android_vh_cpupro_idle_exit(void *data, int state,
		struct cpuidle_device *dev)
{
	cpupro_pm_update(ACTIVE);
}

static int cpupro_cpupm_online(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);

	if (dompro)
		cpumask_set_cpu(cpu, &dompro->online_cpus);

	return 0;
}

static int cpupro_cpupm_offline(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	unsigned long flag;

	if (!dompro)
		return 0;

	raw_spin_lock_irqsave(&cpupro->lock, flag);

	cpumask_clear_cpu(cpu, &dompro->online_cpus);
	cpupro->enabled = false;

	raw_spin_unlock_irqrestore(&cpupro->lock, flag);

	if (!cpumask_weight(&dompro->online_cpus))
		dompro->enabled = false;

	return 0;
}
/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
static int parse_domain_dt(struct domain_profiler *dompro, struct device_node *dn)
{
	const char *buf;
	int ret;

	/* necessary data */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return -1;
	cpulist_parse(buf, &dompro->cpus);
	cpumask_copy(&dompro->online_cpus, &dompro->cpus);

	ret = of_property_read_u32(dn, "cal-id", &dompro->cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "migov-id", &dompro->migov_id);
	if (ret)
		dompro->migov_id = -1;	/* Don't support migov */

	of_property_read_u32(dn, "power-coefficient", &dompro->dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &dompro->st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &dompro->tz_name);

	return 0;
}

static int init_profile_result(struct domain_profiler *dompro,
		struct profile_result *result, int dom_init)
{
	int size = dompro->table_cnt;

	if (init_freq_cstate_result(&result->fc_result, NUM_OF_CSTATE, size))
		return -ENOMEM;
	return 0;
}

static struct freq_table *init_freq_table(struct domain_profiler *dompro,
		struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pos;
	struct freq_table *table;
	struct device *dev;
	int idx;

	dev = get_cpu_device(policy->cpu);
	if (unlikely(!dev)) {
		pr_err("cpupro: failed to get cpu device\n");
		return NULL;
	}

	dompro->table_cnt = cpufreq_table_count_valid_entries(policy);

	table = kcalloc(dompro->table_cnt, sizeof(struct freq_table), GFP_KERNEL);
	if (!table)
		return NULL;

	idx = dompro->table_cnt - 1;
	cpufreq_for_each_valid_entry(pos, policy->freq_table) {
		struct dev_pm_opp *opp;
		unsigned long f_hz, f_mhz, v;

		if ((pos->frequency > policy->cpuinfo.max_freq) ||
		    (pos->frequency < policy->cpuinfo.min_freq))
			continue;

		f_mhz = pos->frequency / 1000;		/* KHz -> MHz */
		f_hz = pos->frequency * 1000;		/* KHz -> Hz */

		/* Get voltage from opp */
		opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);
		v = dev_pm_opp_get_voltage(opp);

		table[idx].freq = pos->frequency;
		table[idx].volt = v;
		table[idx].dyn_cost = pwr_cost(pos->frequency, v,
				dompro->dyn_pwr_coeff, PWR_COST_CFVV);
		table[idx].st_cost = pwr_cost(pos->frequency, v,
				dompro->st_pwr_coeff, PWR_COST_CFVV);
		idx--;
	}

	dompro->max_freq_idx = 0;
	dompro->min_freq_idx = dompro->table_cnt - 1;
	dompro->cur_freq_idx = get_idx_from_freq(table,
			dompro->table_cnt, policy->cur, RELATION_HIGH);

	return table;
}

void exynos_sdp_set_powertable(int id, int cnt, struct freq_table *table);
static int init_domain(struct domain_profiler *dompro, struct device_node *dn)
{
	struct cpufreq_policy *policy;
	int ret, idx = 0, num_of_cpus;

	/* Parse data from Device Tree to init domain */
	ret = parse_domain_dt(dompro, dn);
	if (ret) {
		pr_err("cpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	policy = cpufreq_cpu_get(cpumask_first(&dompro->cpus));
	if (!policy) {
		pr_err("cpupro: failed to get cpufreq_policy(cpu%d)\n",
				cpumask_first(&dompro->cpus));
		return -EINVAL;
	}

	/* init freq table */
	dompro->table = init_freq_table(dompro, policy);
	cpufreq_cpu_put(policy);
	if (!dompro->table) {
		pr_err("cpupro: failed to init freq_table (cpu%d)\n",
				cpumask_first(&dompro->cpus));
		return -ENOMEM;
	}

	if (dompro->migov_id == MIGOV_CL1 || dompro->migov_id == MIGOV_CL2)
		exynos_sdp_set_powertable(dompro->migov_id, dompro->table_cnt, dompro->table);

	for (idx = 0; idx < NUM_OF_USER; idx++)
		if (init_profile_result(dompro, &dompro->result[idx], 1)) {
			kfree(dompro->table);
			return -EINVAL;
		}

	/* get thermal-zone to get temperature */
	if (dompro->tz_name)
		dompro->tz = thermal_zone_get_zone_by_name(dompro->tz_name);

	/* when DTM support this IP and support static cost table, user DTM static table */
	num_of_cpus = cpumask_weight(&dompro->cpus);
	if (dompro->tz)
		init_static_cost(dompro->table, dompro->table_cnt,
				num_of_cpus, dn, dompro->tz);

	return 0;
}

static int init_cpus_of_domain(struct domain_profiler *dompro)
{
	int cpu, idx;

	for_each_cpu(cpu, &dompro->cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		if (!cpupro) {
			pr_err("cpupro: failed to alloc cpu_profiler(%d)\n", cpu);
			return -ENOMEM;
		}

		if (init_freq_cstate(&cpupro->fc, NUM_OF_CSTATE, dompro->table_cnt))
			return -ENOMEM;

		for (idx = 0; idx < NUM_OF_USER; idx++) {
			if (init_freq_cstate_snapshot(&cpupro->fc_snap[idx],
						NUM_OF_CSTATE, dompro->table_cnt))
				return -ENOMEM;

			if (init_profile_result(dompro, &cpupro->result[idx], 1))
				return -ENOMEM;
		}

		/* init spin lock */
		raw_spin_lock_init(&cpupro->lock);
	}

	return 0;
}

static void show_domain_info(struct domain_profiler *dompro)
{
	int idx;
	char buf[10];

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&dompro->cpus));
	pr_info("================ domain(cpus : %s) ================\n", buf);
	pr_info("min= %dKHz, max= %dKHz\n",
			dompro->table[dompro->table_cnt - 1].freq, dompro->table[0].freq);
	for (idx = 0; idx < dompro->table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%5d st_cost=%5d\n",
				idx, dompro->table[idx].freq, dompro->table[idx].volt,
				dompro->table[idx].dyn_cost,
				dompro->table[idx].st_cost);
	if (dompro->tz_name)
		pr_info("support temperature (tz_name=%s)\n", dompro->tz_name);
	if (dompro->migov_id != -1)
		pr_info("support migov domain(id=%d)\n", dompro->migov_id);
}

static int exynos_cpu_profiler_probe(struct platform_device *pdev)
{
	struct domain_profiler *dompro;
	struct device_node *dn;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("cpupro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.root = pdev->dev.of_node;
	profiler.kobj = &pdev->dev.kobj;

	/* init per_cpu variable */
	profiler.cpus = alloc_percpu(struct cpu_profiler);

	/* init list for domain */
	INIT_LIST_HEAD(&profiler.list);

	/* init domains */
	for_each_child_of_node(profiler.root, dn) {
		dompro = kzalloc(sizeof(struct domain_profiler), GFP_KERNEL);
		if (!dompro) {
			pr_err("cpupro: failed to alloc domain\n");
			return -ENOMEM;
		}

		if (init_domain(dompro, dn)) {
			pr_err("cpupro: failed to alloc domain\n");
			return -ENOMEM;
		}

		init_cpus_of_domain(dompro);

		list_add_tail(&dompro->list, &profiler.list);

		/* register profiler to migov */
		if (dompro->migov_id != -1)
			exynos_migov_register_domain(dompro->migov_id,
					&cpu_fn, &cpu_pd_fn);
	}

	/* initialize work and completion for cpu initial setting */
	INIT_WORK(&profiler.init_work, cpupro_init_work);
	mutex_init(&profiler.mode_change_lock);

	/* register cpufreq notifier */
	exynos_cpufreq_register_notifier(&cpupro_cpufreq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);

	/* register cpu pm notifier */
	register_trace_android_vh_cpu_idle_enter(android_vh_cpupro_idle_enter, NULL);
	register_trace_android_vh_cpu_idle_exit(android_vh_cpupro_idle_exit, NULL);

	/* register cpu hotplug notifier */
	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
			"EXYNOS_MIGOV_CPU_POWER_UP_CONTROL",
			cpupro_cpupm_online, NULL);

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"EXYNOS_MIGOV_CPU_POWER_DOWN_CONTROL",
			NULL, cpupro_cpupm_offline);

	/* show domain information */
	list_for_each_entry(dompro, &profiler.list, list)
		show_domain_info(dompro);

	return 0;
}

static const struct of_device_id exynos_cpu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-cpu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_cpu_profiler_match);

static struct platform_driver exynos_cpu_profiler_driver = {
	.probe		= exynos_cpu_profiler_probe,
	.driver	= {
		.name	= "exynos-cpu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_cpu_profiler_match,
	},
};

static int exynos_cpu_profiler_init(void)
{
	return platform_driver_register(&exynos_cpu_profiler_driver);
}
late_initcall(exynos_cpu_profiler_init);

MODULE_DESCRIPTION("Exynos CPU Profiler");
MODULE_LICENSE("GPL");
