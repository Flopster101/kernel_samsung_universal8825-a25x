/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_opp.h>
#include <linux/thermal.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include "../sched.h"
#include "ems.h"

#define UNIT_OF_VOLT		50
#define UNIT_OF_TEMP		5


struct field {
	int size;
	unsigned int min;
	unsigned int *list;
};

struct static_power_table {
	struct thermal_zone_device *tz_dev;

	struct field volt;
	struct field temp;

	unsigned long *power;
};

struct constraint {
	unsigned int cpu_freq;
	unsigned int dsu_freq;
};

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
static struct energy_table {
	struct cpumask cpus;

	struct energy_state *states;
	unsigned int nr_states;

	unsigned int mips_mhz;
	unsigned int mips_mhz_orig;
	unsigned int dynamic_coeff;
	unsigned int dynamic_coeff_orig;
	unsigned int static_coeff;

	struct static_power_table *spt;
	struct static_power_table *sl_spt;

	struct cpumask sl_cpus[VENDOR_NR_CPUS];
	struct cpumask pd_cpus;

	unsigned long cur_freq;	/* requested by governor, NOT real frequency */
	unsigned long cur_volt;	/* voltage matched with cur_freq */
	unsigned int cur_index;	/* real current frequency index */

	struct constraint **constraints;
	int nr_constraint;
} __percpu **energy_table;

static struct dsu_energy_table {
	struct energy_state *states;
	unsigned int nr_states;

	unsigned int dynamic_coeff;
	unsigned int static_coeff;

	struct static_power_table *spt;

	struct cpumask pd_cpus;
} *dsu_table;

static const int default_temp = 85;

#define per_cpu_et(cpu)		(*per_cpu_ptr(energy_table, cpu))

static int get_cap_index(struct energy_state *states, int size, unsigned long cap)
{
	int i;

	if (size == 0)
		return -1;

	for (i = 0; i < size; i++)
		if (states[i].capacity >= cap)
			break;

	return min(i, size);
}

static int get_dpower_index(struct energy_state *states, int size, unsigned long dpower)
{
	int i;

	if (size == 0)
		return -1;

	for (i = 0; i < size; i++)
		if (states[i].dynamic_power >= dpower)
			break;

	return min(i, size);
}

static int get_freq_index(struct energy_state *states, int size, unsigned long freq)
{
	int i;

	if (size == 0)
		return -1;

	for (i = 0; i < size; i++)
		if (states[i].frequency >= freq)
			break;

	return min(i, size);
}
/****************************************************************************************
 *					Helper functions				*
 ****************************************************************************************/
static int emstune_index = 0;
static unsigned long get_needed_dsu_freq(struct energy_table *table, unsigned long cpu_freq)
{
	struct constraint *constraint;
	unsigned long dsu_freq = 0;
	int i;

	if (unlikely(table->nr_constraint == 0)
			|| unlikely(table->nr_constraint <= emstune_index))
		return dsu_freq;

	constraint = table->constraints[emstune_index];
	if (unlikely(!constraint))
		return dsu_freq;

	for (i = 0; i < table->nr_states; i++) {
		if (constraint[i].cpu_freq >= cpu_freq)
			dsu_freq = constraint[i].dsu_freq;
	}

	return dsu_freq;
}

static int find_nearest_index(struct field *f, int value, int unit)
{
	int index;

	value = value - f->min + (unit >> 1);
	value = max(value, 0);

	index = value / unit;
	index = min(index, f->size - 1);

	return index;
}

#define of_power(spt, row, col) (spt->power[(row) * spt->temp.size + (col)])
static unsigned long __get_static_power(struct static_power_table *spt,
		unsigned long volt, int temp)
{
	int row, col;

	row = find_nearest_index(&spt->volt, volt, UNIT_OF_VOLT);
	col = find_nearest_index(&spt->temp, temp, UNIT_OF_TEMP);

	/* get static power from pre-calculated table */
	return of_power(spt, row, col);
}

static unsigned long get_static_power(struct static_power_table *spt,
		unsigned long v, unsigned long c, int t)
{
	/*
	 * Pre-calculated static power table does not exist,
	 * calculate static power with coefficient.
	 */
	if (!spt) {
		/* static power = coefficent * voltage^2 */
		unsigned long power = c * v * v;

		do_div(power, 1000000);

		return power;
	}

	return __get_static_power(spt, v, t);
}

static unsigned long get_dynamic_power(unsigned long f, unsigned long v, unsigned long c)
{
	/* dynamic power = coefficent * frequency * voltage^2 */
	unsigned long power;

	f /= 1000;
	power = c * f * v * v;

	/*
	 * f_mhz is more than 3 digits and volt is also more than 3 digits,
	 * so calculated power is more than 9 digits. For convenience of
	 * calculation, divide the value by 10^9.
	 */
	do_div(power, 1000000000);

	return power;
}

static unsigned long compute_sl_energy(struct energy_table *table,
		struct cpumask *cpus, struct energy_state *states)
{
	struct energy_state *state;
	unsigned long sp, e;

