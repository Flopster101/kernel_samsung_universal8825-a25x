/*
 * exynos_tmu.c - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2014 Samsung Electronics
 *  Bartlomiej Zolnierkiewicz <b.zolnierkie@samsung.com>
 *  Lukasz Majewski <l.majewski@samsung.com>
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.kachhap@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/thermal.h>
#include <soc/samsung/gpu_cooling.h>
#include <soc/samsung/isp_cooling.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <uapi/linux/sched/types.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/debug-snapshot-log.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-mcinfo.h>
#include <soc/samsung/cal-if.h>
#include <linux/pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <linux/pm_opp.h>

#include "exynos_tmu.h"
#include "../thermal_core.h"
#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
#include "exynos_acpm_tmu.h"
#include <soc/samsung/exynos-pmu-if.h>
#endif
#include <soc/samsung/exynos-cpuhp.h>
#include <linux/ems.h>

#define CREATE_TRACE_POINTS
#include <trace/events/thermal_exynos.h>

#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>

static int exynos_tmu_sec_pm_init(void);
static void exynos_tmu_show_curr_temp(void);
static void exynos_tmu_show_curr_temp_work(struct work_struct *work);

#define TMU_LOG_PERIOD		10

static DECLARE_DELAYED_WORK(tmu_log_work, exynos_tmu_show_curr_temp_work);
static int tmu_log_work_canceled;
#endif /* CONFIG_SEC_PM */

#define EXYNOS_GPU_THERMAL_ZONE_ID		(3)

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

#define INVALID_TRIP -1

/**
 * struct freq_table - frequency table along with power entries
 * @frequency:	frequency in KHz
 * @power:	power in mW
 *
 * This structure is built when the cooling device registers and helps
 * in translating frequency to power and vice versa.
 */
struct freq_table {
	u32 frequency;
	u32 power;
};

/**
 * mul_frac() - multiply two fixed-point numbers
 * @x:	first multiplicand
 * @y:	second multiplicand
 *
 * Return: the result of multiplying two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

/**
 * div_frac() - divide two fixed-point numbers
 * @x:	the dividend
 * @y:	the divisor
 *
 * Return: the result of dividing two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 div_frac(s64 x, s64 y)
{
	return div_s64(x << FRAC_BITS, y);
}

struct kthread_worker hotplug_worker;
static void exynos_throttle_cpu_hotplug(struct kthread_work *work);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
static struct acpm_tmu_cap cap;
static unsigned int num_of_devices, suspended_count;
static bool cp_call_mode;
static bool is_aud_on(void)
{
	unsigned int val;

	val = abox_is_on();

	pr_info("%s AUD_STATUS %d\n", __func__, val);

	return val;
}
#else
static bool suspended;
static DEFINE_MUTEX (thermal_suspend_lock);
#endif

static int thermal_status_level[3];
static bool update_thermal_status;

int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size, char *tz_name)
{
	int i, j, ret = -EINVAL;
	int ratio, asv_group, cal_id;

	void *gen_block;
	struct ect_gen_param_table *volt_temp_param = NULL, *asv_param = NULL;
	int cpu_ratio_table[16] = { 0, 18, 22, 27, 33, 40, 49, 60, 73, 89, 108, 131, 159, 194, 232, 250};
	int g3d_ratio_table[16] = { 0, 25, 29, 35, 41, 48, 57, 67, 79, 94, 110, 130, 151, 162, 162, 162};
	int *ratio_table, *var_coeff_table, *asv_coeff_table;

	if (of_property_read_u32(np, "cal-id", &cal_id)) {
		if (of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id)) {
			pr_err("%s: Failed to get cal-id\n", __func__);
			return -EINVAL;
		}
	}

	ratio = cal_asv_get_ids_info(cal_id);
	asv_group = cal_asv_get_grp(cal_id);

	if (asv_group < 0 || asv_group > 15)
		asv_group = 0;

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL) {
		pr_err("%s: Failed to get gen block from ECT\n", __func__);
		return ret;
	}

	if (!strcmp(tz_name, "MID")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_MID_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_MID_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "BIG")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_BIG_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_BIG_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "G3D")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_G3D_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_G3D_ASV");
		ratio_table = g3d_ratio_table;
	}
	else if (!strcmp(tz_name, "LITTLE")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_LIT_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_LIT_ASV");
		ratio_table = cpu_ratio_table;
	}
	else {
		pr_err("%s: Thermal zone %s does not use PIDTM\n", __func__, tz_name);
		return -EINVAL;
	}

	if (asv_group == 0)
		asv_group = 8;

	if (!ratio)
		ratio = ratio_table[asv_group];

	if (volt_temp_param && asv_param) {
		*var_volt_size = volt_temp_param->num_of_row - 1;
		*var_temp_size = volt_temp_param->num_of_col - 1;

		var_coeff_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!var_coeff_table)
			goto err_mem;

		asv_coeff_table = kzalloc(sizeof(int) *
							asv_param->num_of_row *
							asv_param->num_of_col,
							GFP_KERNEL);
		if (!asv_coeff_table)
			goto free_var_coeff;

		*var_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!*var_table)
			goto free_asv_coeff;

		memcpy(var_coeff_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
		memcpy(asv_coeff_table, asv_param->parameter,
			sizeof(int) * asv_param->num_of_row * asv_param->num_of_col);
		memcpy(*var_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
	} else {
		pr_err("%s: Failed to get param table from ECT\n", __func__);
		return -EINVAL;
	}

	for (i = 1; i <= *var_volt_size; i++) {
		long asv_coeff = (long)asv_coeff_table[3 * i + 0] * asv_group * asv_group
				+ (long)asv_coeff_table[3 * i + 1] * asv_group
				+ (long)asv_coeff_table[3 * i + 2];
		asv_coeff = asv_coeff / 100;

		for (j = 1; j <= *var_temp_size; j++) {
			long var_coeff = (long)var_coeff_table[i * (*var_temp_size + 1) + j];
			var_coeff =  ratio * var_coeff * asv_coeff;
			var_coeff = var_coeff / 100000;
			(*var_table)[i * (*var_temp_size + 1) + j] = (int)var_coeff;
		}
	}

	ret = 0;

free_asv_coeff:
	kfree(asv_coeff_table);
free_var_coeff:
	kfree(var_coeff_table);
err_mem:
	return ret;
}
EXPORT_SYMBOL_GPL(exynos_build_static_power_table);

/* list of multiple instance for each thermal sensor */
static LIST_HEAD(dtm_dev_list);

static enum exynos_tmu_boost_mode exynos_tmu_get_boost_status(void)
{
	struct exynos_tmu_data *devnode;
	enum exynos_tmu_boost_mode ret = THROTTLE_MODE;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->boost_param.use_boost && devnode->boost_param.boost_mode < THROTTLE_MODE) {
			ret = STANDBY_MODE;
			break;
		}
	}

	return ret;
}

static void exynos_report_trigger(struct exynos_tmu_data *p)
{
	struct thermal_zone_device *tz = p->tzd;

	if (!tz) {
		pr_err("No thermal zone device defined\n");
		return;
	}

	mutex_lock(&p->lock);
	if (p->boost_param.use_boost_sync) {
		enum exynos_tmu_boost_mode boost_mode = exynos_tmu_get_boost_status();
		if (p->boost_param.boost_mode == STANDBY_MODE) {
			if (boost_mode == THROTTLE_MODE) {
				p->boost_param.boost_mode = THROTTLE_MODE;

				tz->ops->get_trip_temp(tz, 2, &p->boost_param.boost_mode_trip_2_temp);
				tz->ops->get_trip_temp(tz, 3, &p->boost_param.boost_mode_trip_3_temp);
				tz->ops->get_trip_temp(tz, 4, &p->boost_param.boost_mode_trip_4_temp);

				tz->ops->set_trip_temp(tz, 2, p->boost_param.throttle_mode_trip_2_temp);
				tz->ops->set_trip_temp(tz, 3, p->boost_param.throttle_mode_trip_3_temp);
				tz->ops->set_trip_temp(tz, 4, p->boost_param.throttle_mode_trip_4_temp);
			}
		} else {
			if (boost_mode == STANDBY_MODE) {
				p->boost_param.boost_mode = STANDBY_MODE;

				tz->ops->set_trip_temp(tz, 2, p->boost_param.boost_mode_trip_2_temp);
				tz->ops->set_trip_temp(tz, 3, p->boost_param.boost_mode_trip_3_temp);
				tz->ops->set_trip_temp(tz, 4, p->boost_param.boost_mode_trip_4_temp);
			}
		}
	}
	mutex_unlock(&p->lock);

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
}

static int exynos_tmu_initialize(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tz = data->tzd;
	enum thermal_trip_type type;
	int i, temp;
	unsigned char threshold[8] = {0, };
	unsigned char inten = 0;

	mutex_lock(&data->lock);

	for (i = (of_thermal_get_ntrips(tz) - 1); i >= 0; i--) {
		tz->ops->get_trip_type(tz, i, &type);

		if (type == THERMAL_TRIP_PASSIVE)
			continue;

		tz->ops->get_trip_temp(tz, i, &temp);

		threshold[i] = (unsigned char)(temp / MCELSIUS);
		inten |= (1 << i);
	}
	exynos_acpm_tmu_set_threshold(data->id, threshold);
	exynos_acpm_tmu_set_interrupt_enable(data->id, inten);

	mutex_unlock(&data->lock);

	return 0;
}

static void exynos_tmu_control(struct platform_device *pdev, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
	exynos_acpm_tmu_tz_control(data->id, on);
	data->enabled = on;
	mutex_unlock(&data->lock);
}

