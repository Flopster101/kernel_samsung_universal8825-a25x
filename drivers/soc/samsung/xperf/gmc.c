/*
 * General Momentum Chaser (GMC)
 * Jungwook Kim <jwook1.kim@samsung.com>
 */

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>
#include <linux/ems.h>
#include "../../../kernel/sched/sched.h"

#include <soc/samsung/cal-if.h>
#include <soc/samsung/gpu_cooling.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/xperf.h>
#include "../../../thermal/samsung/exynos_tmu.h"
#include "xperf.h"

static const char *prefix = "gmc";

static int is_running;

struct ctrl {
	int boost_cnt;
	int boost_step;
	int cpu_maxlock[MAX_CLUSTER];
	int gpu_maxlock;
};

struct cond {
	int gpu_fps_thd;
	int gpu_freq_thd;
	int gpu_temp_thd;
	int cpu_temp_thd[MAX_CLUSTER];
};

struct type {
	bool enable;
	bool status;

	/* control knob */
	struct ctrl knob;

	/* trigger & release condition */
	struct cond trigger;
	struct cond release;
	int fps_drop_thd;
	int throttle;

	/* timer */
	u64 cur_timer;
	u64 set_timer;
	u64 reset_timer;

	/* util fingerprint */
	int cpu_util_lthd[MAX_CLUSTER];
	int cpu_util_hthd[MAX_CLUSTER];
};

static struct {
	u32 run;
	u32 debug;
	u32 bind_cpu;
	u32 polling_ms;

	int type_cnt;
	struct type *types;
} gmc;

#define for_each_type(type) \
	for ((type) = 0; (type) < gmc.type_cnt; (type)++)

/******************************************************************************
 * monitor functions                                                          *
 ******************************************************************************/
#define FPS_WINDOW 5

static int fps_idx;
static int fps_arr[FPS_WINDOW];

static int fps;
static u64 start_frame_cnt;
static u64 end_frame_cnt;
static void (*get_frame_cnt)(u64 *cnt, ktime_t *time);

void exynos_gmc_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	get_frame_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_gmc_register_frame_cnt);

static void monitor_fps(void)
{
	ktime_t time;
	u64 frame_cnt;

	if (!get_frame_cnt)
		return;

	/* update fps window */
	fps_arr[fps_idx] = fps;
	fps_idx = (fps_idx + 1) % FPS_WINDOW;

	start_frame_cnt = end_frame_cnt;
	get_frame_cnt(&end_frame_cnt, &time);

	/* update fps */
	frame_cnt = end_frame_cnt - start_frame_cnt;
	fps = frame_cnt * 1000 / gmc.polling_ms;

	if (gmc.debug)
		pr_info("[%s] mon gpu freq %d fps %d\n", prefix, gpu_dvfs_get_cur_clock(), fps);
}

/******************************************************************************
 * helper functions                                                           *
 ******************************************************************************/
#define NORMAL_MODE 0
#define HIGH_MODE 6
#define DEFAULT_LEVEL 0

static const char *gpu_tz_names;
static const char *cpu_tz_names[MAX_CLUSTER];

static struct freq_qos_request fqos[NUM_OF_GMC_TYPE][MAX_CLUSTER];
static struct exynos_pm_qos_request gqos[NUM_OF_GMC_TYPE];

static bool gpu_hotter_than(int thd, int type)
{
	int temp;

	thermal_zone_get_temp(thermal_zone_get_zone_by_name(gpu_tz_names), &temp);

	if (gmc.debug)
		pr_info("[%s] type%d gpu temp %d temp-thd %d\n", prefix, type, temp, thd);

	return (temp > thd);
}

static bool gpu_clock_higher_than(int thd, int type)
{
	int freq = gpu_dvfs_get_cur_clock();

	if (gmc.debug)
		pr_info("[%s] type%d gpu freq %d freq-thd %d\n", prefix, type, freq, thd);

	return (freq > thd);
}