	if (!table->sl_spt)
		return 0;

	state = &states[cpumask_any(cpus)];
	sp = __get_static_power(table->sl_spt, state->voltage, state->temperature);

	e = sp << SCHED_CAPACITY_SHIFT;

	trace_ems_compute_sl_energy(cpus, sp, e);

	return e;
}

static unsigned long compute_dsu_energy(struct energy_state *state)
{
	unsigned long dp, sp, e;

	if (unlikely(!dsu_table))
		return 0;

	dp = get_dynamic_power(state->frequency, state->voltage, dsu_table->dynamic_coeff);
	sp = get_static_power(dsu_table->spt, state->voltage, dsu_table->static_coeff,
			default_temp);

	e = (dp + sp) << SCHED_CAPACITY_SHIFT;

	trace_ems_compute_dsu_energy(state->frequency, state->voltage, dp, sp, e);

	return e;
}

static unsigned long __compute_cpu_energy(struct energy_state *state, int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);
	unsigned long dp, sp, e;

	dp = get_dynamic_power(state->frequency, state->voltage, table->dynamic_coeff);
	sp = get_static_power(table->spt, state->voltage, table->static_coeff, state->temperature);
	e = ((dp + sp) << SCHED_CAPACITY_SHIFT) * state->util / state->capacity;
	e = e * 100 / state->weight;

	trace_ems_compute_energy(cpu, state->util, state->capacity,
			state->frequency, state->voltage, state->temperature, dp, sp, e);

	return e;
}

static unsigned long compute_cpu_energy(const struct cpumask *cpus, struct energy_state *states,
		int target_cpu, struct energy_backup *backup)
{
	unsigned long energy = 0;
	int cpu;

	for_each_cpu(cpu, cpus) {
		struct energy_table *table = per_cpu_et(cpu);
		struct energy_state *state = &states[cpu];
		struct cpumask mask;

		cpumask_and(&mask, &table->sl_cpus[cpu], cpus);
		if (cpu == cpumask_first(&mask))
			energy += compute_sl_energy(table, &table->sl_cpus[cpu], states);

		/*
		 * If this cpu is target or backup is null, do not use backup energy.
		 */
		if (cpu == target_cpu || !backup) {
			energy += __compute_cpu_energy(state, cpu);
			continue;
		}

		if (backup[cpu].voltage != state->voltage) {
			backup[cpu].energy = __compute_cpu_energy(state, cpu);
			backup[cpu].voltage = state->voltage;
		}

		energy += backup[cpu].energy;
	}

	return energy;
}

static void update_energy_state(const struct cpumask *cpus,
		struct energy_state *states, struct energy_state *dsu_state)
{
	struct energy_table *table;
	unsigned long cpu_volt, cpu_freq;
	unsigned long dsu_volt, dsu_freq = 0;
	int cpu, rep_cpu, index;

	if (unlikely(!dsu_table))
		return;

	/* 1. Find DSU frequency */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		cpu_freq = states[cpu].frequency ? states[cpu].frequency : table->cur_freq;
		dsu_freq = max(dsu_freq, get_needed_dsu_freq(table, cpu_freq));
	}

	/* 2. Find DSU voltage */
	index = get_freq_index(dsu_table->states, dsu_table->nr_states, dsu_freq);
	if (index < 0)
		return;
	dsu_volt = dsu_table->states[index].voltage;

	/* 3. Apply DSU voltage to CPU */
	for_each_cpu_and(cpu, cpus, &dsu_table->pd_cpus)
		states[cpu].voltage = max(states[cpu].voltage, dsu_volt);

	/* 4. Apply CPU voltage of same pd-cpus */
	for_each_cpu(cpu, cpus) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (!cpumask_weight(&table->pd_cpus))
			continue;

		for_each_cpu(rep_cpu, &table->pd_cpus) {
			cpu_volt = states[rep_cpu].voltage ?
				states[rep_cpu].voltage : per_cpu_et(rep_cpu)->cur_volt;
			cpu_volt = max(states[cpu].voltage, cpu_volt);
		}

		for_each_cpu(rep_cpu, &table->cpus)
			states[rep_cpu].voltage = cpu_volt;
	}

	/* 5. Update temperature info */
	for_each_cpu(cpu, cpus) {
		int temp;

		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (table->spt && table->spt->tz_dev)
			temp = table->spt->tz_dev->temperature / 1000;
		else
			temp = default_temp;

		for_each_cpu_and(rep_cpu, cpus, &table->cpus)
			states[rep_cpu].temperature = temp;
	}

	/* If dsu_state is NULL, don't need to update dsu_state */
	if (!dsu_state)
		return;

	/* 6. Apply CPU voltage to DSU */
	for_each_cpu(cpu, &dsu_table->pd_cpus) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		cpu_volt = states[cpu].voltage ? states[cpu].voltage : table->cur_volt;
		dsu_volt = max(dsu_volt, cpu_volt);
	}

	dsu_state->frequency = dsu_freq;
	dsu_state->voltage = dsu_volt;
}

/****************************************************************************************
 *					Extern APIs					*
 ****************************************************************************************/