static void thermal_freq_qos_init(struct exynos_tmu_data *data)
{

	if (data->limited_frequency) {
		data->policy = cpufreq_cpu_get(cpumask_first(&data->cpu_domain));
		if (data->policy) {
			freq_qos_tracer_add_request(&data->policy->constraints,
					&data->thermal_limit_request, FREQ_QOS_MAX,
					PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		} else {
			pr_err("[%s] Failed to get cpu policy, tz_id: %d\n",
					__func__, data->id);
		}
	}

	if (data->emergency_frequency) {
		data->policy = cpufreq_cpu_get(cpumask_first(&data->cpu_domain));
		if (data->policy) {
			freq_qos_tracer_add_request(&data->policy->constraints,
					&data->emergency_throttle_request, FREQ_QOS_MAX,
					PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		}
	}
	if (data->emergency_frequency_1) {
		data->policy = cpufreq_cpu_get(cpumask_first(&data->cpu_domain));
		if (data->policy) {
			freq_qos_tracer_add_request(&data->policy->constraints,
					&data->emergency_throttle_request, FREQ_QOS_MAX,
					PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		}
	}
}

#define MCINFO_LOG_THRESHOLD	(4)

static int exynos_get_temp(void *p, int *temp)
{
	struct exynos_tmu_data *data = p;
#if IS_ENABLED(CONFIG_EXYNOS_MCINFO)
	unsigned int mcinfo_count;
	unsigned int mcinfo_result[4] = {0, 0, 0, 0};
	unsigned int mcinfo_logging = 0;
	unsigned int mcinfo_temp = 0;
	unsigned int i;
#endif
	int acpm_temp = 0, stat = 0;
	int acpm_data[2];
	unsigned long long dbginfo;
	unsigned int limited_max_freq = 0;
	int emergency_throttle;

	if (!data || !data->enabled)
		return -EINVAL;

	mutex_lock(&data->lock);

	exynos_acpm_tmu_set_read_temp(data->id, &acpm_temp, &stat, acpm_data);

	*temp = acpm_temp * MCELSIUS;

	// Update thermal status
	if (update_thermal_status) {
		ktime_t diff;
		ktime_t cur_time = ktime_get() / NSEC_PER_MSEC;

		diff = cur_time - data->last_thermal_status_updated;

		if (data->last_thermal_status_updated == 0 && *temp >= thermal_status_level[0]) {
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[2]) {
			data->thermal_status[2] += diff;
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[1]) {
			data->thermal_status[1] += diff;
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[0]) {
			data->thermal_status[0] += diff;
			data->last_thermal_status_updated = cur_time;
		} else {
			data->last_thermal_status_updated = 0;
		}
	}

	if (!data->policy) {
		thermal_freq_qos_init(data);
	}

	if (data->policy) {
		if (data->id == 0)
		    limited_max_freq = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE;
		else if (data->id == 1)
		    limited_max_freq = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;

		if (data->limited_frequency_2) {
			if (data->limited == 0) {
				if (*temp >= data->limited_threshold_2) {
					freq_qos_update_request(&data->thermal_limit_request,
							data->limited_frequency_2);

					data->limited = 2;
				} else if (*temp >= data->limited_threshold) {
					freq_qos_update_request(&data->thermal_limit_request,
							data->limited_frequency);
					data->limited = 1;
				}
			} else if (data->limited == 1) {
				if (*temp >= data->limited_threshold_2) {
					freq_qos_update_request(&data->thermal_limit_request,
							data->limited_frequency_2);
					data->limited = 2;
				}
				else if (*temp < data->limited_threshold_release) {
					freq_qos_update_request(&data->thermal_limit_request,
							limited_max_freq);
					data->limited = 0;
				}
			} else if (data->limited == 2) {
				if (*temp < data->limited_threshold_release) {
					freq_qos_update_request(&data->thermal_limit_request,
							limited_max_freq);
					data->limited = 0;
				} else if (*temp < data->limited_threshold_release_2) {
					freq_qos_update_request(&data->thermal_limit_request,
							data->limited_frequency);
					data->limited = 1;
				}
			}
		} else if (data->limited_frequency) {

			if (data->limited == 0) {
				if (*temp >= data->limited_threshold) {
					freq_qos_update_request(&data->thermal_limit_request,
							data->limited_frequency);
					data->limited = 1;
				}
			} else {
				if (*temp < data->limited_threshold_release) {
					freq_qos_update_request(&data->thermal_limit_request,
							limited_max_freq);
					data->limited = 0;
				}
			}
		}

		if (data->emergency_frequency) {

			if (!data->emergency_throttle && (stat & EMERGENCY_THROTTLE_STAT)) {
				freq_qos_update_request(&data->emergency_throttle_request,
						data->emergency_frequency);
				data->emergency_throttle = true;
			} else if (data->emergency_throttle && !(stat & EMERGENCY_THROTTLE_STAT)) {
				freq_qos_update_request(&data->emergency_throttle_request,
						PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
				data->emergency_throttle = false;
			}
		}
		if (data->emergency_frequency_1) {
			if (stat & EMERGENCY_THROTTLE_STAT_3) {
				emergency_throttle = 3;
			} else if (stat & EMERGENCY_THROTTLE_STAT_2) {
				emergency_throttle = 2;
			} else if (stat & EMERGENCY_THROTTLE_STAT_1) {
				emergency_throttle = 1;
			} else {
				emergency_throttle = 0;
			}
			if (data->emergency_throttle != emergency_throttle) {
				switch(emergency_throttle) {
				case 0:
					freq_qos_update_request(&data->emergency_throttle_request,
								PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
					break;
				case 1:
					freq_qos_update_request(&data->emergency_throttle_request,
								data->emergency_frequency_1);
					break;
				case 2:
					freq_qos_update_request(&data->emergency_throttle_request,
								data->emergency_frequency_2);
					break;
				case 3:
					freq_qos_update_request(&data->emergency_throttle_request,
								data->emergency_frequency_3);
					break;
				default:
					BUG();
					break;
				}
				data->emergency_throttle = emergency_throttle;
			}
		}
	}

	if (data->hotplug_enable)
		kthread_queue_work(&hotplug_worker, &data->hotplug_work);

	data->temperature = *temp / 1000;

	mutex_unlock(&data->lock);

	dbginfo = (((unsigned long long)acpm_data[0]) | (((unsigned long long)acpm_data[1]) << 32));
	dbg_snapshot_thermal(data, *temp / 1000, data->tmu_name, dbginfo);
#if IS_ENABLED(CONFIG_EXYNOS_MCINFO) 
	if (data->id == 0) {
		mcinfo_count = get_mcinfo_base_count();
		get_refresh_rate(mcinfo_result);

		for (i = 0; i < mcinfo_count; i++) {
			mcinfo_temp |= (mcinfo_result[i] & 0xf) << (8 * i);

			if (mcinfo_result[i] >= MCINFO_LOG_THRESHOLD)
				mcinfo_logging = 1;
		}

		if (mcinfo_logging == 1)
			dbg_snapshot_thermal(NULL, mcinfo_temp, "MCINFO", 0);
	}
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_SEC_BOOTSTAT)
void sec_bootstat_get_thermal(int *temp)
{
	struct exynos_tmu_data *data;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (!strncasecmp(data->tmu_name, "BIG", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[0]);
			temp[0] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "MID", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[1]);
			temp[1] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "LITTLE", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[2]);
			temp[2] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "G3D", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[3]);
			temp[3] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "ISP", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[4]);
			temp[4] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "NPU", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[5]);
			temp[5] /= 1000;
		} else
			continue;
	}
}
EXPORT_SYMBOL(sec_bootstat_get_thermal);
#endif

static int exynos_get_trend(void *p, int trip, enum thermal_trend *trend)
{
	struct exynos_tmu_data *data = p;
	struct thermal_zone_device *tz = data->tzd;
	int trip_temp, ret = 0;

	if (tz == NULL)
		return ret;

	ret = tz->ops->get_trip_temp(tz, trip, &trip_temp);
	if (ret < 0)
		return ret;

	if (data->use_pi_thermal) {
		*trend = THERMAL_TREND_STABLE;
	} else {
		if (tz->temperature >= trip_temp)
			*trend = THERMAL_TREND_RAISE_FULL;
		else
			*trend = THERMAL_TREND_DROP_FULL;
	}

	return 0;
}

#if defined(CONFIG_THERMAL_EMULATION)
static int exynos_tmu_set_emulation(void *drv_data, int temp)
{
	struct exynos_tmu_data *data = drv_data;
	int ret = -EINVAL;
	unsigned char emul_temp;

	if (temp && temp < MCELSIUS)
		goto out;

	mutex_lock(&data->lock);
	emul_temp = (unsigned char)(temp / MCELSIUS);
	exynos_acpm_tmu_set_emul_temp(data->id, emul_temp);
	mutex_unlock(&data->lock);
	return 0;
out:
	return ret;
}
#else
static int exynos_tmu_set_emulation(void *drv_data, int temp)
	{ return -EINVAL; }
#endif /* CONFIG_THERMAL_EMULATION */

static void start_pi_polling(struct exynos_tmu_data *data, int delay)
{
	kthread_mod_delayed_work(&data->thermal_worker, &data->pi_work,
				 msecs_to_jiffies(delay));
}

static void reset_pi_trips(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	int i, last_active, last_passive;
	bool found_first_passive;

	found_first_passive = false;
	last_active = INVALID_TRIP;
	last_passive = INVALID_TRIP;

	for (i = 0; i < tz->trips; i++) {
		enum thermal_trip_type type;
		int ret;

		ret = tz->ops->get_trip_type(tz, i, &type);
		if (ret) {
			dev_warn(&tz->device,
				 "Failed to get trip point %d type: %d\n", i,
				 ret);
			continue;
		}

		if (type == THERMAL_TRIP_PASSIVE) {
			if (!found_first_passive) {
				params->trip_switch_on = i;
				found_first_passive = true;
				break;
			} else  {
				last_passive = i;
			}
		} else if (type == THERMAL_TRIP_ACTIVE) {
			last_active = i;
		} else {
			break;
		}
	}

	if (last_passive != INVALID_TRIP) {
		params->trip_control_temp = last_passive;
	} else if (found_first_passive) {
		params->trip_control_temp = params->trip_switch_on;
		params->trip_switch_on = last_active;
	} else {
		params->trip_switch_on = INVALID_TRIP;
		params->trip_control_temp = last_active;
	}
}

