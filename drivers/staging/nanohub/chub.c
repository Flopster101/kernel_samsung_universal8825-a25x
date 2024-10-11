// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IF Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif

#include <linux/soc/samsung/exynos-pmu.h>

#include <linux/interrupt.h>

#include "chub.h"
#include "ipc_chub.h"
#include "chub_dbg.h"
#include "chub_exynos.h"

#if IS_ENABLED(CONFIG_SHUB)
#include "chub_shub.h"
#endif

void contexthub_check_time(void)
{
	struct rtc_device *chub_rtc = rtc_class_open(CONFIG_RTC_SYSTOHC_DEVICE);
	struct rtc_device *ap_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	struct rtc_time chub_tm, ap_tm;
	time64_t chub_t, ap_t;

	rtc_read_time(ap_rtc, &chub_tm);
	rtc_read_time(chub_rtc, &ap_tm);

	chub_t = rtc_tm_sub(&chub_tm, &ap_tm);

	if (chub_t) {
		nanohub_info("nanohub %s: diff_time: %llu\n", __func__, chub_t);
		rtc_set_time(chub_rtc, &ap_tm);
	};

	chub_t = rtc_tm_to_time64(&chub_tm);
	ap_t = rtc_tm_to_time64(&ap_tm);
}

static irqreturn_t contexthub_irq_handler(int irq, void *data)
{
	struct contexthub_ipc_info *chub = data;
	struct cipc_info *cipc;
	enum ipc_mb_id rcv_mb_id;
	unsigned int status;
	int start_index;
	int irq_num;
	int ret;

	cipc = chub->chub_ipc->cipc;
	rcv_mb_id = cipc->user_info[CIPC_USER_CHUB2AP].map_info.dst.mb_id;
	status = ipc_hw_read_int_status_reg_all(chub->mailbox, rcv_mb_id);
	start_index = ipc_hw_start_bit(rcv_mb_id);
	irq_num = IRQ_NUM_CHUB_ALIVE + start_index;

	if (atomic_read(&chub->chub_sleep)) {
		nanohub_dev_info(chub->dev, "%s before resume: status:0x%x\n", __func__, status);
		chub->wakeup_by_chub_cnt++;
	}

	/* chub alive interrupt handle */
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(chub->mailbox, rcv_mb_id, irq_num);
		if ((atomic_read(&chub->chub_status) == CHUB_ST_POWER_ON) && chub->multi_os) {
			schedule_work(&chub->debug_work);
			return IRQ_HANDLED;
		}

		if (ipc_read_hw_value(IPC_VAL_HW_AP_STATUS) == CHUB_REBOOT_REQ) {
			nanohub_dev_err(chub->dev, "chub sends to request reboot\n");
			contexthub_handle_debug(chub, CHUB_ERR_FW_REBOOT);
		} else {
			/* set wakeup flag for chub_alive_lock */
			chub_wake_event(&chub->chub_alive_lock);
		}
	}

	if (contexthub_get_token(chub)) {
		nanohub_dev_err(chub->dev, "%s: in reset irq_status:%d\n", __func__, status);
		ipc_hw_clear_all_int_pend_reg(chub->mailbox, rcv_mb_id);
		return IRQ_HANDLED;
	}

	irq_num = IRQ_NUM_CHUB_LOG + start_index;
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(chub->mailbox, rcv_mb_id, irq_num);
	}

	if (status) {
		ret = cipc_irqhandler(CIPC_USER_CHUB2AP, status);
		if (ret)
			contexthub_handle_debug(chub, CHUB_ERR_ISR);
	}
	contexthub_put_token(chub);
	wake_up(&chub->log_kthread_wait);
	return IRQ_HANDLED;
}

static irqreturn_t contexthub_irq_wdt_handler(int irq, void *data)
{
	struct contexthub_ipc_info *chub = data;

	nanohub_dev_info(chub->dev, "%s called\n", __func__);
	disable_irq_nosync(chub->irq_wdt);
	chub->irq_wdt_disabled = 1;
	contexthub_handle_debug(chub, CHUB_ERR_FW_WDT);

	return IRQ_HANDLED;
}

static void __iomem *get_iomem_with_range(struct platform_device *pdev, const char *name,
					  phys_addr_t *base, resource_size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	ret = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ret)) {
		nanohub_dev_err(&pdev->dev, "fails to ioremap resource  %s\n", name);
		return ret;
	}

	if (base && size) {
		*base = res->start;
		*size = resource_size(res);
	}

	return ret;
}

static void __iomem *get_iomem(struct platform_device *pdev, const char *name)
{
	return get_iomem_with_range(pdev, name, NULL, NULL);
}