unsigned int et_cur_freq_idx(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (unlikely(!table))
		return 0;

	if (unlikely(table->cur_index < 0))
		return 0;

	return table->cur_index;
}

unsigned long et_cur_cap(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (unlikely(!table))
		return 0;

	if (unlikely(table->cur_index < 0))
		return 0;

	return table->states[table->cur_index].capacity;
}

unsigned long et_max_cap(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[table->nr_states - 1].capacity;
}

unsigned long et_max_dpower(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[table->nr_states - 1].dynamic_power;
}

unsigned long et_min_dpower(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[0].dynamic_power;
}

unsigned long et_freq_to_cap(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index = get_freq_index(table->states, table->nr_states, freq);

	if (index < 0)
		return 0;

	return table->states[index].capacity;
}

unsigned long et_freq_to_dpower(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index = get_freq_index(table->states, table->nr_states, freq);

	if (index < 0)
		return 0;

	return table->states[index].dynamic_power;
}

unsigned long et_freq_to_spower(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index = get_freq_index(table->states, table->nr_states, freq);

	if (index < 0)
		return 0;

	return table->states[index].static_power;
}
EXPORT_SYMBOL_GPL(et_freq_to_spower);

unsigned long et_dpower_to_cap(int cpu, unsigned long dpower)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index = get_dpower_index(table->states, table->nr_states, dpower);

	if (index < 0)
		return 0;

	return table->states[index].capacity;
}

void et_get_orig_state(int cpu, unsigned long freq, struct energy_state *state)
{
	struct energy_table *table = per_cpu_et(cpu);
	struct energy_state *next_state;
	int index;

	index = get_freq_index(table->states, table->nr_states, freq);
	if (index < 0)
		return;

	next_state = &table->states[index];

	state->frequency = next_state->frequency;
	state->capacity = next_state->capacity;
	state->dynamic_power = next_state->dynamic_power;
	state->static_power = next_state->static_power;
	state->voltage = next_state->voltage;
}

void et_update_freq(int cpu, unsigned long cpu_freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index;

	index = get_freq_index(table->states, table->nr_states, cpu_freq);
	if (index < 0)
		return;

	table->cur_freq = cpu_freq;
	table->cur_volt = table->states[index].voltage;
}

void et_fill_energy_state(struct tp_env *env, struct cpumask *cpus,
		struct energy_state *states, unsigned long capacity, int dst_cpu)
{
	int index, cpu = cpumask_any(cpus);
	struct energy_table *table = per_cpu_et(cpu);

	index = get_cap_index(table->states, table->nr_states, capacity);
	if (index < 0)
		return;

	for_each_cpu(cpu, cpus) {
		states[cpu].capacity = table->states[index].capacity;
		states[cpu].frequency = table->states[index].frequency;
		states[cpu].voltage = table->states[index].voltage;
		states[cpu].dynamic_power = table->states[index].dynamic_power;
		states[cpu].static_power = table->states[index].static_power;

		if (env) {
			if (cpu == dst_cpu)
				states[cpu].util = env->cpu_stat[cpu].util_with;
			else
				states[cpu].util = env->cpu_stat[cpu].util_wo;
			states[cpu].weight = env->weight[cpu];
		} else {
			states[cpu].util = 0;
			states[cpu].weight = 100;
		}
	}
}

unsigned long et_compute_cpu_energy(const struct cpumask *cpus, struct energy_state *states)
{
	update_energy_state(cpus, states, NULL);

	return compute_cpu_energy(cpus, states, -1, NULL);
}

unsigned long et_compute_system_energy(const struct list_head *csd_head,
		struct energy_state *states, int target_cpu, struct energy_backup *backup)
{
	struct energy_state dsu_state;
	struct cs_domain *csd;
	unsigned long energy = 0;

	update_energy_state(cpu_possible_mask, states, &dsu_state);

	energy += compute_dsu_energy(&dsu_state);

	list_for_each_entry(csd, csd_head, list)
		energy += compute_cpu_energy(&csd->cpus, states, target_cpu, backup);

	return energy;
}

/****************************************************************************************
 *					CPUFREQ Change VH				*
 ****************************************************************************************/
void et_arch_set_freq_scale(const struct cpumask *cpus,
		unsigned long freq,  unsigned long max, unsigned long *scale)
{
	struct energy_table *table = per_cpu_et(cpumask_first(cpus));
	int index = get_freq_index(table->states, table->nr_states, freq);

	if (index > -1)
		table->cur_index = index;
}

/****************************************************************************************
 *					Notifier Call					*
 ****************************************************************************************/
static void update_dynamic_power(struct cpumask *cpus)
{
	int cpu;

	for_each_cpu(cpu, cpus) {
		struct energy_table *table = per_cpu_et(cpu);
		int i;

		if (unlikely(!table) || !table->nr_states)
			continue;

		for (i = 0; i < table->nr_states; i++) {
			struct energy_state *state = &table->states[i];

			state->dynamic_power = get_dynamic_power(state->frequency, state->voltage,
					table->dynamic_coeff);
		}
	}
}