static void reset_pi_params(struct exynos_tmu_data *data)
{
	s64 i = int_to_frac(data->pi_param->i_max);

	data->pi_param->err_integral = div_frac(i, data->pi_param->k_i);
	data->pi_param->prev_err = -1;
}

static void allow_maximum_power(struct exynos_tmu_data *data)
{
	struct thermal_instance *instance;
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip != params->trip_control_temp) ||
		    (!cdev_is_power_actor(instance->cdev)))
			continue;

		instance->target = 0;
		mutex_lock(&instance->cdev->lock);
		instance->cdev->updated = false;
		mutex_unlock(&instance->cdev->lock);
		thermal_cdev_update(instance->cdev);
	}
}

static u32 pi_calculate(struct exynos_tmu_data *data,
			 int control_temp,
			 u32 max_allocatable_power)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	s64 p, i, power_range, d = 0;
	s32 err, max_power_frac;

	max_power_frac = int_to_frac(max_allocatable_power);

	err = (control_temp - tz->temperature) / 1000;
	err = int_to_frac(err);

	/* Calculate the proportional term */
	p = mul_frac(err < 0 ? params->k_po : params->k_pu, err);

	/*
	 * Calculate the integral term
	 *
	 * if the error is less than cut off allow integration (but
	 * the integral is limited to max power)
	 */
	i = mul_frac(params->k_i, params->err_integral);

	if (err < int_to_frac(params->integral_cutoff)) {
		s64 i_next = i + mul_frac(params->k_i, err);
		s64 i_windup = int_to_frac(-1 * (s64)params->sustainable_power);

		if (i_next > int_to_frac((s64)params->i_max)) {
			i = int_to_frac((s64)params->i_max);
			params->err_integral = div_frac(i, params->k_i);
		} else if (i_next <= i_windup) {
			i = i_windup;
			params->err_integral = div_frac(i, params->k_i);
		} else {
			i = i_next;
			params->err_integral += err;
		}
	}

	if (params->k_d && params->prev_err != -1)
		d = mul_frac(params->k_d, err - params->prev_err);
	else
		d = 0;

	params->prev_err = err;

	power_range = p + i + d;

	power_range = params->sustainable_power + frac_to_int(power_range);

	power_range = clamp(power_range, (s64)0, (s64)max_allocatable_power);

	trace_thermal_exynos_power_allocator_pid(tz, frac_to_int(err),
						 frac_to_int(params->err_integral),
						 frac_to_int(p), frac_to_int(i),
						 frac_to_int(d), power_range);

	return power_range;
}

static int exynos_pi_controller(struct exynos_tmu_data *data, int control_temp)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	struct thermal_instance *instance, *main_instance, *sub_instance;
	struct thermal_cooling_device *cdev;
	int ret = 0;
	bool found_actor = false;
	u32 max_power, power_range;
	unsigned long state;
	struct thermal_cooling_device *sub_cdev = NULL;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip == params->trip_control_temp) &&
		    cdev_is_power_actor(instance->cdev)) {
			if (data->use_sync_pi_thermal) {
				struct exynos_cpufreq_cooling_device *cpufreq_cdev = instance->cdev->devdata;
				struct thermal_zone_device *cdev_tz = cpufreq_cdev ? cpufreq_cdev->tz : NULL;

				if (cdev_tz && (tz->id == cdev_tz->id)) {
					found_actor = true;
					main_instance = instance;
					cdev = instance->cdev;
				} else {
					sub_instance = instance;
					sub_cdev = instance->cdev;
				}
			} else {
				found_actor = true;
				main_instance = instance;
				cdev = instance->cdev;
			}
		}
	}

	if (!found_actor)
		return -ENODEV;

	cdev->ops->state2power(cdev, 0, &max_power);

	power_range = pi_calculate(data, control_temp, max_power);

	ret = cdev->ops->power2state(cdev, power_range, &state);
	if (ret)
		return ret;

	if (data->use_sync_pi_thermal && sub_cdev) {
		unsigned long freq;
		u32 sub_state, cpu, sub_cpu;
		struct dev_pm_opp *opp, *sub_opp;

		cpu = ((struct exynos_cpufreq_cooling_device *)(cdev->devdata))->policy->cpu;
		sub_cpu = ((struct exynos_cpufreq_cooling_device *)(sub_cdev->devdata))->policy->cpu;

		freq = ((struct exynos_cpufreq_cooling_device *)(cdev->devdata))->freq_table[state].frequency * 1000;
		opp = dev_pm_opp_find_freq_ceil(get_cpu_device(cpu), &freq);
		sub_opp = dev_pm_opp_find_freq_ceil_by_volt(get_cpu_device(sub_cpu), dev_pm_opp_get_voltage(opp));

		if (!IS_ERR(sub_opp) && dev_pm_opp_get_freq(sub_opp) < freq)
			sub_opp = dev_pm_opp_find_freq_floor(get_cpu_device(sub_cpu), &freq);

		if (IS_ERR(sub_opp)) {
			unsigned long min_freq = 0;

			sub_opp = dev_pm_opp_find_freq_ceil(get_cpu_device(sub_cpu), &min_freq);
		}

		sub_state = cpufreq_cooling_get_level(sub_cpu, dev_pm_opp_get_freq(sub_opp) / 1000);

		if (sub_cdev != NULL) {
			sub_instance->target = sub_state;
			mutex_lock(&sub_cdev->lock);
			sub_cdev->updated = false;
			mutex_unlock(&sub_cdev->lock);
			thermal_cdev_update(sub_cdev);
		}
	}

	main_instance->target = state;
	mutex_lock(&cdev->lock);
	cdev->updated = false;
	mutex_unlock(&cdev->lock);
	thermal_cdev_update(cdev);

	trace_thermal_exynos_power_allocator(tz, power_range,
					     max_power, tz->temperature,
					     control_temp - tz->temperature);

	return ret;
}

struct exynos_tmu_data *exynos_tmu_get_data_from_tz(struct thermal_zone_device *tz)
{
	struct exynos_tmu_data *data = NULL;

	list_for_each_entry(data, &dtm_dev_list, node)
		if (data->tzd == tz)
			break;

	return data;
}
EXPORT_SYMBOL_GPL(exynos_tmu_get_data_from_tz);

static void __exynos_tmu_set_boost_mode(struct exynos_tmu_data *data, enum exynos_tmu_boost_mode new_mode, ktime_t cur_time)
{
	enum exynos_tmu_boost_mode cur_mode = data->boost_param.boost_mode;
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	struct exynos_tmu_data *devnode;
	u32 dfs_temp;
	bool update_boost_sync = false;
	bool update_boost_mode_time = true;

	if (!(data->boost_param.use_boost || data->boost_param.use_boost_sync))
		return;

	data->boost_param.boost_mode = new_mode;
	data->boost_param.boost_threshold_cnt = 0;

	if (cur_mode == STANDBY_MODE && new_mode == BOOST_MODE) {
		tz->ops->get_trip_temp(tz, params->trip_control_temp, &data->boost_param.boost_mode_control_temp);
		tz->ops->get_trip_temp(tz, params->trip_switch_on, &data->boost_param.boost_mode_switch_on);
		data->boost_param.boost_mode_sustainable_power = params->sustainable_power;
		data->boost_param.boost_mode_hp_in_threshold = data->hotplug_in_threshold * MCELSIUS;
		data->boost_param.boost_mode_hp_out_threshold = data->hotplug_out_threshold * MCELSIUS;

		data->boost_param.sum_boost_time = 0;
		data->boost_param.last_boost_time = cur_time;
	} else if (cur_mode == AMBIENT_BOOT_MODE && new_mode == THROTTLE_MODE) {
		data->boost_param.boost_mode = BOOT_MODE;
		update_boost_mode_time = false;
	} else if (new_mode == THROTTLE_MODE || new_mode == BOOT_MODE) {
		tz->ops->set_trip_temp(tz, params->trip_control_temp, data->boost_param.throttle_mode_control_temp);
		tz->ops->set_trip_temp(tz, params->trip_switch_on, data->boost_param.throttle_mode_switch_on);
		params->sustainable_power = data->boost_param.throttle_mode_sustainable_power;
		if (new_mode == BOOT_MODE)
			exynos_acpm_tmu_change_threshold(data->id, data->boost_param.throttle_mode_dfs_temp / MCELSIUS, 6);
		else if ((cur_mode == BOOT_MODE || cur_mode == AMBIENT_BOOT_MODE) && new_mode == THROTTLE_MODE) {
			tz->ops->get_trip_temp(tz, 6, &dfs_temp);
			exynos_acpm_tmu_change_threshold(data->id, dfs_temp / MCELSIUS, 6);
		}
		data->hotplug_in_threshold = data->boost_param.throttle_mode_hp_in_threshold / MCELSIUS;
		data->hotplug_out_threshold = data->boost_param.throttle_mode_hp_out_threshold / MCELSIUS;

		update_boost_sync = true;
	} else if (cur_mode == THROTTLE_MODE && new_mode == STANDBY_MODE) {
		tz->ops->set_trip_temp(tz, params->trip_control_temp, data->boost_param.boost_mode_control_temp);
		tz->ops->set_trip_temp(tz, params->trip_switch_on, data->boost_param.boost_mode_switch_on);
		params->sustainable_power = data->boost_param.boost_mode_sustainable_power;

		data->hotplug_in_threshold = data->boost_param.boost_mode_hp_in_threshold / MCELSIUS;
		data->hotplug_out_threshold = data->boost_param.boost_mode_hp_out_threshold / MCELSIUS;

		update_boost_sync = true;
	} else if (cur_mode == BOOT_MODE && new_mode == AMBIENT_MODE) {
		data->boost_param.boost_mode = AMBIENT_BOOT_MODE;
		update_boost_mode_time = false;
	} else if (cur_mode == AMBIENT_BOOT_MODE && new_mode == AMBIENT_MODE) {
		tz->ops->get_trip_temp(tz, 6, &dfs_temp);
		exynos_acpm_tmu_change_threshold(data->id, dfs_temp / MCELSIUS, 6);
	}

	if (update_boost_mode_time)
		data->boost_param.last_boost_mode_updated = cur_time;

	if (update_boost_sync)
		list_for_each_entry(devnode, &dtm_dev_list, node)
			if (devnode->boost_param.use_boost_sync)
				exynos_report_trigger(devnode);
}