static int contexthub_dt_init(struct platform_device *pdev,
					 struct contexthub_ipc_info *chub)
{
	int ret;
	const char *tmp;
	const char *dfs;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	/*chub_usi_array*/
	int reg_cnt;
	const char *reg_names[50];
	int i, k;
	int j = 0;
	int baaw_sizes[BAAW_MAX];

	tmp = of_get_property(node, "multi-os", NULL);
	if (tmp && !strcmp(tmp, "enabled")) {
		chub->multi_os = true;
	}

	nanohub_dev_debug(dev, "%s: multi-os: %d(%s)\n", __func__, chub->multi_os, tmp);

	chub->os_load = 1;

	dfs = of_get_property(node, "chub-dfs-gov", NULL);
	if (dfs) {
		nanohub_dev_debug(dev, "%s: dfs %s\n", __func__, dfs);
		chub->chub_dfs_gov = !strcmp(dfs, "enabled");
	}

	chub->alive_mct = of_property_read_bool(node, "use_alive_mct");

	nanohub_dev_debug(dev, "%s: use_alive_mct enabled: %d\n", __func__, chub->alive_mct);

	chub->irq_mailbox = irq_of_parse_and_map(node, 0);
	if (chub->irq_mailbox < 0) {
		nanohub_dev_err(dev, "failed to get irq\n");
		return -EINVAL;
	}
	nanohub_dev_debug(dev, "%s: chub irq mailbox %d\n", __func__, chub->irq_mailbox);

	/* request irq handler */
#if IS_ENABLED(CONFIG_SHUB)
	ret = devm_request_threaded_irq(dev, chub->irq_mailbox, NULL, contexthub_irq_handler,
			       IRQF_ONESHOT, dev_name(dev), chub);
#else
	ret = devm_request_irq(dev, chub->irq_mailbox, contexthub_irq_handler,
			       IRQ_TYPE_LEVEL_HIGH, dev_name(dev), chub);
#endif
	if (ret) {
		nanohub_dev_err(dev, "failed to request irq:%d, ret:%d\n",
			chub->irq_mailbox, ret);
		return ret;
	}

	irq_set_irq_wake(chub->irq_mailbox, 1);

	chub->irq_wdt = irq_of_parse_and_map(node, 1);
	if (chub->irq_wdt > 0) {
		ret = devm_request_irq(dev, chub->irq_wdt, contexthub_irq_wdt_handler,
					IRQ_TYPE_LEVEL_HIGH, dev_name(dev), chub);
		if (ret) {
			dev_err(dev, "failed to request: %d\n", ret);
			return ret;
		}
		chub->irq_wdt_disabled = 0;
	} else {
		nanohub_dev_info(dev, "WDT IRQ is not registered\n");
	}

	chub->mailbox = get_iomem(pdev, "mailbox");
	if (IS_ERR(chub->mailbox))
		return PTR_ERR(chub->mailbox);

	/* get SRAM base */
	chub->sram = get_iomem_with_range(pdev, "sram", &chub->sram_phys, &chub->sram_size);
	if (IS_ERR(chub->sram))
		return PTR_ERR(chub->sram);

	/* get chub gpr base */
	chub->chub_dumpgpr = get_iomem(pdev, "dumpgpr");
	if (IS_ERR(chub->chub_dumpgpr))
		return PTR_ERR(chub->chub_dumpgpr);

	contexthub_get_qch_base(chub);

	/* get chub sfr base to dump */
	chub->chub_dump_cmu = get_iomem(pdev, "chub_dump_cmu");
	chub->chub_dump_sys = get_iomem(pdev, "chub_dump_sys");
	chub->chub_dump_wdt = get_iomem(pdev, "chub_dump_wdt");
	chub->chub_dump_timer = get_iomem(pdev, "chub_dump_timer");
	chub->chub_dump_pwm = get_iomem(pdev, "chub_dump_pwm");
	chub->chub_dump_rtc = get_iomem(pdev, "chub_dump_rtc");

	/* get usi_array to dump */
	chub->usi_cnt = 0;
	reg_cnt = of_property_count_strings(node, "reg-names");
	of_property_read_string_array(node, "reg-names", reg_names, reg_cnt);
	for (j = 0; j < reg_cnt; j++) {
		if (strstr(reg_names[j], "usi"))
			nanohub_dev_debug(&pdev->dev,
				 "%s: usi reg name %s\n",
				 __func__, reg_names[j]);
	}
	for (j = 0; j < reg_cnt; j++) {
		if (strstr(reg_names[j], "usi")) {
			if (chub->usi_cnt < MAX_USI_CNT) {
				chub->usi_array[chub->usi_cnt] = get_iomem(pdev, reg_names[j]);
				if (IS_ERR(chub->usi_array[chub->usi_cnt]))
					return PTR_ERR(chub->usi_array[chub->usi_cnt]);
				chub->usi_cnt++;
			} else {
				dev_notice(&pdev->dev,
					   "Number of USI regs > MAX Sitting Value(15)\n");
				break;
			}
		}
	}