static void update_capacity(void)
{
	struct energy_table *table;
	unsigned long max_mips = 0;
	int cpu, i;

	/*
	 * Since capacity of energy table is relative value, previously
	 * configured capacity can be reconfigurated according to maximum mips
	 * whenever new energy table is built.
	 *
	 * Find max mips among built energy table first, and calculate or
	 * recalculate capacity.
	 */
	for_each_possible_cpu(cpu) {
		unsigned long max_f, mips;

		table = per_cpu_et(cpu);

		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (unlikely(!table->nr_states))
			continue;

		/* max mips = max_f * mips/mhz */
		max_f = table->states[table->nr_states - 1].frequency;
		mips = max_f * table->mips_mhz;
		if (mips > max_mips)
			max_mips = mips;
	}

	if (!max_mips)
		return;

	/* Fill energy table with capacity */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);

		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (unlikely(!table->nr_states))
			continue;

		for (i = 0; i < table->nr_states; i++) {
			unsigned long capacity;

			/*
			 *     mips(f) = f * mips/mhz
			 * capacity(f) = mips(f) / max_mips * 1024
			 */
			capacity = table->states[i].frequency * table->mips_mhz;
			capacity = (capacity << 10) / max_mips;
			table->states[i].capacity = capacity;
		}

		/* Set CPU scale with max capacity of the CPU */
		per_cpu(cpu_scale, cpu) = table->states[table->nr_states - 1].capacity;
	}
}

static int et_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct emstune_specific_energy_table *emstune_et = &cur_set->specific_energy_table;
	struct cpumask dp_update_cpus;
	bool need_to_update_capacity = false;
	int cpu;

	cpumask_clear(&dp_update_cpus);

	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);
		unsigned int next_mips_mhz;
		unsigned int next_dynamic_coeff;

		/* Check whether dynamic power coefficient is changed */
		next_dynamic_coeff = emstune_et->dynamic_coeff[cpu] ?
			emstune_et->dynamic_coeff[cpu] : table->dynamic_coeff_orig;

		if (table->dynamic_coeff != next_dynamic_coeff) {
			table->dynamic_coeff = next_dynamic_coeff;
			cpumask_set_cpu(cpu, &dp_update_cpus);
		}

		/* Check whether mips per mhz is changed */
		next_mips_mhz = emstune_et->mips_mhz[cpu] ?
			emstune_et->mips_mhz[cpu] : table->mips_mhz_orig;

		if (table->mips_mhz != next_mips_mhz) {
			table->mips_mhz = next_mips_mhz;
			need_to_update_capacity = true;
		}
	}

	if (cpumask_weight(&dp_update_cpus))
		update_dynamic_power(&dp_update_cpus);

	if (need_to_update_capacity)
		update_capacity();

	emstune_index = emstune_cpu_dsu_table_index(v);
	emstune_index = max(emstune_index, 0);

	return NOTIFY_OK;
}

static struct notifier_block et_emstune_notifier = {
	.notifier_call = et_emstune_notifier_call,
};

/****************************************************************************************
 *					SYSFS						*
 ****************************************************************************************/
#define MSG_SIZE 8192

static ssize_t energy_table_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	int cpu, i;
	char *msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	ssize_t count = 0, msg_size;

	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);

		if (unlikely(!table) || cpumask_first(&table->cpus) != cpu)
			continue;

		count += sprintf(msg + count, "[Energy Table: cpu%d]\n", cpu);
		count += sprintf(msg + count,
				"+------------+------------+---------------+---------------+\n"
				"|  frequency |  capacity  | dynamic power |  static power |\n"
				"+------------+------------+---------------+---------------+\n");
		for (i = 0; i < table->nr_states; i++) {
			count += sprintf(msg + count,
					"| %10lu | %10lu | %13lu | %13lu |\n",
					table->states[i].frequency,
					table->states[i].capacity,
					table->states[i].dynamic_power,
					table->states[i].static_power);
		}
		count += sprintf(msg + count,
				"+------------+------------+---------------+---------------+\n\n");
	}

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	kfree(msg);

	return msg_size;
}

static ssize_t static_power_table_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	struct static_power_table *spt;
	int cpu;
	char *msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	ssize_t count = 0, msg_size;

	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);
		int col, row;

		if (unlikely(!table) || cpumask_first(&table->cpus) != cpu)
			continue;

		spt = table->spt;
		if (!spt)
			continue;

		count += sprintf(msg + count, "[Static Power Table: cpu%d]\n", cpu);
		count += sprintf(msg + count, "      ");

		for (col = 0; col < spt->temp.size; col++)
			count += sprintf(msg + count, " %4lu℃", spt->temp.list[col]);
		count += sprintf(msg + count, "\n");

		for (row = 0; row < spt->volt.size; row++) {
			count += sprintf(msg + count, "%4lumV ", spt->volt.list[row]);

			for (col = 0; col < spt->temp.size; col++)
				count += sprintf(msg + count, "%5lu ", of_power(spt, row, col));
			count += sprintf(msg + count, "\n");
		}

		count += sprintf(msg + count, "\n");
	}

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	kfree(msg);

	return msg_size;
}