static void exynos_pi_thermal(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	int ret = 0;
	int switch_on_temp, control_temp, delay;
	ktime_t cur_time = ktime_get();

	if (atomic_read(&data->in_suspend))
		return;

	if (data->tzd) {
		if (!thermal_zone_device_is_enabled(data->tzd)) {
			params->switched_on = false;
			mutex_lock(&data->lock);
			goto polling;
		}
	} else
		return;

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
	mutex_lock(&data->lock);

	/* Check pi boost mode condition */
	if (data->boost_param.use_boost) {
		if (data->boost_param.boost_mode == STANDBY_MODE) {
			if (tz->temperature >= data->boost_param.boost_mode_threshold) {
				data->boost_param.boost_threshold_cnt++;
				if (data->boost_param.boost_threshold_cnt >= 5) {
					// STANDBY_MODE -> BOOST_MODE
					__exynos_tmu_set_boost_mode(data, BOOST_MODE, cur_time);
				}
			} else
				data->boost_param.boost_threshold_cnt = 0;
		} else if (data->boost_param.boost_mode == BOOST_MODE) {
			if (tz->temperature < data->boost_param.boost_mode_exit_threshold) {
				__exynos_tmu_set_boost_mode(data, STANDBY_MODE, cur_time);
			} else {
				if (tz->temperature >= data->boost_param.boost_mode_threshold)
					data->boost_param.sum_boost_time += (cur_time - data->boost_param.last_boost_time);

				if (data->boost_param.sum_boost_time >= data->boost_param.boost_mode_duration)
					// BOOST_MODE -> THROTTLE_MODE
					__exynos_tmu_set_boost_mode(data, THROTTLE_MODE, cur_time);
				data->boost_param.last_boost_time = cur_time;
			}
		} else if (data->boost_param.boost_mode == THROTTLE_MODE) {
			if (tz->temperature >= data->boost_param.throttle_mode_threshold)
				data->boost_param.last_boost_mode_updated = cur_time;
			else if (data->boost_param.last_boost_mode_updated + data->boost_param.throttle_mode_duration < cur_time
					|| tz->temperature < data->boost_param.boost_mode_exit_threshold)
				// THROTTLE_MODE -> STANDBY_MODE
				__exynos_tmu_set_boost_mode(data, STANDBY_MODE, cur_time);
		} else if (data->boost_param.boost_mode == BOOT_MODE) {
			if (data->boost_param.last_boost_mode_updated + data->boost_param.throttle_mode_duration < cur_time)
				__exynos_tmu_set_boost_mode(data, THROTTLE_MODE, cur_time);
		} else if (data->boost_param.boost_mode == AMBIENT_BOOT_MODE) {
			if (data->boost_param.last_boost_mode_updated + data->boost_param.throttle_mode_duration < cur_time)
				__exynos_tmu_set_boost_mode(data, AMBIENT_MODE, cur_time);
		}
	}

	ret = tz->ops->get_trip_temp(tz, params->trip_switch_on,
				     &switch_on_temp);

	if (!ret && (tz->temperature < switch_on_temp)) {
		reset_pi_params(data);
		allow_maximum_power(data);
		params->switched_on = false;
		goto polling;
	}

	params->switched_on = true;

	ret = tz->ops->get_trip_temp(tz, params->trip_control_temp,
				     &control_temp);
	if (ret) {
		pr_warn("Failed to get the maximum desired temperature: %d\n",
			 ret);
		goto polling;
	}

	ret = exynos_pi_controller(data, control_temp);

	if (ret) {
		pr_debug("Failed to calculate pi controller: %d\n",
			 ret);
		goto polling;
	}

polling:
	if (params->switched_on)
		delay = params->polling_delay_on;
	else
		delay = params->polling_delay_off;

	if (!atomic_read(&data->in_suspend))
		start_pi_polling(data, delay);
	mutex_unlock(&data->lock);
}

void exynos_tmu_set_boost_mode(struct exynos_tmu_data *data, enum exynos_tmu_boost_mode new_mode, bool from_amb)
{
	ktime_t cur_time = ktime_get();
	enum exynos_tmu_boost_mode cur_mode;

	if (!data)
		return;

	cur_mode = data->boost_param.boost_mode;

	mutex_lock(&data->lock);
	if (from_amb == true || (from_amb == false && cur_mode != AMBIENT_MODE))
		__exynos_tmu_set_boost_mode(data, new_mode, cur_time);
	mutex_unlock(&data->lock);

	if (cur_mode == AMBIENT_MODE && new_mode != AMBIENT_MODE) {
		if (data->boost_param.use_boost)
			exynos_pi_thermal(data);
		else if (data->boost_param.use_boost_sync)
			exynos_report_trigger(data);
	}
}
EXPORT_SYMBOL_GPL(exynos_tmu_set_boost_mode);

static void exynos_pi_polling(struct kthread_work *work)
{
	struct exynos_tmu_data *data =
			container_of(work, struct exynos_tmu_data, pi_work.work);

	exynos_pi_thermal(data);
}

static void exynos_tmu_work(struct kthread_work *work)
{
	struct exynos_tmu_data *data =
			container_of(work, struct exynos_tmu_data, irq_work);

	mutex_lock(&data->lock);

	exynos_acpm_tmu_clear_tz_irq(data->id);

	mutex_unlock(&data->lock);

	exynos_report_trigger(data);

	if (data->use_pi_thermal)
		exynos_pi_thermal(data);

	enable_irq(data->irq);
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;

	disable_irq_nosync(irq);
	kthread_queue_work(&data->thermal_worker, &data->irq_work);

	return IRQ_HANDLED;
}

static int exynos_tmu_pm_notify(struct notifier_block *nb,
			     unsigned long mode, void *_unused)
{
	struct exynos_tmu_data *data = container_of(nb,
			struct exynos_tmu_data, nb);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
#if IS_ENABLED(CONFIG_SEC_PM)
		if (!tmu_log_work_canceled) {
			tmu_log_work_canceled = 1;
			cancel_delayed_work_sync(&tmu_log_work);
		}
#endif /* CONFIG_SEC_PM */
		if (data->use_pi_thermal)
			mutex_lock(&data->lock);

		atomic_set(&data->in_suspend, 1);

		if (data->use_pi_thermal) {
			mutex_unlock(&data->lock);
			kthread_cancel_delayed_work_sync(&data->pi_work);
		}
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&data->in_suspend, 0);
		if (data->use_pi_thermal)
			exynos_pi_thermal(data);
#if IS_ENABLED(CONFIG_SEC_PM)
		if (tmu_log_work_canceled) {
			tmu_log_work_canceled = 0;
			schedule_delayed_work(&tmu_log_work, TMU_LOG_PERIOD * HZ);
		}
#endif /* CONFIG_SEC_PM */
		break;
	default:
		break;
	}
	return 0;
}