	chub->upmu = get_iomem(pdev, "upmu");
	chub->chub_out = get_iomem(pdev, "chub_out");
#ifdef CHUB_CPU_CONFIGURATION
	chub->pmu_chub_cpu = ioremap(CHUB_CPU_CONFIGURATION, 0x10);
	if (IS_ERR_OR_NULL(chub->pmu_chub_cpu))
		nanohub_dev_debug(dev, "%s: chub core reset not supported\n", __func__);
#endif
	reg_cnt = of_property_count_strings(node, "baaw,names");
	ret = of_property_read_u32_array(node, "baaw,sizes", baaw_sizes, reg_cnt);

	for (i = 0, j =0 ; i < reg_cnt ; i++) {
		of_property_read_string_index(node, "baaw,names", i, &chub->chub_baaws[i].name);
		chub->chub_baaws[i].size = baaw_sizes[i];
		chub->chub_baaws[i].addr = get_iomem(pdev, chub->chub_baaws[i].name);
		for (k = 0; k < baaw_sizes[i] * 5 ; k++) {
			if (of_property_read_u32_index
				(node, "baaw,values", j++,
				&chub->chub_baaws[i].values[k]))
				break;
			nanohub_dev_debug(chub->dev, "%s: %s [%d]0x%x\n", __func__,
					  chub->chub_baaws[i].name, k,
					  chub->chub_baaws[i].values[k]);
		}
	}

	/* disable chub irq list (for sensor irq) */
	of_property_read_u32(node, "chub-irq-pin-len", &chub->irq_pin_len);
	if (chub->irq_pin_len) {
		if (chub->irq_pin_len > ARRAY_SIZE(chub->irq_pins)) {
			dev_err(&pdev->dev, "failed to get irq pin length %d, %d\n",
				chub->irq_pin_len, sizeof(chub->irq_pins));
			chub->irq_pin_len = 0;
			return -EINVAL;
		}

		nanohub_dev_debug(&pdev->dev, "get chub irq_pin len:%d\n", chub->irq_pin_len);
		for (i = 0; i < chub->irq_pin_len; i++) {
			chub->irq_pins[i] = of_get_named_gpio(node, "chub-irq-pin", i);
			if (!gpio_is_valid(chub->irq_pins[i])) {
				nanohub_dev_err(&pdev->dev, "get invalid chub irq_pin:%d\n",
					chub->irq_pins[i]);
				return -EINVAL;
			}
		}
		contexthub_disable_pin(chub);
	}

	contexthub_set_clk(chub);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_AONCAM)
static int contexthub_aon_notifier(struct notifier_block *nb,
				   unsigned long action, void *nb_data)
{
	struct contexthub_ipc_info *chub = container_of(nb, struct contexthub_ipc_info, lpd_nb);
	int timeout;

	nanohub_info("%s: action[%lu]\n", __func__, action);
	if (chub) {
		switch (action) {
			case AON_NOTIFIER_START:
				contexthub_ipc_write_event(chub, MAILBOX_EVT_START_AON);
				chub->aon_flag = AON_WAIT_ACK;
				timeout = wait_event_timeout(chub->aon_wait
							     chub->aon_flag == AON_START_ACK,
							     msecs_to_jiffies(AON_WAIT_TIME));
				nanohub_info("%s: AON start ack timeout %d\n", __func__, timeout);
				return timeout > 0 ? NOTIFY_OK : NOTIFY_DONE;
				break;
			case AON_NOTIFIER_STOP:
				contexthub_ipc_write_event(chub, MAILBOX_EVT_STOP_AON);
				chub->lpd_flag = AON_WAIT_ACK;
				timeout = wait_event_timeout(chub->aon_wait,
							     chub->aon_flag == AON_STOP_ACK,
							     msecs_to_jiffies(AON_WAIT_TIME));
				nanohub_info("%s: AON stop ack timeout %d\n", __func__, timeout);
				return timeout > 0 ? NOTIFY_OK : NOTIFY_DONE;
				break;
			default:
				nanohub_err("%s: not defined\n", __func__);
				return NOTIFY_BAD;
				break;
		}
	} else
		nanohub_err("%s: chub not available!\n", __func__);

	return NOTIFY_BAD;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int contexthub_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct contexthub_ipc_info *data = container_of(nb, struct contexthub_ipc_info, itmon_nb);
	struct itmon_notifier *itmon_data = nb_data;

	if (itmon_data && itmon_data->master &&
	   (strstr(itmon_data->master, "CHUB") || strstr(itmon_data->master, "SHUB"))) {
		nanohub_dev_info(data->dev, "%s: chub(%s) itmon detected: action:%d!!\n",
			__func__, itmon_data->master, action);
		if (atomic_read(&data->chub_status) == CHUB_ST_RUN) {
#if IS_ENABLED(CONFIG_SHUB)
			contexthub_notifier_call(data, CHUB_FW_ST_OFF);
#endif
			atomic_set(&data->chub_status, CHUB_ST_ITMON);
			chub_dbg_dump_hw(data, CHUB_ERR_ITMON);
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
			if (!data->itmon_dumped) {
				memlog_do_dump(data->mlog.memlog_sram_chub, MEMLOG_LEVEL_EMERG);
				data->itmon_dumped = true;
			}
#endif
			contexthub_shutdown(data);
			contexthub_handle_debug(data, CHUB_ERR_ITMON);
		}
		return ITMON_SKIP_MASK;
	}
	return NOTIFY_OK;
}
#endif

