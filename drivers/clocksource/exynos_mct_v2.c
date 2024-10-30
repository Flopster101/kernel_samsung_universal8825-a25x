// SPDX-License-Identifier: GPL-2.0-only
/* drivers/clocksource/exynos_mct_v2.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos MCT(Multi-Core Timer) support
*/

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clocksource.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include "exynos_mct_v2.h"

static void __iomem *reg_base;
static unsigned long osc_clk_rate;
static unsigned int mct_int_type;
static int mct_irqs[MCT_NR_COMPS];
static DEFINE_SPINLOCK(comp_lock);
static u64 exynos_mct_start;

u64 exynos_get_mct_start(void)
{
	return exynos_mct_start;
}
EXPORT_SYMBOL_GPL(exynos_get_mct_start);

static void exynos_mct_set_compensation(unsigned long osc, unsigned long rtc)
{
	unsigned int osc_rtc;
	unsigned int osc_rtc_round;
	unsigned int compen_v;
	unsigned int sign;

	osc_rtc = (unsigned int) (osc * 1000 / rtc);
	osc_rtc_round = (osc_rtc + 500) / 1000 * 1000;
	sign = (osc_rtc_round > osc_rtc) ? 0: 1;

	compen_v = sign ? (osc_rtc - osc_rtc_round):(osc_rtc_round - osc_rtc);
	compen_v = (compen_v + 5) / 10;
	compen_v |= sign << 31;

	osc_rtc_round /= 1000;

	pr_info("MCT: osc-%lu rtc-%lu incr_rtcclk:0x%08x compen_v:0x%08x\n",
			osc, rtc, osc_rtc_round, compen_v);

	writel_relaxed(osc_rtc_round, reg_base + EXYNOS_MCT_MCT_INCR_RTCCLK);
	writel_relaxed(compen_v, reg_base + EXYNOS_MCT_COMPENSATE_VALUE);
}

/* Clocksource handling */
static void exynos_mct_frc_start(void)
{
	writel_relaxed(MCT_FRC_ENABLE, reg_base + EXYNOS_MCT_MCT_FRC_ENABLE);
}

/**
 * exynos_read_count_32 - Read the lower 32-bits of the global counter
 *
 * This will read just the lower 32-bits of the global counter.
 *
 * Returns the number of cycles in the global counter (lower 32 bits).
 */
static u32 exynos_read_count_32(void)
{
	return readl_relaxed(reg_base + EXYNOS_MCT_CNT_L);
}

static u64 exynos_frc_read(struct clocksource *cs)
{
	return exynos_read_count_32();
}