static const struct of_device_id exynos_tmu_match[] = {
	{ .compatible = "samsung,exynos-tmu-v2", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);

static int exynos_tmu_irq_work_init(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct cpumask mask;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	struct task_struct *thread;
	int ret = 0;

	kthread_init_worker(&data->thermal_worker);
	thread = kthread_create(kthread_worker_fn, &data->thermal_worker,
			"thermal_%s", data->tmu_name);
	if (IS_ERR(thread)) {
		dev_err(&pdev->dev, "failed to create thermal thread: %ld\n",
				PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		dev_warn(&pdev->dev, "thermal failed to set SCHED_FIFO\n");
		return ret;
	}

	kthread_init_work(&data->irq_work, exynos_tmu_work);

	wake_up_process(thread);

	if (data->hotplug_enable) {
		kthread_init_work(&data->hotplug_work, exynos_throttle_cpu_hotplug);

		snprintf(data->cpuhp_name, THERMAL_NAME_LENGTH, "DTM_%s", data->tmu_name);
		ecs_request_register(data->cpuhp_name, cpu_possible_mask);
	}

	return ret;
}

int exynos_tmu_extern_get_temp(int tzid)
{
	int temp = 0, stat = 0;

	if (unlikely(list_empty(&dtm_dev_list)))
		return -EINVAL;

	exynos_acpm_tmu_set_read_temp(tzid, &temp, &stat, NULL);

	return temp;
}
EXPORT_SYMBOL_GPL(exynos_tmu_extern_get_temp);

static int exynos_map_dt_data(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct resource res;
	const char *tmu_name, *buf;
	int ret;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;

	data->np = pdev->dev.of_node;

	if (of_property_read_u32(pdev->dev.of_node, "id", &data->id)) {
		dev_err(&pdev->dev, "failed to get TMU ID\n");
		return -ENODEV;
	}

	data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (data->irq <= 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return -ENODEV;
	}

	if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
		dev_err(&pdev->dev, "failed to get Resource 0\n");
		return -ENODEV;
	}

	data->base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
	if (!data->base) {
		dev_err(&pdev->dev, "Failed to ioremap memory\n");
		return -EADDRNOTAVAIL;
	}

	if (of_property_read_string(pdev->dev.of_node, "tmu_name", &tmu_name)) {
		dev_err(&pdev->dev, "failed to get tmu_name\n");
	} else
		strncpy(data->tmu_name, tmu_name, THERMAL_NAME_LENGTH);

	if (of_property_read_bool(pdev->dev.of_node, "thermal_status_level")) {
		of_property_read_u32_array(pdev->dev.of_node, "thermal_status_level", (u32 *)&thermal_status_level,
				(size_t)(ARRAY_SIZE(thermal_status_level)));
		update_thermal_status = true;
	}

	data->hotplug_enable = of_property_read_bool(pdev->dev.of_node, "hotplug_enable");
	if (data->hotplug_enable) {
		dev_info(&pdev->dev, "thermal zone use hotplug function \n");
		of_property_read_u32(pdev->dev.of_node, "hotplug_in_threshold",
					&data->hotplug_in_threshold);
		if (!data->hotplug_in_threshold)
			dev_err(&pdev->dev, "No input hotplug_in_threshold \n");

		of_property_read_u32(pdev->dev.of_node, "hotplug_out_threshold",
					&data->hotplug_out_threshold);
		if (!data->hotplug_out_threshold)
			dev_err(&pdev->dev, "No input hotplug_out_threshold \n");

		ret = of_property_read_string(pdev->dev.of_node, "cpu_domain", &buf);
		if (!ret)
			cpulist_parse(buf, &data->cpu_domain);
	}

	if (of_property_read_bool(pdev->dev.of_node, "emergency_frequency")) {
		int cal_id = 0;

		of_property_read_u32(pdev->dev.of_node, "cal_id", &cal_id);
		if (cal_id) {
			data->emergency_frequency = cal_dfs_get_max_freq(cal_id) - 1;
			dev_info(&pdev->dev, "Use emergency frequency as %d\n", data->emergency_frequency);
		}
	}

	if (of_property_read_bool(pdev->dev.of_node, "emergency_frequency_v2")) {
		int cal_id = 0;

		of_property_read_u32(pdev->dev.of_node, "cal_id", &cal_id);
		if (cal_id) {
			data->emergency_throttle = 0;
			data->emergency_frequency_1 = 2288000;
			data->emergency_frequency_2 = 2132000;
			data->emergency_frequency_3 = 2002000;
			dev_info(&pdev->dev, "Use emergency frequency as %d\n", data->emergency_frequency_1);
		}
	}

	if (of_property_read_bool(pdev->dev.of_node, "use-pi-thermal")) {
		struct exynos_pi_param *params;
		u32 value;

		data->use_pi_thermal = true;

		params = kzalloc(sizeof(*params), GFP_KERNEL);
		if (!params)
			return -ENOMEM;

		data->use_sync_pi_thermal = of_property_read_bool(pdev->dev.of_node, "use-sync-pi-thermal");

		of_property_read_u32(pdev->dev.of_node, "polling_delay_on",
					&params->polling_delay_on);
		if (!params->polling_delay_on)
			dev_err(&pdev->dev, "No input polling_delay_on \n");

		of_property_read_u32(pdev->dev.of_node, "polling_delay_off",
					&params->polling_delay_off);
		if (!params->polling_delay_off)
			dev_err(&pdev->dev, "No input polling_delay_off \n");

		ret = of_property_read_u32(pdev->dev.of_node, "k_po",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_po\n");
		else
			params->k_po = int_to_frac(value);


		ret = of_property_read_u32(pdev->dev.of_node, "k_pu",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_pu\n");
		else
			params->k_pu = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "k_i",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_i\n");
		else
			params->k_i = int_to_frac(value);

		ret = of_property_read_s32(pdev->dev.of_node, "k_d",
					   &value);
		if (ret > 0)
			params->k_d = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "i_max",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input i_max\n");
		else
			params->i_max = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "integral_cutoff",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input integral_cutoff\n");
		else
			params->integral_cutoff = value;

		ret = of_property_read_u32(pdev->dev.of_node, "sustainable_power",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input sustainable_power\n");
		else
			params->sustainable_power = value;

		data->boost_param.use_boost = of_property_read_bool(pdev->dev.of_node, "use-boost");

		if (of_property_read_bool(pdev->dev.of_node, "use-dt-boost-param")) {
			if (of_property_read_u32(pdev->dev.of_node, "boost_mode_threshold",
						&data->boost_param.boost_mode_threshold) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "boost_mode_threshold %d\n", data->boost_param.boost_mode_threshold);

			if (of_property_read_u32(pdev->dev.of_node, "boost_mode_exit_threshold",
						&data->boost_param.boost_mode_exit_threshold) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "boost_mode_exit_threshold %d\n", data->boost_param.boost_mode_exit_threshold);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_threshold",
						&data->boost_param.throttle_mode_threshold) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "throttle_mode_threshold %d\n", data->boost_param.throttle_mode_threshold);

			if (of_property_read_u32(pdev->dev.of_node, "boost_mode_duration",
						(u32 *)&data->boost_param.boost_mode_duration) < 0)
				data->boost_param.use_boost = false;
			else {
				dev_info(&pdev->dev, "boost_mode_duration %d\n", data->boost_param.boost_mode_duration);
				data->boost_param.boost_mode_duration *= NSEC_PER_SEC;
			}

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_duration",
						(u32 *)&data->boost_param.throttle_mode_duration) < 0)
				data->boost_param.use_boost = false;
			else {
				dev_info(&pdev->dev, "throttle_mode_duration %d\n", data->boost_param.throttle_mode_duration);
				data->boost_param.throttle_mode_duration *= NSEC_PER_SEC;
			}

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_control_temp",
						&data->boost_param.throttle_mode_control_temp) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "throttle_mode_control_temp %d\n",
						data->boost_param.throttle_mode_control_temp);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_switch_on",
						&data->boost_param.throttle_mode_switch_on) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "throttle_mode_switch_on %d\n", data->boost_param.throttle_mode_switch_on);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_dfs_temp",
						&data->boost_param.throttle_mode_dfs_temp) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "throttle_mode_dfs_temp %d\n", data->boost_param.throttle_mode_dfs_temp);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_sustainable_power",
						&data->boost_param.throttle_mode_sustainable_power) < 0)
				data->boost_param.use_boost = false;
			else
				dev_info(&pdev->dev, "throttle_mode_sustainable_power %d\n",
						data->boost_param.throttle_mode_sustainable_power);

			if (data->hotplug_enable) {
				if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_hp_in_threshold",
							&data->boost_param.throttle_mode_hp_in_threshold) < 0)
					data->boost_param.use_boost = false;
				else
					dev_info(&pdev->dev, "throttle_mode_hp_in_threshold %d\n",
							data->boost_param.throttle_mode_hp_in_threshold);

				if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_hp_out_threshold",
							&data->boost_param.throttle_mode_hp_out_threshold) < 0)
					data->boost_param.use_boost = false;
				else
					dev_info(&pdev->dev, "throttle_mode_hp_out_threshold %d\n",
							data->boost_param.throttle_mode_hp_out_threshold);
			}
		}

		data->pi_param = params;
	} else {
		data->use_pi_thermal = false;

		data->boost_param.use_boost_sync = of_property_read_bool(pdev->dev.of_node, "use-boost-sync");

		if (data->boost_param.use_boost_sync) {
			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_trip_2_temp",
						&data->boost_param.throttle_mode_trip_2_temp) < 0)
				data->boost_param.use_boost_sync = false;
			else
				dev_info(&pdev->dev, "throttle_mode_trip_2_temp %d\n",
						data->boost_param.throttle_mode_trip_2_temp);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_trip_3_temp",
						&data->boost_param.throttle_mode_trip_3_temp) < 0)
				data->boost_param.use_boost_sync = false;
			else
				dev_info(&pdev->dev, "throttle_mode_trip_3_temp %d\n",
						data->boost_param.throttle_mode_trip_3_temp);

			if (of_property_read_u32(pdev->dev.of_node, "throttle_mode_trip_4_temp",
						&data->boost_param.throttle_mode_trip_4_temp) < 0)
				data->boost_param.use_boost_sync = false;
			else
				dev_info(&pdev->dev, "throttle_mode_trip_4_temp %d\n",
						data->boost_param.throttle_mode_trip_4_temp);
		}

	}

	return 0;
}

static void exynos_throttle_cpu_hotplug(struct kthread_work *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, hotplug_work);
	struct cpumask mask;

	mutex_lock(&data->lock);

	if (data->is_cpu_hotplugged_out) {
		if (data->temperature < data->hotplug_in_threshold) {
			/*
			 * If current temperature is lower than low threshold,
			 * call cluster1_cores_hotplug(false) for hotplugged out cpus.
			 */
			ecs_request(data->cpuhp_name, cpu_possible_mask);
			data->is_cpu_hotplugged_out = false;
		}
	} else {
		if (data->temperature >= data->hotplug_out_threshold) {
			/*
			 * If current temperature is higher than high threshold,
			 * call cluster1_cores_hotplug(true) to hold temperature down.
			 */
			data->is_cpu_hotplugged_out = true;
			cpumask_andnot(&mask, cpu_possible_mask, &data->cpu_domain);
			ecs_request(data->cpuhp_name, &mask);
		}
	}

	mutex_unlock(&data->lock);
}

static const struct thermal_zone_of_device_ops exynos_hotplug_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.get_trend = exynos_get_trend,
};

static const struct thermal_zone_of_device_ops exynos_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.get_trend = exynos_get_trend,
};


static ssize_t
hotplug_out_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_out_threshold);
}

static ssize_t
hotplug_out_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_out = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_out)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	data->hotplug_out_threshold = hotplug_out;

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
hotplug_in_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_in_threshold);
}

static ssize_t
hotplug_in_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_in = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_in)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	data->hotplug_in_threshold = hotplug_in;

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
sustainable_power_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	if (data->pi_param)
		return sprintf(buf, "%u\n", data->pi_param->sustainable_power);
	else
		return -EIO;
}

static ssize_t
sustainable_power_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 sustainable_power;

	if (!data->pi_param)
		return -EIO;

	if (kstrtou32(buf, 10, &sustainable_power))
		return -EINVAL;

	data->pi_param->sustainable_power = sustainable_power;

	return count;
}