static int contexthub_panic_handler(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	struct contexthub_ipc_info *chub = container_of(nb, struct contexthub_ipc_info, panic_nb);

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	if (!chub->itmon_dumped)
		memlog_do_dump(chub->mlog.memlog_sram_chub, MEMLOG_LEVEL_EMERG);
#endif
	chub_dbg_dump_ram(chub, CHUB_ERR_KERNEL_PANIC);
	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
static int contexthub_sysevent_shutdown(const struct sysevent_desc *desc, bool force_stop)
{
	(void) desc;
	nanohub_info("%s [%d]\n", __func__, force_stop);

	return 0;
}

static int contexthub_sysevent_powerup(const struct sysevent_desc *desc)
{
	(void) desc;
	nanohub_info("%s\n", __func__);
	return 0;
}

static int contexthub_sysevent_ramdump(int tmp, const struct sysevent_desc *desc)
{
	(void) desc;
	(void) tmp;
	nanohub_info("%s\n", __func__);
	return 0;
}

static void contexthub_sysevent_crash_shutdown(const struct sysevent_desc *desc)
{
	(void) desc;
	nanohub_info("%s\n", __func__);
}
#endif
/* CIPC Notification */
static int contexthub_notifier(struct contexthub_notifier_block *nb)
{
	nanohub_info("%s called!: subsys:%s, start_off:%x, end_off:%x",
			 __func__, nb->subsystem, nb->start_off, nb->end_off);
	return 0;
}

int contexthub_memlog_init(struct contexthub_ipc_info *chub)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	int ret;

	ret = memlog_register("CHB", chub->dev, &chub->mlog.memlog_chub);
	if (ret) {
		dev_err(chub->dev, "memlog chub desc registration fail ret:%d\n", ret);
		return ret;
	}

	/* error handling */
	chub->mlog.memlog_printf_file_chub = memlog_alloc_file(chub->mlog.memlog_chub,
							       "log-fil", SZ_512K, SZ_1M, 1000, 3);
	dev_info(chub->dev, "memlog printf file chub %s\n",
		 chub->mlog.memlog_printf_file_chub ? "pass" : "fail");
	if (chub->mlog.memlog_printf_file_chub) {
		memlog_obj_set_sysfs_mode(chub->mlog.memlog_printf_file_chub, true);
		/* error handling */
		chub->mlog.memlog_printf_chub =
		    memlog_alloc_printf(chub->mlog.memlog_chub, SZ_512K,
					chub->mlog.memlog_printf_file_chub, "log-mem", 0);
	}
	dev_info(chub->dev, "memlog printf chub %s\n",
		 chub->mlog.memlog_printf_chub ? "pass" : "fail");

	/* error handling */
	chub->mlog.memlog_sram_file_chub =
		memlog_alloc_file(chub->mlog.memlog_chub,
				  "srm-fil", chub->sram_size, chub->sram_size, 1000, 3);
	if (chub->mlog.memlog_sram_file_chub)
		/* error handling */
		chub->mlog.memlog_sram_chub =
		    memlog_alloc_dump(chub->mlog.memlog_chub, chub->sram_size, chub->sram_phys, false,
				      chub->mlog.memlog_sram_file_chub, "srm-dmp");
	dev_info(chub->dev, "memlog dump chub %s\n",
		 chub->mlog.memlog_sram_chub ? "pass" : "fail");
#endif
	return 0;
}