static bool gpu_fps_higher_than(int thd, int type)
{
	if (gmc.debug)
		pr_info("[%s] type%d gpu fps %d fps-thd %d\n", prefix, type, fps, thd);

	return (fps > thd);
}

static bool gpu_fps_sudden_drop(int type)
{
	int i, sum = 0, avg = 0;
	struct type *tp = &gmc.types[type];

	for (i = 0; i < FPS_WINDOW; i++)
		sum += fps_arr[i];
	avg = sum / FPS_WINDOW;

	if (gmc.debug)
		pr_info("[%s] type%d gpu drop %d drop-thd %d\n",
			prefix, type, avg - fps, tp->fps_drop_thd);

	return (avg - fps) > tp->fps_drop_thd;
}

static bool all_cpu_hotter_than(int *thd, int type)
{
	int cluster;
	int temp[MAX_CLUSTER];
	struct thermal_zone_device *tz;

	for_each_cluster(cluster) {
		tz = thermal_zone_get_zone_by_name(cpu_tz_names[cluster]);
		thermal_zone_get_temp(tz, &temp[cluster]);

		if (gmc.debug)
			pr_info("[%s] type%d cluster%d temp %d temp-thd %d\n",
				prefix, type, cluster, temp[cluster], thd[cluster]);

		if (temp[cluster] <= thd[cluster])
			return false;
	}

	return true;
}

static bool is_supported_mode(void)
{
	return ((emstune_get_cur_mode() == EMSTUNE_NORMAL_MODE) ||
		(emstune_get_cur_mode() == EMSTUNE_HIGH_MODE)) &&
		(emstune_get_cur_level() == EMSTUNE_DEFAULT_LEVEL);
}

static bool match_util_finger_print(int type)
{
	int cpu, cluster;
	struct type *tp = &gmc.types[type];

	for_each_cluster(cluster) {
		int sum = 0, avg = 0;
		int cpu_num = cpumask_weight(cls[cluster].siblings);

		if (!cpu_num)
			return false;

		for_each_cpu(cpu, cls[cluster].siblings)
			sum = cpu_util_avgs[cpu];

		avg = sum / cpu_num;

		if (gmc.debug)
			pr_info("[%s] type%d cluster%d sum %d avg %d cpu-util-thd %d~%d\n",
				prefix, type, cluster, sum, avg,
				tp->cpu_util_lthd[cluster], tp->cpu_util_hthd[cluster]);

		if (avg < tp->cpu_util_lthd[cluster] || avg > tp->cpu_util_hthd[cluster])
			return false;
	}

	return true;
}

/******************************************************************************
 * update type status                                                         *
 ******************************************************************************/
static bool set_timer_expired(int type)
{
	struct type *tp = &gmc.types[type];

	if (gmc.debug > 1)
		pr_info("[%s] type%d cur-timer %llu set-timer %llu\n",
			prefix, type, tp->cur_timer, tp->set_timer);

	return tp->cur_timer >= tp->set_timer;
}

static bool reset_timer_expired(int type)
{
	struct type *tp = &gmc.types[type];

	if (gmc.debug > 1)
		pr_info("[%s] type%d cur-timer %llu reset-timer %llu\n",
			prefix, type, tp->cur_timer, tp->reset_timer);

	return tp->cur_timer >= tp->reset_timer;
}

/*
 * Returns true if all conditions are met
 */
static bool check_cond(struct cond *cd, int type)
{
	if (!gpu_fps_higher_than(cd->gpu_fps_thd, type))
		return false;

	if (!gpu_clock_higher_than(cd->gpu_freq_thd, type))
		return false;

	if (!gpu_hotter_than(cd->gpu_temp_thd, type))
		return false;

	if (!all_cpu_hotter_than(cd->cpu_temp_thd, type))
		return false;

	return true;
}

/*
 * Returns the status, '1' (trigger) or '0' (release).
 * If the value is the same as before, returns '-1'.
 */