static ssize_t
temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *tmu_data = platform_get_drvdata(pdev);
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;

	exynos_acpm_tmu_ipc_dump(tmu_data->id, data.dump);

	return snprintf(buf, PAGE_SIZE, "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
}

static ssize_t
use_boost_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", data->boost_param.use_boost);
}

static ssize_t
use_boost_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 use_boost;

	if (kstrtou32(buf, 10, &use_boost))
		return -EINVAL;

	data->boost_param.use_boost = !!use_boost;

	return count;
}

static ssize_t
emergency_frequency_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	if (data)
		return sprintf(buf, "%u\n", data->emergency_frequency);
	else
		return -EIO;
}

static ssize_t
emergency_frequency_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 emergency_frequency;

	if (kstrtou32(buf, 10, &emergency_frequency))
		return -EINVAL;

	data->emergency_frequency = emergency_frequency;

	return count;
}

static ssize_t
use_boost_sync_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", data->boost_param.use_boost_sync);
}

static ssize_t
use_boost_sync_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 use_boost_sync;

	if (kstrtou32(buf, 10, &use_boost_sync))
		return -EINVAL;

	data->boost_param.use_boost_sync = !!use_boost_sync;

	return count;
}

static ssize_t
boost_mode_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->boost_param.boost_mode);
}

static ssize_t
boost_mode_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 boost_mode;

	if (kstrtou32(buf, 10, &boost_mode))
		return -EINVAL;

	if (boost_mode < MODE_END)
		exynos_tmu_set_boost_mode(data, boost_mode, true);

	return count;
}

static ssize_t
boost_mode_duration_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", data->boost_param.boost_mode_duration / NSEC_PER_SEC);
}

static ssize_t
boost_mode_duration_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 boost_mode_duration;

	if (kstrtou32(buf, 10, &boost_mode_duration))
		return -EINVAL;

	data->boost_param.boost_mode_duration = boost_mode_duration * NSEC_PER_SEC;

	return count;
}

static ssize_t
throttle_mode_duration_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", data->boost_param.throttle_mode_duration / NSEC_PER_SEC);
}

static ssize_t
throttle_mode_duration_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 throttle_mode_duration;

	if (kstrtou32(buf, 10, &throttle_mode_duration))
		return -EINVAL;

	data->boost_param.throttle_mode_duration = throttle_mode_duration * NSEC_PER_SEC;

	return count;
}

static ssize_t
throttle_mode_sustainable_power_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%u\n", data->boost_param.throttle_mode_sustainable_power);
}

static ssize_t
throttle_mode_sustainable_power_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 throttle_mode_sustainable_power;

	if (kstrtou32(buf, 10, &throttle_mode_sustainable_power))
		return -EINVAL;

	data->boost_param.throttle_mode_sustainable_power = throttle_mode_sustainable_power;

	return count;
}

static ssize_t thermal_log_show(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t offset, size_t count)
{
	ssize_t len = 0, printed = 0;
	static unsigned int front = 0, log_len = 0, i = 0;
	struct thermal_log *log;
	char str[256];

	if (offset == 0) {
		front = dss_get_first_thermal_log_idx();
		log_len = dss_get_len_thermal_log();
		printed = 0;
		i = 0;
		len = snprintf(str, sizeof(str), "TEST: %d %d\n", front, log_len);
		memcpy(buf + printed, str, len);
		printed += len;
	}

	for ( ; i < log_len; i++) {
		log = dss_get_thermal_log_iter(i + front);
		len = snprintf(str, sizeof(str), "%llu %d %s %d %llu\n", log->time, log->cpu, log->cooling_device, log->temp, log->cooling_state);

		if (len + printed <= count) {
			memcpy(buf + printed, str, len);
			printed += len;
		} else
			break;
	}

	return printed;
}

static ssize_t thermal_status_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	ssize_t count = 0;
	struct exynos_tmu_data *devnode;

	count += snprintf(buf + count, PAGE_SIZE, "DOMAIN %10d %10d %10d\n",
			thermal_status_level[0], thermal_status_level[1],
			thermal_status_level[2]);
	list_for_each_entry(devnode, &dtm_dev_list, node) {
		exynos_report_trigger(devnode);
		mutex_lock(&devnode->lock);
		count += snprintf(buf + count, PAGE_SIZE, "%6s %10llu %10llu %10llu\n",
				devnode->tmu_name, devnode->thermal_status[0],
				devnode->thermal_status[1], devnode->thermal_status[2]);
		devnode->thermal_status[0] = 0;
		devnode->thermal_status[1] = 0;
		devnode->thermal_status[2] = 0;
		mutex_unlock(&devnode->lock);
	}

	return count;
}

static struct bin_attribute thermal_log_bin_attr = {
	.attr.name = "thermal_log",
	.attr.mode = 0444,
	.read = thermal_log_show,
};

static struct kobj_attribute thermal_status_attr = __ATTR(thermal_status, 0440, thermal_status_show, NULL);

#define create_s32_param_attr(name)						\
	static ssize_t								\
	name##_show(struct device *dev, struct device_attribute *devattr, 	\
		char *buf)							\
	{									\
	struct platform_device *pdev = to_platform_device(dev);			\
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);		\
										\
	if (data->pi_param)							\
		return sprintf(buf, "%d\n", data->pi_param->name);		\
	else									\
		return -EIO;							\
	}									\
										\
	static ssize_t								\
	name##_store(struct device *dev, struct device_attribute *devattr, 	\
		const char *buf, size_t count)					\
	{									\
		struct platform_device *pdev = to_platform_device(dev);		\
		struct exynos_tmu_data *data = platform_get_drvdata(pdev);	\
		s32 value;							\
										\
		if (!data->pi_param)						\
			return -EIO;						\
										\
		if (kstrtos32(buf, 10, &value))					\
			return -EINVAL;						\
										\
		data->pi_param->name = value;					\
										\
		return count;							\
	}									\
	static DEVICE_ATTR_RW(name)

#define create_s32_boost_param_attr(name)						\
	static ssize_t								\
	name##_show(struct device *dev, struct device_attribute *devattr, 	\
		char *buf)							\
	{									\
	struct platform_device *pdev = to_platform_device(dev);			\
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);		\
										\
	return sprintf(buf, "%d\n", data->boost_param.name);		\
	}									\
										\
	static ssize_t								\
	name##_store(struct device *dev, struct device_attribute *devattr, 	\
		const char *buf, size_t count)					\
	{									\
		struct platform_device *pdev = to_platform_device(dev);		\
		struct exynos_tmu_data *data = platform_get_drvdata(pdev);	\
		s32 value;							\
										\
		if (kstrtos32(buf, 10, &value))					\
			return -EINVAL;						\
										\
		data->boost_param.name = value;					\
										\
		return count;							\
	}									\
	static DEVICE_ATTR_RW(name)

static DEVICE_ATTR(hotplug_out_temp, S_IWUSR | S_IRUGO, hotplug_out_temp_show,
		hotplug_out_temp_store);

static DEVICE_ATTR(hotplug_in_temp, S_IWUSR | S_IRUGO, hotplug_in_temp_show,
		hotplug_in_temp_store);

static DEVICE_ATTR_RW(sustainable_power);
static DEVICE_ATTR_RW(throttle_mode_sustainable_power);
static DEVICE_ATTR_RW(use_boost);
static DEVICE_ATTR_RW(use_boost_sync);
static DEVICE_ATTR_RW(boost_mode_duration);
static DEVICE_ATTR_RW(throttle_mode_duration);
static DEVICE_ATTR_RW(boost_mode);
static DEVICE_ATTR_RW(emergency_frequency);
static DEVICE_ATTR(temp, S_IRUGO, temp_show, NULL);

create_s32_param_attr(k_po);
create_s32_param_attr(k_pu);
create_s32_param_attr(k_i);
create_s32_param_attr(k_d);
create_s32_param_attr(i_max);
create_s32_param_attr(integral_cutoff);

create_s32_boost_param_attr(boost_mode_threshold);
create_s32_boost_param_attr(boost_mode_exit_threshold);
create_s32_boost_param_attr(throttle_mode_threshold);
create_s32_boost_param_attr(throttle_mode_control_temp);
create_s32_boost_param_attr(throttle_mode_switch_on);
create_s32_boost_param_attr(throttle_mode_dfs_temp);
create_s32_boost_param_attr(throttle_mode_trip_2_temp);
create_s32_boost_param_attr(throttle_mode_trip_3_temp);
create_s32_boost_param_attr(throttle_mode_trip_4_temp);

static struct attribute *exynos_tmu_attrs[] = {
	&dev_attr_hotplug_out_temp.attr,
	&dev_attr_hotplug_in_temp.attr,
	&dev_attr_sustainable_power.attr,
	&dev_attr_emergency_frequency.attr,
	&dev_attr_k_po.attr,
	&dev_attr_k_pu.attr,
	&dev_attr_k_i.attr,
	&dev_attr_k_d.attr,
	&dev_attr_i_max.attr,
	&dev_attr_integral_cutoff.attr,
	&dev_attr_boost_mode_threshold.attr,
	&dev_attr_throttle_mode_threshold.attr,
	&dev_attr_throttle_mode_control_temp.attr,
	&dev_attr_throttle_mode_switch_on.attr,
	&dev_attr_boost_mode.attr,
	&dev_attr_use_boost.attr,
	&dev_attr_use_boost_sync.attr,
	&dev_attr_boost_mode_duration.attr,
	&dev_attr_throttle_mode_duration.attr,
	&dev_attr_throttle_mode_sustainable_power.attr,
	&dev_attr_boost_mode_exit_threshold.attr,
	&dev_attr_throttle_mode_trip_2_temp.attr,
	&dev_attr_throttle_mode_trip_3_temp.attr,
	&dev_attr_throttle_mode_trip_4_temp.attr,
	&dev_attr_throttle_mode_dfs_temp.attr,
	&dev_attr_temp.attr,
	NULL,
};