static struct clocksource mct_frc = {
	.name		= "mct-frc",
	.rating		= 350,	/* use value lower than ARM arch timer */
	.read		= exynos_frc_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int exynos_clocksource_init(void)
{
	if (clocksource_register_hz(&mct_frc, osc_clk_rate))
		panic("%s: can't register clocksource\n", mct_frc.name);

	return 0;
}

static void exynos_mct_comp_stop(struct mct_clock_event_device *mevt)
{
	unsigned int index = mevt->comp_index;
	unsigned int comp_mode, comp_enable, int_enb;
	unsigned long flags;

	spin_lock_irqsave(&comp_lock, flags);
	comp_enable = readl_relaxed(reg_base + EXYNOS_MCT_COMP_ENABLE);
	comp_enable &= ~(0x1 << index);
	writel_relaxed(comp_enable, reg_base + EXYNOS_MCT_COMP_ENABLE);
	mevt->disable_time = readl_relaxed(reg_base + EXYNOS_MCT_CNT_L);

	comp_mode = readl_relaxed(reg_base + EXYNOS_MCT_COMP_MODE);
	comp_mode &= ~(0x1 << index);
	writel_relaxed(comp_mode, reg_base + EXYNOS_MCT_COMP_MODE);

	int_enb = readl_relaxed(reg_base + EXYNOS_MCT_INT_ENB);
	int_enb &= ~(0x1 << index);
	writel_relaxed(int_enb, reg_base + EXYNOS_MCT_INT_ENB);

	writel_relaxed(0, reg_base + EXYNOS_MCT_COMP_L(index));
	writel_relaxed(0, reg_base + EXYNOS_MCT_COMP_U(index));

	writel_relaxed(0, reg_base + EXYNOS_MCT_PREPARE_CSTAT);
	writel_relaxed(0x1 << mevt->comp_index, reg_base + EXYNOS_MCT_INT_CSTAT);

	spin_unlock_irqrestore(&comp_lock, flags);

}

inline unsigned int count_diff(unsigned int pre_count, unsigned int cur_count) {
	if (cur_count >= pre_count)
		return cur_count - pre_count;
	else
		return (0xffffffff - pre_count) + cur_count;
}

static void exynos_mct_comp_start(struct mct_clock_event_device *mevt,
					bool periodic, unsigned long cycles)
{
	unsigned int index = mevt->comp_index;
	unsigned int comp_mode, comp_enable, int_enb;
	unsigned int pre_comp_l, pre_comp_u, comp_l, comp_u;
	unsigned int pre_cnt_l, cnt_l, cnt_u;
	unsigned int loop_cnt;
	unsigned long flags;

	int retry = 0;

	do {
		comp_enable = readl_relaxed(reg_base + EXYNOS_MCT_COMP_ENABLE);
		if (comp_enable & (0x1 << index))
			exynos_mct_comp_stop(mevt);

		pre_comp_l = readl_relaxed(reg_base + EXYNOS_MCT_COMP_L(index));
		pre_comp_u = readl_relaxed(reg_base + EXYNOS_MCT_COMP_U(index));

		spin_lock_irqsave(&comp_lock, flags);
		if (periodic) {
			comp_mode = readl_relaxed(reg_base + EXYNOS_MCT_COMP_MODE);
			comp_mode |= 0x1 << index;
			writel_relaxed(comp_mode , reg_base + EXYNOS_MCT_COMP_MODE);
		}

		writel_relaxed(cycles, reg_base + EXYNOS_MCT_COMP_PERIOD(index));

		int_enb = readl_relaxed(reg_base + EXYNOS_MCT_INT_ENB);
		int_enb |= 0x1 << index;
		writel_relaxed(int_enb, reg_base + EXYNOS_MCT_INT_ENB);
		spin_unlock_irqrestore(&comp_lock, flags);

		/*
		 * Wait until WAIT_MCT_CNT cycles have passed since COMP is disabled.
		 */
		loop_cnt = 0;
		pre_cnt_l = mevt->disable_time;

		do {
			cnt_l = readl_relaxed(reg_base + EXYNOS_MCT_CNT_L);
			loop_cnt++;
		} while((count_diff(pre_cnt_l, cnt_l) < WAIT_MCT_CNT) &&
				(loop_cnt < TIMEOUT_LOOP_COUNT));

		if (TIMEOUT_LOOP_COUNT == loop_cnt)
			pr_warn("MCT(comp%d) disable timeout\n", index);

		spin_lock_irqsave(&comp_lock, flags);
		comp_enable = readl_relaxed(reg_base + EXYNOS_MCT_COMP_ENABLE);
		comp_enable |= 0x1 << index;
		writel_relaxed(comp_enable , reg_base + EXYNOS_MCT_COMP_ENABLE);
		spin_unlock_irqrestore(&comp_lock, flags);

		pre_cnt_l = readl_relaxed(reg_base + EXYNOS_MCT_CNT_L);

		/*
		 * Wait for the COMP_U/L to change
		 */
		loop_cnt = 0;
		do {
			comp_l = readl_relaxed(reg_base + EXYNOS_MCT_COMP_L(index));
			comp_u = readl_relaxed(reg_base + EXYNOS_MCT_COMP_U(index));
			loop_cnt++;

			if (comp_l != pre_comp_l || comp_u != pre_comp_u) {
				return;
			}
		} while(TIMEOUT_LOOP_COUNT > loop_cnt);

		retry++;
		cnt_l = readl_relaxed(reg_base + EXYNOS_MCT_CNT_L);
		cnt_u = readl_relaxed(reg_base + EXYNOS_MCT_CNT_U);

		pr_warn("MCT(comp%d) enable timeout "
			"(COMP_U/L:0x%08x, 0x%08x, pre COMP_U/L:0x%08x, 0x%08x) "
			"(CNT_U/L:0x%08x, 0x%08x) "
			"pre CNT_L:0x%08x "
			"retry:%d/%d\n",
			index,
			comp_u, comp_l,
			pre_comp_u, pre_comp_l,
			cnt_u, cnt_l,
			pre_cnt_l,
			retry, RETRY_CNT);
	} while(retry <= RETRY_CNT);

	panic("MCT(comp%d) hangs (periodic:%d cycles:%lu)\n", index, periodic, cycles);
}

static int exynos_comp_set_next_event(unsigned long cycles,
				       struct clock_event_device *evt)
{
	struct mct_clock_event_device *mevt;