int contexthub_sync_memlogger(void *chub_p)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	struct contexthub_ipc_info *chub = chub_p;

	if (chub->mlog.memlog_printf_chub)
		ret |= memlog_sync_to_file(chub->mlog.memlog_printf_chub);
#endif
	return ret;
}

static struct contexthub_notifi_info cipc_noti[IPC_OWN_MAX] = {
	{"EMP", 0, NULL},
	{"CHUB", IPC_OWN_CHUB, NULL},
	{"AP", IPC_OWN_AP, NULL},
	{"GNSS", IPC_OWN_GNSS, NULL},
	{"ABOX", IPC_OWN_ABOX, NULL},
	{"VTS", IPC_OWN_VTS, NULL},
};

int contexthub_notifier_call(struct contexthub_ipc_info *chub,
			     enum CHUB_STATE state)
{
	int ret = 0;
	int i;
	u32 cipc_start_offset = 0;
	u32 cipc_size = 0;
	u32 ipc_base_offset = 0;
	u32 cipc_base_offset = 0;
	bool find = 0;

	nanohub_dev_info(chub->dev, "%s enters: state:%d\n", __func__, state);
	if (state == CHUB_FW_ST_OFF || state == CHUB_FW_ST_ON) {
		for (i = IPC_OWN_HOST; i < IPC_OWN_MAX; i++) {
			/* Check Notifier call pointer exist */
			if (!cipc_noti[i].nb)
				continue;

			if (cipc_noti[i].nb->state != CHUB_FW_ST_INVAL) {
				nanohub_dev_info(chub->dev,
						 "%s (%d:%s) call notifier call on chub-reset\n",
						 __func__, i, cipc_noti[i].name);
				cipc_noti[i].nb->state = state;
				ret = cipc_noti[i].nb->notifier_call(cipc_noti[i].nb);
				if (ret)
					nanohub_dev_info(chub->dev,
							 "%s (%d:%s) fails to notifier ret[%x] --\n",
							 __func__, i, cipc_noti[i].name, ret);
			}
		}
		return 0;
	}

	/* find the cipc base of sussystem */
	for (i = IPC_OWN_HOST; i < IPC_OWN_MAX; i++) {
		nanohub_dev_info(chub->dev,
				 "%s (%d:%s) start to find start_off\n",
				 __func__, i, cipc_noti[i].name);
		if ((i == IPC_OWN_MASTER) || (i == IPC_OWN_HOST)) {
			nanohub_dev_info(chub->dev,
					 "%s (%d:%s) skip by master & host\n",
					 __func__, i, cipc_noti[i].name);
			continue;
		}
		if (!cipc_noti[i].nb) {
			nanohub_dev_info(chub->dev,
					 "%s (%d:%s) skip by no notifier\n",
					 __func__, i, cipc_noti[i].name);
			continue;
		}

		cipc_get_offset_owner(i, &cipc_start_offset, &cipc_size);
		if (cipc_start_offset) {
			nanohub_dev_info(chub->dev,
					 "%s (%d:%s) get start_off:+%x, size:%d\n",
					 __func__, i, cipc_noti[i].name,
					 cipc_start_offset, cipc_size);

			cipc_base_offset = cipc_get_base(CIPC_REG_CIPC_BASE) - chub->sram;
			ipc_base_offset = ipc_get_base(IPC_REG_IPC) - chub->sram;
			/* offset check: offset should be 4KB align. And it's bigger than cipc_base. and it's smaller than ipc_end */
			if (((cipc_start_offset % IPC_ALIGN_SIZE) == 0) &&
			    ((cipc_size % IPC_ALIGN_SIZE) == 0) &&
			    (cipc_base_offset <= cipc_start_offset) &&
			    ((cipc_start_offset + cipc_size) <=
			    (ipc_base_offset + ipc_get_size(IPC_REG_IPC)))) {
				cipc_noti[i].nb->start_off = cipc_start_offset;
				cipc_noti[i].nb->end_off = cipc_start_offset + cipc_size;
				nanohub_dev_info(chub->dev,
						 "%s (%d:%s) fill notifier:start_off:+%x, end_off:+%x\n",
						 __func__, i, cipc_noti[i].name,
						 cipc_noti[i].nb->start_off,
						 cipc_noti[i].nb->end_off);
				find = 1;
			} else {
				nanohub_dev_err(chub->dev,
						"%s (%d:%s) invalid start_off:+%x(align:%d), end_off:+%x(align:%d), cipc_base_off:+%x, ipc_end_off:+%x\n",
						__func__, i, cipc_noti[i].name,
						cipc_start_offset, cipc_start_offset % IPC_ALIGN_SIZE,
						cipc_start_offset + cipc_size,
						(cipc_start_offset + cipc_size) % IPC_ALIGN_SIZE,
						cipc_base_offset, ipc_base_offset + ipc_get_size((IPC_REG_IPC)));
			}
		}
	}