static int update_type_status(int type)
{
	bool next;
	struct type *tp = &gmc.types[type];
	struct cond *cnd = tp->status ? &tp->release : &tp->trigger;

	next = check_cond(cnd, type);

	/* If the fingerprints do not match,
	 * release next status.
	 */
	if (next && !match_util_finger_print(type))
		next = false;

	/* When the fps drops suddenly,
	 * release next status.
	 */
	if (next && gpu_fps_sudden_drop(type))
		next = false;

	/* If the next status is 1, increment the current timer.
	 * Else decrement the current timer.
	 */
	if (next) {
		tp->cur_timer++;
		if (!tp->status)
			next = set_timer_expired(type);
		else
			next = !reset_timer_expired(type);
	} else
		tp->cur_timer = 0;

	if (tp->status == next)
		return -1;

	return next;
}

static void reset_control_knob(struct ctrl *kn, int type)
{
	int cluster;

	if (kn->gpu_maxlock >= 0)
		exynos_pm_qos_update_request(&gqos[type], FREQ_QOS_MAX_DEFAULT_VALUE);

	for_each_cluster(cluster)
		if (kn->cpu_maxlock[cluster] >= 0)
			freq_qos_update_request(&fqos[type][cluster], FREQ_QOS_MAX_DEFAULT_VALUE);
}

static void set_control_knob(struct ctrl *kn, int type)
{
	int cluster;

	if (kn->gpu_maxlock >= 0)
		exynos_pm_qos_update_request(&gqos[type], kn->gpu_maxlock);

	for_each_cluster(cluster)
		if (kn->cpu_maxlock[cluster] >= 0)
			freq_qos_update_request(&fqos[type][cluster], kn->cpu_maxlock[cluster]);
}

/*
 * Set the type of control knob.
 */
static void control_type_knob(int type)
{
	int cluster;
	struct type *tp = &gmc.types[type];
	struct ctrl *kn = &tp->knob;
	struct thermal_zone_device *tz;
	struct exynos_tmu_data *td;

	if (tp->status) {
		set_control_knob(kn, type);
		if (tp->throttle) {
			/* Set TMU throttle mode */
			for_each_cluster(cluster) {
				tz = thermal_zone_get_zone_by_name(cpu_tz_names[cluster]);
				td = exynos_tmu_get_data_from_tz(tz);
				exynos_tmu_set_boost_mode(td, THROTTLE_MODE, false);
			}
			tz = thermal_zone_get_zone_by_name(gpu_tz_names);
			td = exynos_tmu_get_data_from_tz(tz);
			exynos_tmu_set_boost_mode(td, THROTTLE_MODE, false);

			if (gmc.debug)
				pr_info("[%s] type%d tmu throttle\n", prefix, type);
		}
	} else
		reset_control_knob(kn, type);
}

/******************************************************************************
 * gmc main thread                                                            *
 ******************************************************************************/
static int gmc_thread(void *data)
{
	int next, type;

	if (is_running)
		return 0;

	is_running = 1;

	while (is_running) {
		monitor_fps();

		for_each_type(type) {
			struct type *tp = &gmc.types[type];

			if (!tp->enable)
				continue;

			/* When the current mode is not supported,
			 * set type status to false and reset control knob.
			 */
			if (!is_supported_mode()) {
				if (tp->status) {
					tp->status = false;
					reset_control_knob(&tp->knob, type);
				}
				continue;
			}

			next = update_type_status(type);

			/* If there is no update, do nothing. */
			if (next < 0)
				continue;

			tp->status = next;
			control_type_knob(type);

			if (gmc.debug)
				pr_info("[%s] type%d %s\n",
					prefix, type, tp->status ? "trigger" : "release");
		}

		msleep(gmc.polling_ms);
	}

	return 0;
}

static void gmc_stop(void)
{
	int cluster, type;

	for_each_type(type)
		exynos_pm_qos_remove_request(&gqos[type]);

	for_each_cluster(cluster)
		for_each_type(type)
			freq_qos_tracer_remove_request(&fqos[type][cluster]);

	is_running = 0;
}