static ssize_t sl_static_power_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct static_power_table *sl_spt;
	int cpu;
	ssize_t ret = 0;

	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);
		int col, row;

		if (unlikely(!table) || cpumask_first(&table->cpus) != cpu)
			continue;

		sl_spt = table->sl_spt;
		if (!sl_spt)
			continue;

		ret += sprintf(buf + ret, "[Shared Logic Static Power Table: cpu%d]\n", cpu);
		ret += sprintf(buf + ret, "      ");

		for (col = 0; col < sl_spt->temp.size; col++)
			ret += sprintf(buf + ret, " %4lu℃", sl_spt->temp.list[col]);
		ret += sprintf(buf + ret, "\n");

		for (row = 0; row < sl_spt->volt.size; row++) {
			ret += sprintf(buf + ret, "%4lumV ", sl_spt->volt.list[row]);

			for (col = 0; col < sl_spt->temp.size; col++)
				ret += sprintf(buf + ret, "%5lu ", of_power(sl_spt, row, col));
			ret += sprintf(buf + ret, "\n");
		}

		ret += sprintf(buf + ret, "\n");
	}

	return ret;
}

static ssize_t dsu_static_power_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct static_power_table *spt;
	int col, row;
	ssize_t ret = 0;

	if (unlikely(!dsu_table) || !dsu_table->spt)
		return ret;

	spt = dsu_table->spt;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[Static Power Table: DSU]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "      ");

	for (col = 0; col < spt->temp.size; col++)
		ret += snprintf(buf + ret, PAGE_SIZE - ret, " %4lu℃", spt->temp.list[col]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	for (row = 0; row < spt->volt.size; row++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%4lumV ", spt->volt.list[row]);

		for (col = 0; col < spt->temp.size; col++)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5lu ", of_power(spt, row, col));
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
}

static DEVICE_ATTR_RO(sl_static_power_table);
static DEVICE_ATTR_RO(dsu_static_power_table);

static BIN_ATTR(energy_table, 0440, energy_table_read, NULL, 0);
static BIN_ATTR(static_power_table, 0440, static_power_table_read, NULL, 0);

static struct attribute *et_attrs[] = {
	&dev_attr_sl_static_power_table.attr,
	&dev_attr_dsu_static_power_table.attr,
	NULL,
};

static struct bin_attribute *et_bin_attrs[] = {
	&bin_attr_energy_table,
	&bin_attr_static_power_table,
	NULL,
};

static struct attribute_group et_attr_group = {
	.name		= "energy_table",
	.attrs		= et_attrs,
	.bin_attrs	= et_bin_attrs,
};

/****************************************************************************************
 *					Initialize					*
 ****************************************************************************************/
static struct ect_gen_param_table *get_ect_gen_param_table(char *name)
{
	void *gen_block = ect_get_block("GEN");

	if (!gen_block) {
		pr_err("Failed to get general parameter block from ECT\n");
		return NULL;
	}

	return ect_gen_param_get_table(gen_block, name);
}

static struct static_power_table *alloc_static_power_table(int row_size, int col_size)
{
	struct static_power_table *spt;

	spt = kzalloc(sizeof(struct static_power_table), GFP_KERNEL);
	if (unlikely(!spt))
		return NULL;

	spt->volt.size = row_size;
	spt->temp.size = col_size;

	spt->power = kcalloc(spt->volt.size * spt->temp.size, sizeof(unsigned long), GFP_KERNEL);
	spt->volt.list = kcalloc(spt->volt.size, sizeof(int), GFP_KERNEL);
	spt->temp.list = kcalloc(spt->temp.size, sizeof(int), GFP_KERNEL);

	if (!spt->power || !spt->volt.list || !spt->temp.list) {
		kfree(spt->power);
		kfree(spt->volt.list);
		kfree(spt->temp.list);
		kfree(spt);
		return NULL;
	}

	return spt;
}

static void copy_static_power_table(struct static_power_table *dst,
		struct static_power_table *src)
{
	memcpy(dst->volt.list, src->volt.list, sizeof(unsigned int) * src->volt.size);
	memcpy(dst->temp.list, src->temp.list, sizeof(unsigned int) * src->temp.size);
}

static struct static_power_table *init_dsu_static_power_table(struct static_power_table *spt)
{
	struct ect_gen_param_table *ect_dsr;
	struct static_power_table *dsu_spt;
	int row, col, dsr;

	ect_dsr = get_ect_gen_param_table("DSU_STATIC_RATIO");
	if (!ect_dsr)
		return NULL;

	dsr = ect_dsr->parameter[0];

	/* size of dsu static power table is same as this domain */
	dsu_spt = alloc_static_power_table(spt->volt.size, spt->temp.size);
	if (!dsu_spt)
		return NULL;

	copy_static_power_table(dsu_spt, spt);

	for (row = 0; row < spt->volt.size; row++) {
		for (col = 0; col < spt->temp.size; col++) {
			unsigned int orig_power = of_power(spt, row, col);

			of_power(spt, row, col) = orig_power * (100 - dsr) / 100;
			of_power(dsu_spt, row, col) = orig_power * dsr / 100;
		}
	}

	return dsu_spt;
}

static struct static_power_table *init_sl_static_power_table(struct device_node *np,
		struct static_power_table *spt)
{
	struct static_power_table *sl_spt;
	struct ect_gen_param_table *ect_cr, *ect_ccr, *ect_csr;
	const char *buf[VENDOR_NR_CPUS];
	int cr, ccr, csr;
	int row, col;
	int count, i;

	np = of_find_node_by_name(np, "complex");
	if (!np)
		return NULL;

	count = of_property_count_strings(np, "cpus");
	if (!count)
		return NULL;

	ect_cr = get_ect_gen_param_table("COMPLEX_RATIO");
	ect_ccr = get_ect_gen_param_table("COMPLEX_CORE_RATIO");
	ect_csr = get_ect_gen_param_table("COMPLEX_SHARED_RATIO");
	if (!ect_cr || !ect_ccr || !ect_csr)
		return NULL;

	cr = ect_cr->parameter[0];
	ccr= ect_ccr->parameter[0];
	csr = ect_csr->parameter[0];

	sl_spt = alloc_static_power_table(spt->volt.size, spt->temp.size);
	if (!sl_spt)
		return NULL;

	copy_static_power_table(sl_spt, spt);

	for (row = 0; row < spt->volt.size; row++) {
		for (col = 0; col < spt->temp.size; col++) {
			unsigned long orig_power = of_power(spt, row, col);

			/*
			 * P(complex) = P(total) * complex_ratio / 100
			 * P(complex-core) = P(complex) * complex_ccr / 100
			 * P(complex-shared) = P(complex) * complex_shared_ratio / 100
			 */
			of_power(spt, row, col) = orig_power * cr * ccr / 10000;
			of_power(sl_spt, row, col) = orig_power * cr * csr / 10000;
		}
	}

	of_property_read_string_array(np, "cpus", buf, count);
	for (i = 0; i < count; i++) {
		struct cpumask mask;
		int cpu;

		cpulist_parse(buf[i], &mask);
		for_each_cpu(cpu, &mask)
			cpumask_copy(&per_cpu_et(cpu)->sl_cpus[cpu], &mask);
	}

	sl_spt->tz_dev = spt->tz_dev;

	return sl_spt;
}

static void get_field_property_size(struct ect_gen_param_table *vt, int *volt_size, int *temp_size)
{
	int min, max;

	min = vt->parameter[vt->num_of_col];
	max = vt->parameter[(vt->num_of_row - 1) * vt->num_of_col];
	*volt_size = (max - min) / UNIT_OF_VOLT + 1;

	min = vt->parameter[1];
	max = vt->parameter[vt->num_of_col - 1];
	*temp_size = (max - min) / UNIT_OF_TEMP + 1;
}

static unsigned long calulate_static_power(int ag, int ids, int mV, int vt_c,
		int ac_a, int ac_b, int ac_c)
{
	unsigned long long sp;
	long long ratio;

	ratio = (vt_c * ((ac_a * ag * ag) + (ac_b * ag) + ac_c));
	sp = div64_u64(mV * ids * ratio, 10000000000);

	return (unsigned long)sp;
}

static void fill_static_power_table(struct static_power_table *spt, struct ect_gen_param_table *vt,
		struct ect_gen_param_table *a, int asv_grp, int ids)
{
	int row, col;
	int i;

	/* Fill voltage field */
	spt->volt.min = vt->parameter[vt->num_of_col];
	for (i = 0; i < spt->volt.size; i++)
		spt->volt.list[i] = spt->volt.min + UNIT_OF_VOLT * i;

	/* Fill temperature field */
	spt->temp.min = vt->parameter[1];
	for (i = 0; i < spt->temp.size; i++)
		spt->temp.list[i] = spt->temp.min + UNIT_OF_TEMP * i;

	for (row = 1; row < vt->num_of_row; row++) {
		unsigned int volt = vt->parameter[row * vt->num_of_col];
		int vi = find_nearest_index(&spt->volt, volt, UNIT_OF_VOLT);
		int ac_a = a->parameter[row * a->num_of_col + 0];
		int ac_b = a->parameter[row * a->num_of_col + 1];
		int ac_c = a->parameter[row * a->num_of_col + 2];

		for (col = 1; col < vt->num_of_col; col++) {
			unsigned int temp = vt->parameter[col];
			int ti = find_nearest_index(&spt->temp, temp, UNIT_OF_TEMP);

			of_power(spt, vi, ti) = calulate_static_power(asv_grp, ids, volt,
						vt->parameter[col + row * vt->num_of_col],
						ac_a, ac_b, ac_c);
		}
	}

	/* Fill the power values of volt/temp that are not in ECT by scaling */
	for (row = 0; row < spt->volt.size; row++) {
		if (of_power(spt, row, 0))
			continue;

		for (col = 0; col < spt->temp.size; col++) {
			of_power(spt, row, col) =
				(of_power(spt, row - 1, col) + of_power(spt, row + 1, col)) / 2;
		}
	}
	for (col = 0; col < spt->temp.size; col++) {
		if (of_power(spt, 0, col))
			continue;

		for (row = 0; row < spt->volt.size; row++) {
			of_power(spt, row, col) =
				(of_power(spt, row, col - 1) + of_power(spt, row, col + 1)) / 2;
		}
	}
}

static struct static_power_table *init_core_static_power_table(struct device_node *np)
{
	struct static_power_table *spt;
	struct ect_gen_param_table *vt, *a;
	const char *name;
	int cal_id, asv_grp, ids;
	int volt_size, temp_size;

	/* Parse cal-id */
	if (of_property_read_u32(np, "cal-id", &cal_id))
		return NULL;

	asv_grp = cal_asv_get_grp(cal_id);
	if (asv_grp <= 0)
		asv_grp = 8;		/* 8 = last group */

	ids = cal_asv_get_ids_info(cal_id);
	if (!ids)
		ids = 73;		/* 73 = temporarily value */

	/* Parse ect-param-vt */
	if (of_property_read_string(np, "ect-param-vt", &name))
		return NULL;
	vt = get_ect_gen_param_table((char *)name);
	if (!vt) {
		pr_err("Failed to get volt-temp param table from ECT\n");
		return NULL;
	}

	/* Parse ect-param-a */
	if (of_property_read_string(np, "ect-param-a", &name))
		return NULL;
	a = get_ect_gen_param_table((char *)name);
	if (!a) {
		pr_err("Failed to get asv param table from ECT\n");
		return NULL;
	}

	get_field_property_size(vt, &volt_size, &temp_size);

	spt = alloc_static_power_table(volt_size, temp_size);
	if (!spt)
		return NULL;

	fill_static_power_table(spt, vt, a, asv_grp, ids);

	if (!of_property_read_string(np, "tz-name", &name)) {
		spt->tz_dev = thermal_zone_get_zone_by_name(name);
		if (IS_ERR_VALUE(spt->tz_dev))
			spt->tz_dev = NULL;
	}

	return spt;
}

static void init_static_power_table(struct device_node *np, struct energy_table *table)
{
	struct static_power_table *spt;

	spt = init_core_static_power_table(np);
	if (!spt)
		return;

	if (dsu_table && of_property_read_bool(np, "power-sharing-dsu"))
		dsu_table->spt = init_dsu_static_power_table(spt);

	table->sl_spt = init_sl_static_power_table(np, spt);
	if (!table->sl_spt) {
		int num_of_cpus = cpumask_weight(&table->cpus);
		int row, col;

		for (row = 0; row < spt->volt.size; row++)
			for (col = 0; col < spt->temp.size; col++)
				of_power(spt, row, col) /= num_of_cpus;
	}

	table->spt = spt;
}

void et_register_dsu_constraint(int cpu, void *p, int size)
{
	struct energy_table *table = per_cpu_et(cpu);
	struct constraint *constraint = (struct constraint *)p;
	struct constraint *new_constraint;
	struct constraint **temp;
	int i;

	temp = kcalloc(table->nr_constraint + 1, sizeof(struct constraint *), GFP_KERNEL);
	if (!temp)
		return;

	for (i = 0; i < table->nr_constraint; i++)
		temp[i] = table->constraints[i];

	new_constraint = kcalloc(size, sizeof(struct constraint), GFP_KERNEL);
	if (!new_constraint) {
		kfree(temp);
		return;
	}

	memcpy(new_constraint, constraint, sizeof(struct constraint) * size);
	temp[table->nr_constraint] = new_constraint;

	kfree(table->constraints);
	table->constraints = temp;
	table->nr_constraint++;
}
EXPORT_SYMBOL_GPL(et_register_dsu_constraint);

void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size)
{
	int i;

	if (unlikely(!dsu_table))
		pr_err("Failed to init dsu_table\n");

	dsu_table->states = kcalloc(size, sizeof(struct energy_state), GFP_KERNEL);
	if (!dsu_table->states)
		return;

	dsu_table->nr_states = size;

	for (i = 0; i < size; i++) {
		dsu_table->states[i].frequency = freq_table[i];
		dsu_table->states[i].voltage = volt_table[i] / 1000;
	}
}
EXPORT_SYMBOL_GPL(et_init_dsu_table);