	/* call cipc notifiers */
	if (find) {
		for (i = IPC_OWN_HOST; i < IPC_OWN_MAX; i++) {
			/* Check Notifier call pointer exist */
			if (!cipc_noti[i].nb) {
				nanohub_dev_info(chub->dev,
						 "%s (%d:%s) doesn't have notifier call\n",
						 __func__, i, cipc_noti[i].name);
				continue;
			}
			nanohub_dev_info(chub->dev,
					 "%s (%d:%s) start_off:+%x, end_off:+%x\n",
					 __func__, i, cipc_noti[i].name,
					 cipc_noti[i].nb->start_off,
					 cipc_noti[i].nb->end_off);
			if (cipc_noti[i].nb->start_off) {
				cipc_noti[i].nb->state = state;
				ret = cipc_noti[i].nb->notifier_call(cipc_noti[i].nb);
				if (ret)
					nanohub_dev_info(chub->dev,
							 "%s (%d:%s) fails to notifier ret[%x] --\n",
							 __func__, i,
							 cipc_noti[i].name,
							 ret);
			}
		}
	}
	return ret;
}

int contexthub_notifier_register(struct contexthub_notifier_block *nb)
{
	int index;

	if (!nb) {
		pr_err("%s: subsystem notifier block is NULL pointer\n",
		       __func__);
		return -EINVAL;
	}
	if (!nb->subsystem || !nb->notifier_call) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return -EINVAL;
	}

	for (index = IPC_OWN_HOST; index < IPC_OWN_MAX; index++) {
		if (!strncmp
		    (cipc_noti[index].name, nb->subsystem, IPC_NAME_MAX))
			break;
	}

	if (index >= IPC_OWN_MAX) {
		pr_err("%s: can't find subsystem:%s\n",
		       __func__, nb->subsystem);
		return -EINVAL;
	}

	nb->start_off = 0;
	nb->end_off = 0;
	cipc_noti[index].nb = nb;
	pr_info("%s: (%s) register successful!\n", __func__, nb->subsystem);
	return 0;
}
EXPORT_SYMBOL(contexthub_notifier_register);

static int contexthub_ipc_probe(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub;
	int ret = 0;

	chub_dbg_get_memory(pdev->dev.of_node);

	chub = devm_kzalloc(&pdev->dev, sizeof(*chub), GFP_KERNEL);
	if (!chub)
		return -ENOMEM;

	chub->dev = &pdev->dev;
	platform_set_drvdata(pdev, chub);

	ret = contexthub_log_init(chub);
	if (ret) {
		dev_err(&pdev->dev, "log init is fail with ret %d\n", ret);
		return ret;
	}

	/* parse dt and hw init */
	ret = contexthub_dt_init(pdev, chub);
	if (ret)
		return ret;

	chub_dbg_register_dump_to_dss(chub->sram_phys, chub->sram_size);
	ret = contexthub_memlog_init(chub);
	if (ret) {
		dev_err(chub->dev, "memlog not registered...\n");
		return ret;
	}

	ret = contexthub_blk_poweron(chub);
	if (ret) {
		dev_err(&pdev->dev, "block poweron failed\n");
		return ret;
	}

	spin_lock_init(&chub->logout_lock);
	atomic_set(&chub->chub_status, CHUB_ST_NO_POWER);
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	/* itmon notifier */
	chub->itmon_nb.notifier_call = contexthub_itmon_notifier;
	itmon_notifier_chain_register(&chub->itmon_nb);
#endif
	/* panic notifier */
	chub->panic_nb.notifier_call = contexthub_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &chub->panic_nb);

	/* chub power & reset notifier */
	chub->chub_cipc_nb.subsystem = "CHUB";
	chub->chub_cipc_nb.notifier_call = contexthub_notifier;
	contexthub_notifier_register(&chub->chub_cipc_nb);