	mevt = container_of(evt, struct mct_clock_event_device, evt);

	exynos_mct_comp_start(mevt, false, cycles);

	return 0;
}

static int mct_set_state_shutdown(struct clock_event_device *evt)
{
	struct mct_clock_event_device *mevt;

	mevt = container_of(evt, struct mct_clock_event_device, evt);

	exynos_mct_comp_stop(mevt);
	return 0;
}

static int mct_set_state_periodic(struct clock_event_device *evt)
{
	unsigned long cycles_per_jiffy;
	struct mct_clock_event_device *mevt;

	mevt = container_of(evt, struct mct_clock_event_device, evt);


	cycles_per_jiffy = (((unsigned long long)NSEC_PER_SEC / HZ * evt->mult)
			    >> evt->shift);
	exynos_mct_comp_start(mevt, true, cycles_per_jiffy);
	return 0;
}



static struct mct_clock_event_device mct_comp_device = {
	.evt	= {
		.name			= "mct-comp0",
		.features		= CLOCK_EVT_FEAT_PERIODIC |
					  CLOCK_EVT_FEAT_ONESHOT,
		.rating			= 250,
		.set_next_event		= exynos_comp_set_next_event,
		.set_state_periodic	= mct_set_state_periodic,
		.set_state_shutdown	= mct_set_state_shutdown,
		.set_state_oneshot	= mct_set_state_shutdown,
		.set_state_oneshot_stopped = mct_set_state_shutdown,
		.tick_resume		= mct_set_state_shutdown,
	},
	.name	= "mct-comp0",
	.comp_index = MCT_COMP0,
};

static irqreturn_t exynos_mct_comp_isr(int irq, void *dev_id)
{
	struct mct_clock_event_device *mevt = dev_id;
	struct clock_event_device *evt = &mevt->evt;
	unsigned long flags;

	spin_lock_irqsave(&comp_lock, flags);
	writel_relaxed(0, reg_base + EXYNOS_MCT_PREPARE_CSTAT);
	writel_relaxed(0x1 << mevt->comp_index, reg_base + EXYNOS_MCT_INT_CSTAT);
	spin_unlock_irqrestore(&comp_lock, flags);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int exynos_clockevent_init(void)
{
	mct_comp_device.evt.cpumask = cpumask_of(0);
	clockevents_config_and_register(&mct_comp_device.evt, osc_clk_rate,
					0xf, 0xffffffff);
	if (request_irq(mct_irqs[MCT_COMP0], exynos_mct_comp_isr,
			IRQF_TIMER | IRQF_IRQPOLL, "mct_comp0_irq",
			&mct_comp_device))
		pr_err("%s: request_irq() failed\n", "mct_comp0_irq");

	return 0;
}

static DEFINE_PER_CPU(struct mct_clock_event_device, percpu_mct_tick);

static int exynos_mct_starting_cpu(unsigned int cpu)
{
	struct mct_clock_event_device *mevt =
		per_cpu_ptr(&percpu_mct_tick, cpu);
	struct clock_event_device *evt = &mevt->evt;
	struct irq_desc *desc = irq_to_desc(evt->irq);

	snprintf(mevt->name, sizeof(mevt->name), "mct_comp%d", MCT_COMP4 + cpu);

	evt->name = mevt->name;
	evt->cpumask = cpumask_of(cpu);
	evt->set_next_event = exynos_comp_set_next_event;
	evt->set_state_periodic = mct_set_state_periodic;
	evt->set_state_shutdown = mct_set_state_shutdown;
	evt->set_state_oneshot = mct_set_state_shutdown;
	evt->set_state_oneshot_stopped = mct_set_state_shutdown;
	evt->tick_resume = mct_set_state_shutdown;
	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	evt->rating = 500;	/* use value higher than ARM arch timer */

	if (evt->irq == -1)
		return -EIO;

#ifdef MODULE
	if (desc)
		irq_do_set_affinity(irq_desc_get_irq_data(desc), cpumask_of(cpu), true);
#else
	irq_force_affinity(evt->irq, cpumask_of(cpu));
#endif
	enable_irq(evt->irq);
	clockevents_config_and_register(evt, osc_clk_rate, 0xf, 0x7fffffff);

	return 0;
}

static int exynos_mct_dying_cpu(unsigned int cpu)
{
	struct mct_clock_event_device *mevt =
		per_cpu_ptr(&percpu_mct_tick, cpu);
	struct clock_event_device *evt = &mevt->evt;
	unsigned long flags;

	evt->set_state_shutdown(evt);
	if (evt->irq != -1)
		disable_irq_nosync(evt->irq);

	spin_lock_irqsave(&comp_lock, flags);
	writel_relaxed(0, reg_base + EXYNOS_MCT_PREPARE_CSTAT);
	writel_relaxed(0x1 << mevt->comp_index, reg_base + EXYNOS_MCT_INT_CSTAT);
	spin_unlock_irqrestore(&comp_lock, flags);

	return 0;
}

static int exynos_timer_resources(struct device_node *np, void __iomem *base)
{
	int err, cpu;

	struct clk *mct_clk, *tick_clk,  *rtc_clk;
	unsigned long rtc_clk_rate;
	int div;
	int ret;

	ret = of_property_read_u32(np, "div", &div);
	if (ret || !div) {
		pr_warn("MCT: fail to get the div value. set div to the default\n");
		div = DEFAULT_CLK_DIV;
	}

	tick_clk = of_clk_get_by_name(np, "fin_pll");
	if (IS_ERR(tick_clk))
		panic("%s: unable to determine tick clock rate\n", __func__);
	osc_clk_rate = clk_get_rate(tick_clk) / div;

	mct_clk = of_clk_get_by_name(np, "mct");
	if (IS_ERR(mct_clk))
		panic("%s: unable to retrieve mct clock instance\n", __func__);
	clk_prepare_enable(mct_clk);

	rtc_clk = of_clk_get_by_name(np, "rtc");
	if (IS_ERR(rtc_clk)) {
		pr_warn("MCT: fail to get rtc clock. set to the default\n");
		rtc_clk_rate = DEFAULT_RTC_CLK_RATE;
	} else {
		rtc_clk_rate = clk_get_rate(rtc_clk);
	}

	reg_base = base;
	if (!reg_base)
		panic("%s: unable to ioremap mct address space\n", __func__);

	exynos_mct_set_compensation(osc_clk_rate, rtc_clk_rate);
	exynos_mct_frc_start();

	for_each_possible_cpu(cpu) {
		int mct_irq;
		struct mct_clock_event_device *pcpu_mevt;

		if (MCT_COMP4 + cpu >= MCT_NR_COMPS)
			break;

		mct_irq = mct_irqs[MCT_COMP4 + cpu];
		pcpu_mevt = per_cpu_ptr(&percpu_mct_tick, cpu);

		pcpu_mevt->evt.irq = -1;
		pcpu_mevt->comp_index = MCT_COMP4 + cpu;
		pcpu_mevt->disable_time = 0;

		irq_set_status_flags(mct_irq, IRQ_NOAUTOEN);
		if (request_irq(mct_irq,
				exynos_mct_comp_isr,
				IRQF_TIMER | IRQF_NOBALANCING | IRQF_PERCPU,
				"exynos-mct", pcpu_mevt)) {
			pr_err("exynos-mct: cannot register IRQ (cpu%d)\n",
								cpu);

			continue;
		}
		pcpu_mevt->evt.irq = mct_irq;
	}

	/* Install hotplug callbacks which configure the timer on this CPU */
	err = cpuhp_setup_state(CPUHP_AP_EXYNOS4_MCT_TIMER_STARTING,
				"clockevents/exynos4/mct_timer:starting",
				exynos_mct_starting_cpu,
				exynos_mct_dying_cpu);
	if (err)
		goto out_irq;

	return 0;

out_irq:
	for_each_possible_cpu(cpu) {
		struct mct_clock_event_device *pcpu_mevt =
			per_cpu_ptr(&percpu_mct_tick, cpu);

		if (pcpu_mevt->evt.irq != -1) {
			free_irq(pcpu_mevt->evt.irq, pcpu_mevt);
			pcpu_mevt->evt.irq = -1;
		}
	}
	return err;
}

static int mct_init_dt(struct device_node *np, unsigned int int_type)
{
	u32 nr_irqs = 0, i;
	struct of_phandle_args irq;
	int ret;

	mct_int_type = int_type;

	/* This driver uses only one global timer interrupt */
	mct_irqs[MCT_COMP0] = irq_of_parse_and_map(np, MCT_COMP0);

	/*
	 * Find out the number of local irqs specified. The local
	 * timer irqs are specified after the four global timer
	 * irqs are specified.
	 */
	while (of_irq_parse_one(np, nr_irqs, &irq) == 0)
		nr_irqs++;

	for (i = MCT_COMP4; i < nr_irqs; i++)
		mct_irqs[i] = irq_of_parse_and_map(np, i);

	pr_info("## exynos_timer_resources\n");
	ret = exynos_timer_resources(np, of_iomap(np, 0));
	if (ret)
		return ret;

	pr_info("## exynos_clocksource_init\n");
	ret = exynos_clocksource_init();
	if (ret)
		return ret;

	pr_info("## exynos_clockevent_init\n");

	if (IS_ENABLED(CONFIG_SEC_BOOTSTAT)) {
		unsigned long __osc_clk_rate;
		u64 ts_msec;

		exynos_mct_start = exynos_read_count_32();
		__osc_clk_rate = osc_clk_rate / 1000;
		if (__osc_clk_rate)
			exynos_mct_start /= __osc_clk_rate;

		ts_msec = local_clock();
		do_div(ts_msec, 1000000);

		exynos_mct_start -= ts_msec;
	}

	return exynos_clockevent_init();
}

static int mct_init_spi(struct device_node *np)
{
	return mct_init_dt(np, MCT_INT_SPI);
}

#ifdef MODULE
static int exynos_mct_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	pr_info("exynos_mct_probe\n");

	return mct_init_spi(np);
}

static const struct of_device_id exynos_mct_match_table[] = {
	{ .compatible = "samsung,s5e9925-mct" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_mct_match_table);

static struct platform_driver s5e9925_mct_driver = {
	.probe		= exynos_mct_probe,
	.driver		= {
		.name	= "exynos-mct",
		.of_match_table = exynos_mct_match_table,
	},
};
module_platform_driver(s5e9925_mct_driver);

#else
TIMER_OF_DECLARE(s5e9925, "samsung,s5e9925-mct", mct_init_spi);
#endif

MODULE_DESCRIPTION("Exynos Multi Core Timer v2 driver");
MODULE_AUTHOR("Donghoon Yu <hoony.yu@samsung.com>");
MODULE_LICENSE("GPL v2");