static const struct attribute_group exynos_tmu_attr_group = {
	.attrs = exynos_tmu_attrs,
};

#define PARAM_NAME_LENGTH	25

#if IS_ENABLED(CONFIG_ECT)
static int exynos_tmu_ect_get_param(struct ect_pidtm_block *pidtm_block, char *name)
{
	int i;
	int param_value = -1;

	for (i = 0; i < pidtm_block->num_of_parameter; i++) {
		if (!strncasecmp(pidtm_block->param_name_list[i], name, PARAM_NAME_LENGTH)) {
			param_value = pidtm_block->param_value_list[i];
			break;
		}
	}

	return param_value;
}

#define parse_ect_get_boost_param(name, coeff)										\
	if ((value = exynos_tmu_ect_get_param(pidtm_block, #name)) != -1) {		\
			pr_info("Parse from ECT ##name##: %d\n", value);			\
			data->boost_param.name = value * (coeff);				\
		}

static int exynos_tmu_parse_ect(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	int ntrips = 0;

	if (!tz)
		return -EINVAL;

	if (!data->use_pi_thermal) {
		/* if pi thermal not used */
		void *thermal_block;
		struct ect_ap_thermal_function *function;
		int i, temperature;
		int hotplug_threshold_temp = 0, hotplug_flag = 0;
		unsigned int freq;

		thermal_block = ect_get_block(BLOCK_AP_THERMAL);
		if (thermal_block == NULL) {
			pr_err("Failed to get thermal block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		function = ect_ap_thermal_get_function(thermal_block, tz->type);
		if (function == NULL) {
			pr_err("Failed to get thermal block %s", tz->type);
			return -EINVAL;
		}

		ntrips = of_thermal_get_ntrips(tz);
		pr_info("Trip count parsed from ECT : %d, ntrips: %d, zone : %s",
			function->num_of_range, ntrips, tz->type);

		for (i = 0; i < function->num_of_range; ++i) {
			temperature = function->range_list[i].lower_bound_temperature;
			freq = function->range_list[i].max_frequency;
			tz->ops->set_trip_temp(tz, i, temperature  * MCELSIUS);

			pr_info("Parsed From ECT : [%d] Temperature : %d, frequency : %u\n",
					i, temperature, freq);

			if (function->range_list[i].flag != hotplug_flag) {
				if (function->range_list[i].flag != hotplug_flag) {
					hotplug_threshold_temp = temperature;
					hotplug_flag = function->range_list[i].flag;
					data->hotplug_out_threshold = temperature;

					if (i)
						data->hotplug_in_threshold = function->range_list[i-1].lower_bound_temperature;

					pr_info("[ECT]hotplug_threshold : %d\n", hotplug_threshold_temp);
					pr_info("[ECT]hotplug_in_threshold : %d\n", data->hotplug_in_threshold);
					pr_info("[ECT]hotplug_out_threshold : %d\n", data->hotplug_out_threshold);
				}
			}

			if (hotplug_threshold_temp != 0)
				data->hotplug_enable = true;
			else
				data->hotplug_enable = false;

		}
	} else {
		void *block;
		struct ect_pidtm_block *pidtm_block;
		struct exynos_pi_param *params;
		int i, temperature, value;
		int hotplug_out_threshold = 0, hotplug_in_threshold = 0, limited_frequency = 0;
		int limited_threshold = 0, limited_threshold_release = 0;

		block = ect_get_block(BLOCK_PIDTM);
		if (block == NULL) {
			pr_err("Failed to get PIDTM block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		pidtm_block = ect_pidtm_get_block(block, tz->type);
		if (pidtm_block == NULL) {
			pr_err("Failed to get PIDTM block %s", tz->type);
			return -EINVAL;
		}

		ntrips = of_thermal_get_ntrips(tz);
		pr_info("Trip count parsed from ECT : %d, ntrips: %d, zone : %s",
			pidtm_block->num_of_temperature, ntrips, tz->type);

		for (i = 0; i < pidtm_block->num_of_temperature; ++i) {
			temperature = pidtm_block->temperature_list[i];
			tz->ops->set_trip_temp(tz, i, temperature  * MCELSIUS);
			pr_info("Parsed From ECT : [%d] Temperature : %d\n", i, temperature);
		}

		params = data->pi_param;

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_po")) != -1) {
			pr_info("Parse from ECT k_po: %d\n", value);
			params->k_po = int_to_frac(value);
		} else
			pr_err("Fail to parse k_po parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_pu")) != -1) {
			pr_info("Parse from ECT k_pu: %d\n", value);
			params->k_pu = int_to_frac(value);
		} else
			pr_err("Fail to parse k_pu parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_i")) != -1) {
			pr_info("Parse from ECT k_i: %d\n", value);
			params->k_i = int_to_frac(value);
		} else
			pr_err("Fail to parse k_i parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_d")) != -1) {
			pr_info("Parse from ECT k_d: %d\n", value);
			params->k_d = int_to_frac(value);
		}

		/* integral_max */
		if ((value = exynos_tmu_ect_get_param(pidtm_block, "i_max")) != -1) {
			pr_info("Parse from ECT i_max: %d\n", value);
			params->i_max = value;
		} else
			pr_err("Fail to parse i_max parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "integral_cutoff")) != -1) {
			pr_info("Parse from ECT integral_cutoff: %d\n", value);
			params->integral_cutoff = value;
		} else
			pr_err("Fail to parse integral_cutoff parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "p_control_t")) != -1) {
			pr_info("Parse from ECT p_control_t: %d\n", value);
			params->sustainable_power = value;
		} else
			pr_err("Fail to parse p_control_t parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_out_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_out_threshold: %d\n", value);
			hotplug_out_threshold = value;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_in_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_in_threshold: %d\n", value);
			hotplug_in_threshold = value;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			tz->ops->set_trip_temp(tz, 3, temperature  * MCELSIUS);
			data->limited_threshold = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release = limited_threshold_release;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency_1")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_1")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			tz->ops->set_trip_temp(tz, 3, temperature  * MCELSIUS);
			data->limited_threshold = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release_1")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release = limited_threshold_release;
		}


		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency_2")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency_2 = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_2")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			tz->ops->set_trip_temp(tz, 4, temperature  * MCELSIUS);
			data->limited_threshold_2 = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release_2")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release_2 = limited_threshold_release;
		}

		parse_ect_get_boost_param(boost_mode_threshold, MCELSIUS);
		parse_ect_get_boost_param(boost_mode_exit_threshold, MCELSIUS);
		parse_ect_get_boost_param(throttle_mode_threshold, MCELSIUS);
		parse_ect_get_boost_param(boost_mode_duration, NSEC_PER_SEC);
		parse_ect_get_boost_param(throttle_mode_duration, NSEC_PER_SEC);
		parse_ect_get_boost_param(throttle_mode_sustainable_power, 1);
		parse_ect_get_boost_param(throttle_mode_control_temp, MCELSIUS);
		parse_ect_get_boost_param(throttle_mode_switch_on, MCELSIUS);
		parse_ect_get_boost_param(throttle_mode_dfs_temp, MCELSIUS);

		if (hotplug_out_threshold != 0 && hotplug_in_threshold != 0) {
			data->hotplug_out_threshold = hotplug_out_threshold;
			data->hotplug_in_threshold = hotplug_in_threshold;
			parse_ect_get_boost_param(throttle_mode_hp_in_threshold, MCELSIUS);
			parse_ect_get_boost_param(throttle_mode_hp_out_threshold, MCELSIUS);
			data->hotplug_enable = true;
		} else {
			data->hotplug_enable = false;
		}
	}
	return 0;
};
#endif

#if defined(CONFIG_MALI_DEBUG_KERNEL_SYSFS)
struct exynos_tmu_data *gpu_thermal_data;
#endif

static int exynos_thermal_create_debugfs(void);
static int exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	int ret;
	unsigned int irq_flags = IRQF_SHARED;
	struct kobject *kobj;

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_tmu_data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	ret = exynos_map_dt_data(pdev);
	if (ret)
		goto err_sensor;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
	if (list_empty(&dtm_dev_list)) {
		exynos_acpm_tmu_init();
		exynos_acpm_tmu_set_init(&cap);
	}
#endif

	data->tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, data,
						    data->hotplug_enable ?
						    &exynos_hotplug_sensor_ops :
						    &exynos_sensor_ops);
	if (IS_ERR(data->tzd)) {
		ret = PTR_ERR(data->tzd);
		dev_err(&pdev->dev, "Failed to register sensor: %d\n", ret);
		goto err_sensor;
	}

	thermal_zone_device_disable(data->tzd);

#if IS_ENABLED(CONFIG_ECT)
	exynos_tmu_parse_ect(data);
#endif

	ret = exynos_tmu_initialize(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize TMU\n");
		goto err_thermal;
	}

	ret = exynos_tmu_irq_work_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "cannot exynow tmu interrupt work initialize\n");
		goto err_thermal;
	}

#ifdef MULTI_IRQ_SUPPORT_ITMON
	irq_flags |= IRQF_GIC_MULTI_TARGET;