#if defined(CONFIG_EXYNOS_AONCAM)
	chub->aon_nb.notifier_call = aon_notifier_chub;
	aon_notifier_register(&chub->aon_nb);
	init_waitqueue_head(&chub->aon_wait);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	/* sysevent register */
	chub->sysevent_desc.name = "CHB";
	strcpy(chub->sysevent_desc.fw_name, chub->multi_os ? OS_IMAGE_MULTIOS : OS_IMAGE_DEFAULT);

	chub->sysevent_desc.owner = THIS_MODULE;
	chub->sysevent_desc.shutdown = contexthub_sysevent_shutdown;
	chub->sysevent_desc.powerup = contexthub_sysevent_powerup;
	chub->sysevent_desc.ramdump = contexthub_sysevent_ramdump;
	chub->sysevent_desc.crash_shutdown = contexthub_sysevent_crash_shutdown;
	chub->sysevent_desc.dev = &pdev->dev;
	chub->sysevent_dev = sysevent_register(&chub->sysevent_desc);

	if (IS_ERR(chub->sysevent_dev)) {
		ret = PTR_ERR(chub->sysevent_dev);
		dev_err(&pdev->dev, "failed to register sysevent:%d\n", ret);
		return ret;
	}
#endif

	ret = contexthub_ipc_if_init(chub);
	if (ret) {
		dev_err(&pdev->dev, "ipc_if init is fail with ret %d\n", ret);
		return ret;
	}

	ret = contexthub_bootup_init(chub);
	if (ret) {
		dev_err(&pdev->dev, "bootup init is fail with ret %d\n", ret);
		return ret;
	}

	nanohub_dev_info(&pdev->dev, "Probe done. sensor_drv:%s\n", chub->data ? "on" : "off");
#if IS_ENABLED(CONFIG_SHUB)
	set_contexthub_info(chub);
#endif
	return 0;
}

static int contexthub_ipc_remove(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub = platform_get_drvdata(pdev);

	kthread_stop(chub->log_kthread);
	chub_wake_lock_destroy(chub->ws_reset);
	return 0;
}

#if IS_ENABLED(CONFIG_SHUB)
static int contexthub_alive(struct contexthub_ipc_info *ipc)
{
	int cnt = 10;

	atomic_set(&ipc->chub_alive_lock.flag, 0);

	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_COMPLETE);
	ipc_hw_gen_interrupt(ipc->mailbox, ipc->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);

	while (cnt--) {
		if (atomic_read(&ipc->chub_alive_lock.flag)) {
			nanohub_dev_info(ipc->dev, "chub is alive\n");
			return 0;
		}
		mdelay(1);
	}
	nanohub_dev_info(ipc->dev, "chub is not alive\n");
	return -1;
}
#endif

static int contexthub_alive_noirq(struct contexthub_ipc_info *ipc)
{
	int cnt = 100;
	int start_index;
	unsigned int status;
	int irq_num;
	struct cipc_info *cipc;
	enum ipc_mb_id rcv_mb_id;
	void *base;

	cipc = ipc->chub_ipc->cipc;
	rcv_mb_id = cipc->user_info[CIPC_USER_CHUB2AP].map_info.dst.mb_id;
	status = ipc_hw_read_int_status_reg_all(ipc->mailbox, rcv_mb_id);
	start_index = ipc_hw_start_bit(rcv_mb_id);
	irq_num = IRQ_NUM_CHUB_ALIVE + start_index;
	base = cipc->user_info[CIPC_USER_CHUB2AP].mb_base;

#if IS_ENABLED(CONFIG_SHUB)
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_WAKE);
#else
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, CHUB_PG_OUT);
#endif 

	ipc_hw_gen_interrupt(ipc->mailbox, ipc->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);

	atomic_set(&ipc->chub_alive_lock.flag, 0);
	while(cnt--) {
		mdelay(1);
		status = ipc_hw_read_int_status_reg_all(base, rcv_mb_id);
		if (status & (1 << irq_num)) {
			ipc_hw_clear_int_pend_reg(base, rcv_mb_id, irq_num);
			atomic_set(&ipc->chub_alive_lock.flag, 1);
			nanohub_dev_info(ipc->dev, "chub is alive\n");
			return 0;
		}
	}
	nanohub_dev_info(ipc->dev, "chub is not alive\n");
	return -1;
}

static int contexthub_resume_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

 	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	disable_irq_wake(ipc->irq_mailbox);
	contexthub_alive_noirq(ipc);

	nanohub_dev_info(dev, "%s\n", __func__);
	return 0;
}

static int contexthub_suspend_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	enable_irq_wake(ipc->irq_mailbox);
#if IS_ENABLED(CONFIG_SHUB)
	nanohub_dev_info(dev, "%s: send ap sleep\n", __func__);
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_SLEEP);
#else
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, CHUB_PG_IN);
#endif
	ipc_hw_gen_interrupt(ipc->mailbox, ipc->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);

	return 0;
}