/*
 * Whenever frequency domain is registered, and energy table corresponding to
 * the domain is created. Because cpu in the same frequency domain has the same
 * energy table. Capacity is calculated based on the max frequency of the fastest
 * cpu, so once the frequency domain of the faster cpu is regsitered, capacity
 * is recomputed.
 */
void et_init_table(struct cpufreq_policy *policy)
{
	struct energy_table *table;
	struct device *dev;
	struct cpufreq_frequency_table *cursor;
	int table_size = 0, i = 0;

	table = per_cpu_et(policy->cpu);
	if (unlikely(!table))
		return;

	dev = get_cpu_device(policy->cpu);
	if (unlikely(!dev))
		return;

	/* Count valid frequency */
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		if ((cursor->frequency > policy->cpuinfo.max_freq) ||
				(cursor->frequency < policy->cpuinfo.min_freq))
			continue;

		table_size++;
	}

	/* There is no valid frequency in the table, cancels building energy table */
	if (!table_size)
		return;

	table->states = kcalloc(table_size, sizeof(struct energy_state), GFP_KERNEL);
	if (!table->states)
		return;

	table->nr_states = table_size;

	/* Fill the energy table with frequency, dynamic/static power and voltage */
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		struct dev_pm_opp *opp;
		struct energy_state *state;
		unsigned long f_hz;

		if ((cursor->frequency > policy->cpuinfo.max_freq) ||
				(cursor->frequency < policy->cpuinfo.min_freq))
			continue;

		f_hz = cursor->frequency * 1000;
		opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);

		state = &table->states[i++];
		state->frequency = cursor->frequency;
		state->voltage = dev_pm_opp_get_voltage(opp) / 1000;

		state->dynamic_power = get_dynamic_power(state->frequency, state->voltage,
				table->dynamic_coeff);
		state->static_power = get_static_power(table->spt, state->voltage,
				table->static_coeff, default_temp);
	}

	update_capacity();
}