#endif

	if (data->use_pi_thermal) {
		kthread_init_delayed_work(&data->pi_work, exynos_pi_polling);
	}

	ret = devm_request_irq(&pdev->dev, data->irq, exynos_tmu_irq,
			irq_flags, dev_name(&pdev->dev), data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err_thermal;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_tmu_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos tmu attr group");

	mutex_lock(&data->lock);
	list_add_tail(&data->node, &dtm_dev_list);
	num_of_devices++;
	mutex_unlock(&data->lock);

	if (list_is_singular(&dtm_dev_list)) {
		exynos_thermal_create_debugfs();

		if (data->hotplug_enable) {
			kthread_init_worker(&hotplug_worker);
			thread = kthread_create(kthread_worker_fn, &hotplug_worker,
					"thermal_hotplug_kworker");
			kthread_bind(thread, 0);
			sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
			wake_up_process(thread);
		}

		kobj = kobject_create_and_add("exynos-thermal", kernel_kobj);
		sysfs_create_bin_file(kobj, &thermal_log_bin_attr);
		sysfs_create_file(kobj, &thermal_status_attr.attr);
	}

	if (data->use_pi_thermal) {
		reset_pi_trips(data);
		reset_pi_params(data);
		start_pi_polling(data, 0);
	}

	if (!IS_ERR(data->tzd))
		thermal_zone_device_enable(data->tzd);

	data->nb.notifier_call = exynos_tmu_pm_notify;
	register_pm_notifier(&data->nb);

#if defined(CONFIG_MALI_DEBUG_KERNEL_SYSFS)
	if (data->id == EXYNOS_GPU_THERMAL_ZONE_ID)
		gpu_thermal_data = data;
#endif

#if IS_ENABLED(CONFIG_ISP_THERMAL)
	if (!strncmp(data->tmu_name, "ISP", 3))
		exynos_isp_cooling_init();
#endif

	if (data->boost_param.use_boost) {
		__exynos_tmu_set_boost_mode(data, BOOST_MODE, ktime_get());
		__exynos_tmu_set_boost_mode(data, BOOT_MODE, ktime_get());
	}

	exynos_tmu_control(pdev, true);
#if IS_ENABLED(CONFIG_SEC_PM)
	exynos_tmu_sec_pm_init();
#endif

	return 0;

err_thermal:
	thermal_zone_of_sensor_unregister(&pdev->dev, data->tzd);
err_sensor:
	return ret;
}

static int exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tzd = data->tzd;
	struct exynos_tmu_data *devnode;

	thermal_zone_of_sensor_unregister(&pdev->dev, tzd);
	exynos_tmu_control(pdev, false);

	mutex_lock(&data->lock);
	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->id == data->id) {
			list_del(&devnode->node);
			num_of_devices--;
			break;
		}
	}
	mutex_unlock(&data->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_SEC_PM)
static void exynos_tmu_shutdown(struct platform_device *pdev)
{
	if (!tmu_log_work_canceled) {
		tmu_log_work_canceled = 1;
		cancel_delayed_work_sync(&tmu_log_work);
		pr_info("%s: cancel tmu_log_work\n", __func__);
		exynos_tmu_show_curr_temp();
	}
}
#else
static void exynos_tmu_shutdown(struct platform_device *pdev) {}
#endif /* CONFIG_SEC_PM */

#ifdef CONFIG_PM_SLEEP
static int exynos_tmu_suspend(struct device *dev)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	suspended_count++;
	disable_irq(data->irq);

	if (data->hotplug_enable)
		kthread_flush_work(&data->hotplug_work);
	kthread_flush_work(&data->irq_work);

	cp_call_mode = is_aud_on() && cap.acpm_irq;
	if (cp_call_mode) {
		if (suspended_count == num_of_devices) {
			exynos_acpm_tmu_set_cp_call();
			pr_info("%s: TMU suspend w/ AUD-on\n", __func__);
		}
	} else {
		exynos_tmu_control(pdev, false);
		if (suspended_count == num_of_devices) {
			exynos_acpm_tmu_set_suspend(false);
			pr_info("%s: TMU suspend w/ AUD-off\n", __func__);
		}
	}
#else
	exynos_tmu_control(to_platform_device(dev), false);
#endif

	dev->power.must_resume = true;

	return 0;
}

static int exynos_tmu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cpumask mask;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int temp, stat;

	if (suspended_count == num_of_devices)
		exynos_acpm_tmu_set_resume();

	if (!cp_call_mode) {
		exynos_tmu_control(pdev, true);
	}

	exynos_acpm_tmu_set_read_temp(data->id, &temp, &stat, NULL);

	pr_info("%s: thermal zone %d temp %d stat %d\n",
			__func__, data->tzd->id, temp, stat);

	enable_irq(data->irq);
	suspended_count--;

	if (!suspended_count)
		pr_info("%s: TMU resume complete\n", __func__);
#else
	exynos_tmu_control(pdev, true);
#endif

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(data->thermal_worker.task, &mask);

	return 0;
}

static SIMPLE_DEV_PM_OPS(exynos_tmu_pm,
			 exynos_tmu_suspend, exynos_tmu_resume);
#define EXYNOS_TMU_PM	(&exynos_tmu_pm)
#else
#define EXYNOS_TMU_PM	NULL
#endif

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.pm     = EXYNOS_TMU_PM,
		.of_match_table = exynos_tmu_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_tmu_probe,
	.remove	= exynos_tmu_remove,
	.shutdown = exynos_tmu_shutdown,
};

module_platform_driver(exynos_tmu_driver);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
static void exynos_acpm_tmu_test_cp_call(bool mode)
{
	struct exynos_tmu_data *devnode;

	if (mode) {
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			disable_irq(devnode->irq);
		}
		exynos_acpm_tmu_set_cp_call();
	} else {
		exynos_acpm_tmu_set_resume();
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			enable_irq(devnode->irq);
		}
	}
}

static int emul_call_get(void *data, unsigned long long *val)
{
	*val = exynos_acpm_tmu_is_test_mode();

	return 0;
}

static int emul_call_set(void *data, unsigned long long val)
{
	int status = exynos_acpm_tmu_is_test_mode();

	if ((val == 0 || val == 1) && (val != status)) {
		exynos_acpm_tmu_set_test_mode(val);
		exynos_acpm_tmu_test_cp_call(val);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(emul_call_fops, emul_call_get, emul_call_set, "%llu\n");

static int log_print_set(void *data, unsigned long long val)
{
	if (val == 0 || val == 1)
		exynos_acpm_tmu_log(val);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(log_print_fops, NULL, log_print_set, "%llu\n");

static ssize_t ipc_dump1_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;
	char buf[48];
	ssize_t ret;

	exynos_acpm_tmu_ipc_dump(0, data.dump);

	ret = snprintf(buf, sizeof(buf), "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t ipc_dump2_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;
	char buf[48];
	ssize_t ret;

	exynos_acpm_tmu_ipc_dump(EXYNOS_GPU_THERMAL_ZONE_ID, data.dump);

	ret = snprintf(buf, sizeof(buf), "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);

}

static const struct file_operations ipc_dump1_fops = {
	.open = simple_open,
	.read = ipc_dump1_read,
	.llseek = default_llseek,
};

static const struct file_operations ipc_dump2_fops = {
	.open = simple_open,
	.read = ipc_dump2_read,
	.llseek = default_llseek,
};

#endif

static struct dentry *debugfs_root;

static int exynos_thermal_create_debugfs(void)
{
	debugfs_root = debugfs_create_dir("exynos-thermal", NULL);
	if (!debugfs_root) {
		pr_err("Failed to create exynos thermal debugfs\n");
		return 0;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ACPM_THERMAL)
	debugfs_create_file("emul_call", 0644, debugfs_root, NULL, &emul_call_fops);
	debugfs_create_file("log_print", 0644, debugfs_root, NULL, &log_print_fops);
	debugfs_create_file("ipc_dump1", 0644, debugfs_root, NULL, &ipc_dump1_fops);
	debugfs_create_file("ipc_dump2", 0644, debugfs_root, NULL, &ipc_dump2_fops);
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_SEC_PM)
#define NR_THERMAL_SENSOR_MAX	10

static int tmu_sec_pm_init_done;

static ssize_t exynos_tmu_get_curr_temp_all(char *buf)
{
	struct exynos_tmu_data *data;
	int temp[NR_THERMAL_SENSOR_MAX] = {0, };
	int i, id_max = 0;
	ssize_t ret = 0;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (data->id < NR_THERMAL_SENSOR_MAX) {
			exynos_get_temp(data, &temp[data->id]);
			temp[data->id] /= 1000;

			if (id_max < data->id)
				id_max = data->id;
		} else {
			pr_err("%s: id:%d %s\n", __func__, data->id,
					data->tmu_name);
			continue;
		}
	}

	for (i = 0; i <= id_max; i++)
		ret += snprintf(buf + ret, sizeof(buf), "%d,", temp[i]);

	sprintf(buf + ret - 1, "\n");
	pr_err("%s: %s", __func__, buf);

	return ret;
}

static void exynos_tmu_show_curr_temp(void)
{
	char buf[32];

	exynos_tmu_get_curr_temp_all(buf);
}

static void exynos_tmu_show_curr_temp_work(struct work_struct *work)
{
	char buf[32];

	exynos_tmu_get_curr_temp_all(buf);

	schedule_delayed_work(&tmu_log_work, TMU_LOG_PERIOD * HZ);
}

static ssize_t exynos_tmu_curr_temp(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return exynos_tmu_get_curr_temp_all(buf);
}

static DEVICE_ATTR(curr_temp, 0444, exynos_tmu_curr_temp, NULL);

static struct attribute *exynos_tmu_sec_pm_attributes[] = {
	&dev_attr_curr_temp.attr,
	NULL
};

static const struct attribute_group exynos_tmu_sec_pm_attr_grp = {
	.attrs = exynos_tmu_sec_pm_attributes,
};

static int exynos_tmu_sec_pm_init(void)
{
	int ret = 0;
	struct device *dev;

	if (tmu_sec_pm_init_done)
		return 0;

	dev = sec_device_create(NULL, "exynos_tmu");

	if (IS_ERR(dev)) {
		pr_err("%s: failed to create device\n", __func__);
		return PTR_ERR(dev);
	}

	ret = sysfs_create_group(&dev->kobj, &exynos_tmu_sec_pm_attr_grp);
	if (ret) {
		pr_err("%s: failed to create sysfs group(%d)\n", __func__, ret);
		goto err_create_sysfs;
	}

	schedule_delayed_work(&tmu_log_work, 60 * HZ);
	tmu_sec_pm_init_done = 1;

	return ret;

err_create_sysfs:
	sec_device_destroy(dev->devt);

	return ret;
}
#endif /* CONFIG_SEC_PM */

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_LICENSE("GPL");