static struct task_struct *task;
static void gmc_start(void)
{
	int cpu, cluster, type;
	struct cpufreq_policy *policy;

	if (is_running)
		return;

	for_each_type(type)
		exynos_pm_qos_add_request(&gqos[type],
					  PM_QOS_GPU_THROUGHPUT_MAX,
					  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);

	for_each_cluster(cluster) {
		cpu = cls[cluster].first_cpu;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			gmc_stop();
			return;
		}

		for_each_type(type)
			freq_qos_tracer_add_request(&policy->constraints,
						    &fqos[type][cluster],
						    FREQ_QOS_MAX,
						    FREQ_QOS_MAX_DEFAULT_VALUE);

		cpufreq_cpu_put(policy);
	}

	/* start gmc main thread */
	task = kthread_create(gmc_thread, NULL, "gmc%u", 0);
	kthread_bind(task, gmc.bind_cpu);
	wake_up_process(task);
	return;
}

/******************************************************************************
 * sysfs interface                                                            *
 ******************************************************************************/
#define DEF_NODE_CTRL_RW(name) \
	static ssize_t name##_show(struct kobject *k, struct kobj_attribute *attr, char *buf) \
	{ \
		int type; \
		int ret = 0; \
		for_each_type(type) \
			ret += sprintf(buf + ret, "[type%d] %d\n", type, gmc.types[type].knob.name); \
		ret += sprintf(buf + ret, "\n# echo <type> <value> > node\n"); \
		return ret; } \
	static ssize_t name##_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) \
	{ \
		int type = -1; \
		int val; \
		if (sscanf(buf, "%d %d", &type, &val) != 2) \
			return -EINVAL; \
		if (type < 0 || type >= gmc.type_cnt) \
			return -EINVAL; \
		gmc.types[type].knob.name = val; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR_RW(name)

DEF_NODE_CTRL_RW(boost_cnt);
DEF_NODE_CTRL_RW(boost_step);
DEF_NODE_CTRL_RW(gpu_maxlock);

static ssize_t cpu_maxlock_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int type, cluster, ret = 0;

	for_each_type(type) {
		ret += sprintf(buf + ret, "[type%d]\n", type);
		for_each_cluster(cluster)
			ret += sprintf(buf + ret, "cluster%d=%d\n",
				       cluster,
				       gmc.types[type].knob.cpu_maxlock[cluster]);
	}
	ret += sprintf(buf + ret, "\n# echo <cluster> <freq> > cpu_maxlock\n");
	return ret;
}

static ssize_t cpu_maxlock_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int type, cluster, val;

	if (sscanf(buf, "%d %d %d", &type, &cluster, &val) != 3)
		return -EINVAL;
	if (type < 0 || type >= gmc.type_cnt)
		return -EINVAL;
	if (cluster < 0 || cluster >= MAX_CLUSTER)
		return -EINVAL;

	gmc.types[type].knob.cpu_maxlock[cluster] = val;
	return count;
}
static struct kobj_attribute cpu_maxlock_attr = __ATTR_RW(cpu_maxlock);