static int init_cpu_data(struct device_node *np,
			struct ect_gen_param_table *dp_coeff, int *nr_cluster)
{
	struct energy_table *table;
	const char *buf;
	int cpu, index;

	table = kzalloc(sizeof(struct energy_table), GFP_KERNEL);
	if (unlikely(!table))
		return -ENOMEM;

	if (of_property_read_string(np, "cpus", &buf))
		goto fail;
	cpulist_parse(buf, &table->cpus);

	if (of_property_read_u32(np, "capacity-dmips-mhz", &table->mips_mhz_orig))
		goto fail;

	if (of_property_read_u32(np, "static-power-coefficient", &table->static_coeff))
		goto fail;

	if (dp_coeff && !of_property_read_u32(np, "ect-coeff-idx", &index))
		table->dynamic_coeff_orig = dp_coeff->parameter[index];
	else
		of_property_read_u32(np, "dynamic-power-coefficient", &table->dynamic_coeff_orig);

	cpumask_clear(&table->pd_cpus);
	if (!of_property_read_string(np, "pd-cpus", &buf))
		cpulist_parse(buf, &table->pd_cpus);

	table->dynamic_coeff = table->dynamic_coeff_orig;
	table->mips_mhz = table->mips_mhz_orig;

	for_each_cpu(cpu, &table->cpus) {
		per_cpu_et(cpu) = table;
		ems_rq_cluster_idx(cpu_rq(cpu)) = *nr_cluster;
	}
	*nr_cluster = *nr_cluster + 1;