static int contexthub_suspend(struct device *dev)
{
	struct contexthub_ipc_info *chub = dev_get_drvdata(dev);
	struct ipc_info *chub_ipc = chub->chub_ipc;

	if (atomic_read(&chub->chub_status) != CHUB_ST_RUN)
		return 0;

#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
#ifdef ALIVE_WORK
	cancel_delayed_work(&chub->sensor_alive_work);
	chub->sensor_alive_work_run = false;
#endif
#endif
	nanohub_dev_info(dev, "%s: irq-pend:ap:%x,chub:%x\n", __func__,
		ipc_hw_read_int_status_reg_all(chub->mailbox, chub_ipc->my_mb_id),
		ipc_hw_read_int_status_reg_all(chub->mailbox, chub_ipc->opp_mb_id));
	atomic_set(&chub->chub_sleep, 1);

	if (atomic_read(&chub->in_log)) {
		nanohub_dev_info(dev, "%s: waiting log print done...\n", __func__);
		chub_wait_event(&chub->log_lock, 100);
	}

#if !IS_ENABLED(CONFIG_SHUB)
	nanohub_dev_info(dev, "%s: send ap sleep\n", __func__);
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_SLEEP);
	ipc_hw_gen_interrupt(chub->mailbox, chub->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);
	nanohub_dev_info(dev, "%s: out\n", __func__);
#endif

	return 0;
}

static int contexthub_resume(struct device *dev)
{
	struct contexthub_ipc_info *chub = dev_get_drvdata(dev);
	struct ipc_info *chub_ipc = chub->chub_ipc;

	if (atomic_read(&chub->chub_status) != CHUB_ST_RUN)
		return 0;
#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
#ifdef ALIVE_WORK
	if (chub->sensor_alive_work_run == false) {
		dev_info(dev, "%s: schedule sensor_alive_work\n", __func__);
		schedule_delayed_work(&chub->sensor_alive_work, msecs_to_jiffies(1000));
	}
#endif
#endif

#if !IS_ENABLED(CONFIG_SHUB)
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_WAKE);
	ipc_hw_gen_interrupt(chub->mailbox, chub->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);
#endif
	atomic_set(&chub->chub_sleep, 0);

	nanohub_dev_info(dev, "%s: irq-pend:ap:%x,chub:%x\n", __func__,
		ipc_hw_read_int_status_reg_all(chub->mailbox, chub_ipc->my_mb_id),
		ipc_hw_read_int_status_reg_all(chub->mailbox, chub_ipc->opp_mb_id));
	return 0;
}

#if IS_ENABLED(CONFIG_SHUB)
static int contexthub_prepare(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	nanohub_dev_info(dev, "%s\n", __func__);
	ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_PREPARE);
	ipc_hw_gen_interrupt(ipc->mailbox, ipc->chub_ipc->opp_mb_id, IRQ_NUM_CHUB_ALIVE);

	return 0;
}

static void contexthub_complete(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return;

	nanohub_dev_info(dev, "%s\n", __func__);
	contexthub_alive(ipc);
}
#endif

static const struct dev_pm_ops contexthub_pm_ops = {
#if IS_ENABLED(CONFIG_SHUB)
	.prepare = contexthub_prepare,
	.complete = contexthub_complete,
#endif
	.suspend = contexthub_suspend,
	.suspend_noirq = contexthub_suspend_noirq,
	.resume = contexthub_resume,
	.resume_noirq = contexthub_resume_noirq,
};

static const struct of_device_id contexthub_ipc_match[] = {
	{.compatible = "samsung,exynos-nanohub"},
	{},
};

static struct platform_driver samsung_contexthub_ipc_driver = {
	.probe = contexthub_ipc_probe,
	.remove = contexthub_ipc_remove,
	.driver = {
		   .name = "nanohub-ipc",
		   .owner = THIS_MODULE,
		   .of_match_table = contexthub_ipc_match,
		   .pm = &contexthub_pm_ops,
	},
};

int contexthub_init(void)
{
#ifdef CONFIG_SENSOR_DRV
	nanohub_init();
#endif
	return platform_driver_register(&samsung_contexthub_ipc_driver);
}

static void __exit contexthub_cleanup(void)
{
#ifdef CONFIG_SENSOR_DRV
	nanohub_cleanup();
#endif
	platform_driver_unregister(&samsung_contexthub_ipc_driver);
}

module_init(contexthub_init);
module_exit(contexthub_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos contexthub mailbox Driver");
MODULE_AUTHOR("Boojin Kim <boojin.kim@samsung.com>");