#define DEF_NODE_GMC_RW(name) \
	static ssize_t name##_show(struct kobject *k, struct kobj_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		ret += sprintf(buf + ret, "%u\n", gmc.name); \
		return ret; } \
	static ssize_t name##_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) \
	{ \
		if (kstrtouint(buf, 10, &gmc.name)) \
			return -EINVAL; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR_RW(name)

DEF_NODE_GMC_RW(debug);
DEF_NODE_GMC_RW(bind_cpu);
DEF_NODE_GMC_RW(polling_ms);

#define DEF_NODE_COND_RW(name) \
	static ssize_t name##_show(struct kobject *k, struct kobj_attribute *attr, char *buf) \
	{ \
		int type; \
		int ret = 0; \
		for_each_type(type) { \
			ret += sprintf(buf + ret, "[type%d] trigger=%-8d release=%-8d\n", \
				       type, \
				       gmc.types[type].trigger.name, \
				       gmc.types[type].release.name); \
		} \
		ret += sprintf(buf + ret, "\n# echo <type> <trigger> <release> > node\n"); \
		return ret; } \
	static ssize_t name##_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) \
	{ \
		int type = -1; \
		int val1, val2; \
		if (sscanf(buf, "%d %d %d", &type, &val1, &val2) != 3) \
			return -EINVAL; \
		if (type < 0 || type >= gmc.type_cnt) \
			return -EINVAL; \
		gmc.types[type].trigger.name = val1; \
		gmc.types[type].release.name = val2; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR_RW(name)

DEF_NODE_COND_RW(gpu_fps_thd);
DEF_NODE_COND_RW(gpu_freq_thd);
DEF_NODE_COND_RW(gpu_temp_thd);

static ssize_t cpu_temp_thd_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int type, cluster, ret = 0;

	for_each_type(type) {
		ret += sprintf(buf + ret, "[type%d]\n", type);
		for_each_cluster(cluster)
			ret += sprintf(buf + ret, "cluster%d trigger=%-8d release=%-8d\n",
				       cluster,
				       gmc.types[type].trigger.cpu_temp_thd[cluster],
				       gmc.types[type].release.cpu_temp_thd[cluster]);
	}
	ret += sprintf(buf + ret, "\n# echo <cluster> <freq> > cpu_maxlock\n");
	return ret;
}

static ssize_t cpu_temp_thd_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int type, cluster;
	int val1, val2;

	if (sscanf(buf, "%d %d %d %d", &type, &cluster, &val1, &val2) != 4)
		return -EINVAL;
	if (type < 0 || type >= gmc.type_cnt)
		return -EINVAL;
	if (cluster < 0 || cluster >= MAX_CLUSTER)
		return -EINVAL;

	gmc.types[type].trigger.cpu_temp_thd[cluster] = val1;
	gmc.types[type].release.cpu_temp_thd[cluster] = val2;
	return count;
}
static struct kobj_attribute cpu_temp_thd_attr = __ATTR_RW(cpu_temp_thd);

static ssize_t run_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;

	ret += sprintf(buf + ret, "%u\n", gmc.run);
	return ret;
}
static ssize_t run_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (kstrtouint(buf, 10, &gmc.run))
		return -EINVAL;
	if (gmc.run)
		gmc_start();
	else
		gmc_stop();
	return count;
}
static struct kobj_attribute run_attr = __ATTR_RW(run);

static struct attribute *gmc_attrs[] = {
	&run_attr.attr,
	&debug_attr.attr,
	&bind_cpu_attr.attr,
	&polling_ms_attr.attr,
	&gpu_fps_thd_attr.attr,
	&gpu_freq_thd_attr.attr,
	&gpu_temp_thd_attr.attr,
	&cpu_temp_thd_attr.attr,
	&boost_step_attr.attr,
	&boost_cnt_attr.attr,
	&cpu_maxlock_attr.attr,
	&gpu_maxlock_attr.attr,
	NULL
};

static struct attribute_group gmc_group = {
	.attrs = gmc_attrs,
};

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
static struct kobject *gmc_kobj;

int init_cond(struct device_node *dn, struct cond *con, int init)
{
	int cluster, ret = 0;

	if (!dn)
		ret = -ENODATA;

	if (ret || of_property_read_u32(dn, "gpu-fps", &con->gpu_fps_thd))
		con->gpu_fps_thd = init;

	if (ret || of_property_read_u32(dn, "gpu-freq", &con->gpu_freq_thd))
		con->gpu_freq_thd = init;

	if (ret || of_property_read_u32(dn, "gpu-temp", &con->gpu_temp_thd))
		con->gpu_temp_thd = init;

	if (ret || of_property_read_u32_array(dn, "cpu-temp", con->cpu_temp_thd, cl_cnt))
		for_each_cluster(cluster)
			con->cpu_temp_thd[cluster] = init;

	return ret;
}