	init_static_power_table(np, table);

	return 0;

fail:
	kfree(table);
	return -EINVAL;
}

static void init_dsu_data(void)
{
	struct device_node *np;
	struct ect_gen_param_table *dp_coeff;
	const char *buf;
	int index;

	np = of_find_node_by_path("/power-data/dsu");
	if (!np)
		return;

	if (of_property_read_u32(np, "ect-coeff-idx", &index))
		return;

	dp_coeff = get_ect_gen_param_table("DTM_PWR_Coeff");
	if (!dp_coeff || !dp_coeff->parameter)
		return;

	dsu_table = kzalloc(sizeof(struct dsu_energy_table), GFP_KERNEL);
	if (unlikely(!dsu_table))
		return;

	dsu_table->dynamic_coeff = dp_coeff->parameter[index];

	cpumask_clear(&dsu_table->pd_cpus);
	if (!of_property_read_string(np, "pd-cpus", &buf))
		cpulist_parse(buf, &dsu_table->pd_cpus);
}

static int init_data(void)
{
	struct device_node *np, *child;
	struct ect_gen_param_table *dp_coeff;
	int nr_cluster = 0;

	/* DSU power data should be initialized before CPU */
	init_dsu_data();

	/* Initialize CPU power data */
	np = of_find_node_by_path("/power-data/cpu");
	if (!np)
		return -ENODATA;

	dp_coeff = get_ect_gen_param_table("DTM_PWR_Coeff");

	for_each_child_of_node(np, child)
		if (init_cpu_data(child, dp_coeff, &nr_cluster))
			return -EINVAL;

	of_node_put(np);

	return 0;
}

int et_init(struct kobject *ems_kobj)
{
	int ret;

	energy_table = alloc_percpu(struct energy_table *);
	if (!energy_table) {
		pr_err("Failed to allocate energy table\n");
		return -ENOMEM;
	}

	ret = init_data();
	if (ret) {
		kfree(energy_table);
		pr_err("Failed to initialize energy table\n");
		return ret;
	}

	if (sysfs_create_group(ems_kobj, &et_attr_group))
		pr_err("failed to initialize energy_table sysfs\n");

	emstune_register_notifier(&et_emstune_notifier);

	return ret;
}