void init_type(struct device_node *dn, int type)
{
	int cluster;
	struct type *tp = &gmc.types[type];

	tp->enable = true;

	/* init control knob */
	if (of_property_read_u32(dn, "boost-cnt", &tp->knob.boost_cnt))
		tp->knob.boost_cnt = -ENODATA;

	if (of_property_read_u32(dn, "boost-step", &tp->knob.boost_step))
		tp->knob.boost_step = -ENODATA;

	if (of_property_read_u32_array(dn, "cpu-maxlock", tp->knob.cpu_maxlock, cl_cnt))
		for_each_cluster(cluster)
			tp->knob.cpu_maxlock[cluster] = -ENODATA;

	if (of_property_read_u32(dn, "gpu-maxlock", &tp->knob.gpu_maxlock))
		tp->knob.gpu_maxlock = -ENODATA;

	/* init util fingerprint */
	if (of_property_read_u32_array(dn, "cpu-util-low", tp->cpu_util_lthd, cl_cnt))
		for_each_cluster(cluster)
			tp->cpu_util_lthd[cluster] = 0;

	if (of_property_read_u32_array(dn, "cpu-util-high", tp->cpu_util_hthd, cl_cnt))
		for_each_cluster(cluster)
			tp->cpu_util_hthd[cluster] = 1024;

	/* init condition */
	if (of_property_read_u64(dn, "set-timer", &tp->set_timer))
		tp->set_timer = 0;

	if (of_property_read_u64(dn, "reset-timer", &tp->reset_timer))
		tp->reset_timer = UINT_MAX;

	if (of_property_read_u32(dn, "fps-drop", &tp->fps_drop_thd))
		tp->fps_drop_thd = INT_MAX;
	if (of_property_read_u32(dn, "throttle", &tp->throttle))
		tp->throttle = 0;

	if (init_cond(of_get_child_by_name(dn, "trigger"), &tp->trigger, INT_MAX))
		tp->enable = false;

	if (init_cond(of_get_child_by_name(dn, "release"), &tp->release, -ENODATA))
		tp->enable = false;
}

int dt_init(struct device_node *dn)
{
	int index;
	struct device_node *gmc_node, *type_node;

	gmc_node = of_get_child_by_name(dn, "gmc");
	if (!gmc_node)
		return -EINVAL;

	/* init gmc settings */
	if (of_property_read_u32(gmc_node, "bind-cpu", &gmc.bind_cpu))
		return -ENODATA;
	if (of_property_read_u32(gmc_node, "polling-ms", &gmc.polling_ms))
		return -ENODATA;

	/* init thermal zone */
	if (of_property_read_string_array(gmc_node, "cpu-tz-names", cpu_tz_names, cl_cnt) < 0)
		return -ENODATA;
	if (of_property_read_string(gmc_node, "gpu-tz-names", &gpu_tz_names))
		return -ENODATA;

	gmc.type_cnt =  of_get_child_count(gmc_node);
	if (!gmc.type_cnt)
		return -ENODATA;

	gmc.types = kcalloc(gmc.type_cnt, sizeof(struct type), GFP_KERNEL);
	if (!gmc.types)
		return -ENOMEM;

	index = 0;
	for_each_child_of_node(gmc_node, type_node)
		init_type(type_node, index++);

	gmc.run = 1;
	gmc.debug = 0;

	return 0;
}

int xperf_gmc_init(struct platform_device *pdev, struct kobject *kobj)
{
	int ret = 0;

	ret = dt_init(pdev->dev.of_node);
	if (ret) {
		pr_info("[%s] failed to initialize driver with err=%d\n", prefix, ret);
		return ret;
	}

	gmc_kobj = kobject_create_and_add("gmc", &pdev->dev.kobj);

	if (!gmc_kobj) {
		pr_info("[%s] create node failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	ret = sysfs_create_group(gmc_kobj, &gmc_group);
	if (ret) {
		pr_info("[%s] create group failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	gmc_start();

	return 0;
}
