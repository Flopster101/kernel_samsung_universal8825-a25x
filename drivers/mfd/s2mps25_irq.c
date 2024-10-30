/*
 * s2mps25-irq.c - Interrupt controller support for S2MPS25
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
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

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mps25.h>
#include <linux/mfd/samsung/s2mps25-regulator.h>
#include <linux/input.h>

#define S2MPS25_VGI_CNT		5

static uint8_t irq_reg[S2MPS25_IRQ_GROUP_NR] = {0};

static const uint8_t s2mps25_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[S2MPS25_PMIC_INT1] = S2MPS25_REG_INT1M,
	[S2MPS25_PMIC_INT2] = S2MPS25_REG_INT2M,
	[S2MPS25_PMIC_INT3] = S2MPS25_REG_INT3M,
	[S2MPS25_PMIC_INT4] = S2MPS25_REG_INT4M,
	[S2MPS25_PMIC_INT5] = S2MPS25_REG_INT5M,
	[S2MPS25_PMIC_INT6] = S2MPS25_REG_INT6M,
	[S2MPS25_PMIC_INT7] = S2MPS25_REG_INT7M,
	[S2MPS25_PMIC_INT8] = S2MPS25_REG_INT8M,
	[S2MPS25_ADC_INTP]  = S2MPS25_REG_ADC_INTM,
};

static struct i2c_client *get_i2c(struct s2mps25_dev *s2mps25,
				enum s2mps25_irq_source src)
{
	switch (src) {
	case S2MPS25_PMIC_INT1 ... S2MPS25_PMIC_INT8:
		return s2mps25->pmic1;
	case S2MPS25_ADC_INTP:
		return s2mps25->adc_i2c;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mps25_irq_data {
	int mask;
	enum s2mps25_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mps25_irq_data s2mps25_irqs[] = {
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PWRONP_INT1,		S2MPS25_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PWRONR_INT1,		S2MPS25_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_JIGONBF_INT1,		S2MPS25_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_JIGONBR_INT1,		S2MPS25_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_ACOKBF_INT1,		S2MPS25_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_ACOKBR_INT1,		S2MPS25_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PWRON1S_INT1,		S2MPS25_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_MRB_INT1,			S2MPS25_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_RTC60S_INT2,		S2MPS25_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_RTCA1_INT2,		S2MPS25_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_RTCA0_INT2,		S2MPS25_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_SMPL_INT2,			S2MPS25_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_RTC1S_INT2,		S2MPS25_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_WTSR_INT2,			S2MPS25_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,	S2MPS25_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_WRSTB_INT2,		S2MPS25_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_120C_INT3,			S2MPS25_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_140C_INT3,			S2MPS25_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_TSD_INT3,			S2MPS25_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OVP_INT3,			S2MPS25_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_TX_TRAN_FAIL_INT3,		S2MPS25_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OTP_CSUM_ERR_INT3,		S2MPS25_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_VOLDNR_INT3,		S2MPS25_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_VOLDNP_INT3,		S2MPS25_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B1M_INT4,		S2MPS25_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B2M_INT4,		S2MPS25_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B3M_INT4,		S2MPS25_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B4M_INT4,		S2MPS25_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B5M_INT4,		S2MPS25_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B6M_INT4,		S2MPS25_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B7M_INT4,		S2MPS25_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B8M_INT4,		S2MPS25_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B9M_INT5,		S2MPS25_PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_B10M_INT5,		S2MPS25_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_SR1M_INT5,		S2MPS25_PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_SR2M_INT5,		S2MPS25_PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OCP_SR3M_INT5,		S2MPS25_PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PARITY_ERR0_INT5,		S2MPS25_PMIC_INT5, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PARITY_ERR1_INT5,		S2MPS25_PMIC_INT5, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PARITY_ERR2_INT5,		S2MPS25_PMIC_INT5, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B1M_INT6,		S2MPS25_PMIC_INT6, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B2M_INT6,		S2MPS25_PMIC_INT6, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B3M_INT6,		S2MPS25_PMIC_INT6, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B4M_INT6,		S2MPS25_PMIC_INT6, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B5M_INT6,		S2MPS25_PMIC_INT6, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B6M_INT6,		S2MPS25_PMIC_INT6, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B7M_INT6,		S2MPS25_PMIC_INT6, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B8M_INT6,		S2MPS25_PMIC_INT6, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B9M_INT7,		S2MPS25_PMIC_INT7, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_B10M_INT7,		S2MPS25_PMIC_INT7, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_SR1M_INT7,		S2MPS25_PMIC_INT7, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_SR2M_INT7,		S2MPS25_PMIC_INT7, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_SR3M_INT7,		S2MPS25_PMIC_INT7, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L20M_INT7,		S2MPS25_PMIC_INT7, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_WDT_INT7,			S2MPS25_PMIC_INT7, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_PARITY_ERR2_INT7,		S2MPS25_PMIC_INT7, 1 << 7),

	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L1M_INT8,		S2MPS25_PMIC_INT8, 1 << 0),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L5M_INT8,		S2MPS25_PMIC_INT8, 1 << 1),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L6M_INT8,		S2MPS25_PMIC_INT8, 1 << 2),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L7M_INT8,		S2MPS25_PMIC_INT8, 1 << 3),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L12M_INT8,		S2MPS25_PMIC_INT8, 1 << 4),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L14M_INT8,		S2MPS25_PMIC_INT8, 1 << 5),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L18M_INT8,		S2MPS25_PMIC_INT8, 1 << 6),
	DECLARE_IRQ(S2MPS25_PMIC_IRQ_OI_L19M_INT8,		S2MPS25_PMIC_INT8, 1 << 7),

	DECLARE_IRQ(S2MPS25_ADC_IRQ_NTC0_OVER_INTP,		S2MPS25_ADC_INTP, 1 << 3),
	DECLARE_IRQ(S2MPS25_ADC_IRQ_NTC1_OVER_INTP,		S2MPS25_ADC_INTP, 1 << 4),
};

static void s2mps25_irq_lock(struct irq_data *data)
{
	struct s2mps25_dev *s2mps25 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mps25->irqlock);
}

static void s2mps25_irq_sync_unlock(struct irq_data *data)
{
	struct s2mps25_dev *s2mps25 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mps25_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mps25, i);

		if (mask_reg == S2MPS25_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mps25->irq_masks_cache[i] = s2mps25->irq_masks_cur[i];

		s2mps25_write_reg(i2c, s2mps25_mask_reg[i],
				s2mps25->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mps25->irqlock);
}

static const inline struct s2mps25_irq_data *
irq_to_s2mps25_irq(struct s2mps25_dev *s2mps25, int irq)
{
	return &s2mps25_irqs[irq - s2mps25->irq_base];
}

static void s2mps25_irq_mask(struct irq_data *data)
{
	struct s2mps25_dev *s2mps25 = irq_get_chip_data(data->irq);
	const struct s2mps25_irq_data *irq_data =
	    irq_to_s2mps25_irq(s2mps25, data->irq);

	if (irq_data->group >= S2MPS25_IRQ_GROUP_NR)
		return;

	s2mps25->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mps25_irq_unmask(struct irq_data *data)
{
	struct s2mps25_dev *s2mps25 = irq_get_chip_data(data->irq);
	const struct s2mps25_irq_data *irq_data =
	    irq_to_s2mps25_irq(s2mps25, data->irq);

	if (irq_data->group >= S2MPS25_IRQ_GROUP_NR)
		return;

	s2mps25->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mps25_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mps25_irq_lock,
	.irq_bus_sync_unlock	= s2mps25_irq_sync_unlock,
	.irq_mask		= s2mps25_irq_mask,
	.irq_unmask		= s2mps25_irq_unmask,
};

static void s2mps25_report_irq(struct s2mps25_dev *s2mps25)
{
	int i;

	/* Apply masking */
	for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mps25->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPS25_IRQ_NR; i++)
		if (irq_reg[s2mps25_irqs[i].group] & s2mps25_irqs[i].mask)
			handle_nested_irq(s2mps25->irq_base + i);
}

static int s2mps25_key_detection(struct s2mps25_dev *s2mps25, int key)
{
	int ret, i;
	uint8_t val, reg_int, reg_status, mask_status, flag_status, key_release, key_press;

	switch (key) {
	case KEY_POWER:
		reg_int = S2MPS25_PMIC_INT1;
		reg_status = S2MPS25_REG_STATUS1;
		mask_status = 0x03;
		flag_status = S2MPS25_STATUS1_PWRON;
		key_release = S2MPS25_PWRKEY_RELEASE;
		key_press = S2MPS25_PWRKEY_PRESS;
		break;
	case KEY_VOLUMEDOWN:
		reg_int = S2MPS25_PMIC_INT3;
		reg_status = S2MPS25_REG_STATUS1;
		mask_status = 0xC0;
		flag_status = S2MPS25_STATUS1_MRB;
		key_release = S2MPS25_VOLDN_RELEASE;
		key_press = S2MPS25_VOLDN_PRESS;
		break;
	default:
		return 0;
	}

	/* Determine release/press edge for PWR_ON key */
	if ((irq_reg[reg_int] & mask_status) == mask_status) {
		ret = s2mps25_read_reg(s2mps25->pmic1, reg_status, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[reg_int] &= (~mask_status);

		if (val & flag_status) {
			irq_reg[reg_int] = key_release;
			s2mps25_report_irq(s2mps25);

			/* clear irq */
			for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[reg_int] = key_press;
		} else {
			irq_reg[reg_int] = key_press;
			s2mps25_report_irq(s2mps25);

			/* clear irq */
			for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[reg_int] = key_release;
		}
	}
	return 0;

power_key_err:
	return -1;
}

static void s2mps25_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	int i, cnt = 0;

	for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++) {
		if (i == S2MPS25_ADC_INTP)
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "ADC_INTP(%#02hhx) ", irq_reg[i]);
		else
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(%#02hhx) ", i + 1, irq_reg[i]);
	}

	pr_info("[PMIC] %s: main pmic interrupt %s\n", __func__, buf);
}

static void s2mps25_pending_clear(struct s2mps25_dev *s2mps25)
{
	uint32_t val = 0;

	val = readl(s2mps25->sysreg_pending);
	writel(val, s2mps25->sysreg_pending);

	val = readl(s2mps25->sysreg_pending2);
	writel(val, s2mps25->sysreg_pending2);
}

static irqreturn_t s2mps25_irq_thread(int irq, void *data)
{
	struct s2mps25_dev *s2mps25 = data;
	uint8_t vgi_src[S2MPS25_VGI_CNT] = {0};
	uint32_t val;
	int i, ret, key;

	/* Clear interrupt pending bit */
	s2mps25_pending_clear(s2mps25);

	/* Read VGPIO_RX_MONITOR */
	val = readl(s2mps25->mem_base);

	for (i = 0; i < S2MPS25_VGI_CNT; i++) {
		if (i == 4) /* VOL_DN */
			val = readl(s2mps25->mem_base2);

		vgi_src[i] = val & 0x0F;
		val = (val >> 8);
	}

	if (!(vgi_src[1] & S2MPS25_VGI0_IRQ_M) && !(vgi_src[1] & S2MPS25_VGI0_IRQ_S1)) {
		pr_info("[PMIC] %s: r0(%#hhx) r1(%#hhx) r2(%#hhx) r3(%#hhx) r4(%#hhx)\n",
			__func__, vgi_src[0], vgi_src[1], vgi_src[2], vgi_src[3], vgi_src[4]);
		return IRQ_HANDLED;
	}

	/* notify Main PMIC */
	if (vgi_src[1] & S2MPS25_VGI0_IRQ_M) {
		ret = s2mps25_bulk_read(s2mps25->pmic1, S2MPS25_REG_INT1,
				S2MPS25_NUM_IRQ_PMIC_REGS, &irq_reg[S2MPS25_PMIC_INT1]);
		if (ret) {
			pr_err("%s: %s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}

		ret = s2mps25_bulk_read(s2mps25->adc_i2c, S2MPS25_REG_ADC_INTP,
				S2MPS25_NUM_IRQ_ADC_REGS, &irq_reg[S2MPS25_ADC_INTP]);
		if (ret) {
			pr_err("%s: %s Failed to read power-meter interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}

		// queue_delayed_work(s2mps25->irq_wqueue, &s2mps25->irq_work, 0);
		pr_info("[PMIC] %s: %#hhx %#hhx %#hhx %#hhx %#hhx\n",
				__func__, irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3], irq_reg[4]);

		/* Power-key W/A */
		if ((irq_reg[S2MPS25_PMIC_INT3] & 0xC0) == 0xC0)
			key = KEY_VOLUMEDOWN;
		else
			key = KEY_POWER;
		ret = s2mps25_key_detection(s2mps25, key);
		if (ret < 0)
			pr_err("%s: KEY(%d) detection error\n", __func__, key);

		/* Report IRQ */
		s2mps25_report_irq(s2mps25);
	}
	/* notify Sub PMIC */
	if (vgi_src[1] & S2MPS25_VGI0_IRQ_S1) {
		pr_info("%s: IBI from sub pmic\n", __func__);
		s2mps26_call_notifier();
	}

	return IRQ_HANDLED;
}

static int s2mps25_set_wqueue(struct s2mps25_dev *s2mps25)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mps25->dev), dev_name(s2mps25->dev));

	s2mps25->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mps25->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return 1;
	}

	INIT_DELAYED_WORK(&s2mps25->irq_work, s2mps25_irq_work_func);

	return 0;
}

static void s2mps25_set_vgpio_monitor(struct s2mps25_dev *s2mps25)
{
	s2mps25->mem_base = ioremap(VGPIO_I3C_BASE + VGPIO_MONITOR_ADDR, SZ_32);
	if (s2mps25->mem_base == NULL)
		pr_info("%s: fail to allocate mem_base\n", __func__);

	/* VOL_DN */
	s2mps25->mem_base2 = ioremap(VGPIO_I3C_BASE + VGPIO_MONITOR_ADDR2, SZ_32);
	if (s2mps25->mem_base2 == NULL)
		pr_info("%s: fail to allocate mem_base2\n", __func__);

	s2mps25->sysreg_pending = ioremap(SYSREG_VGPIO2AP + INTC0_IPRIO_PEND, SZ_32);
	if (s2mps25->sysreg_pending == NULL)
		pr_info("%s: fail to allocate sysreg_pending\n", __func__);

	s2mps25->sysreg_pending2 = ioremap(SYSREG_VGPIO2PMU + INTC0_IPRIO_PEND, SZ_32);
	if (s2mps25->sysreg_pending2 == NULL)
		pr_info("%s: fail to allocate sysreg_pending\n", __func__);
}

int s2mps25_irq_init(struct s2mps25_dev *s2mps25)
{
	uint8_t i2c_data;
	int cur_irq, ret, i;
	static char irq_name[32] = {0, };

	if (!s2mps25->irq_base) {
		dev_err(s2mps25->dev, "[PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mps25->irqlock);

	/* Set VGPIO monitor */
	s2mps25_set_vgpio_monitor(s2mps25);

	/* Set workqueue */
	s2mps25_set_wqueue(s2mps25);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPS25_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mps25->irq_masks_cur[i] = 0xff;
		s2mps25->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mps25, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mps25_mask_reg[i] == S2MPS25_REG_INVALID)
			continue;

		s2mps25_write_reg(i2c, s2mps25_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPS25_IRQ_NR; i++) {
		cur_irq = i + s2mps25->irq_base;
		irq_set_chip_data(cur_irq, s2mps25);
		irq_set_chip_and_handler(cur_irq, &s2mps25_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	//s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_TX_MASK, 0xff);
	/* Unmask s2mps25 interrupt */
	ret = s2mps25_read_reg(s2mps25->i2c, S2MPS25_REG_TX_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s: %s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(S2MPS25_PM_IRQM_MASK | S2MPS25_ADC_IRQM_MASK);
	i2c_data |= S2MPS25_IRQ_TX_EN;
	s2mps25_write_reg(s2mps25->i2c, S2MPS25_REG_TX_MASK, i2c_data);

	/* VOL_DN IRQ enable */
	s2mps25_update_reg(s2mps25->pmic2, S2MPS25_REG_OFF_CTRL1, 0x20, 0x20);

	pr_info("%s: %s S2MPS25_PMIC_REG_IRQM=0x%02hhx\n",
		MFD_DEV_NAME, __func__, i2c_data);

	/* Dynamic allocation for device name */
	snprintf(irq_name, sizeof(irq_name) - 1, "%s-irq@%s",
		 dev_driver_string(s2mps25->dev), dev_name(s2mps25->dev));

	s2mps25->irq = irq_of_parse_and_map(s2mps25->dev->of_node, 0);
	ret = devm_request_threaded_irq(s2mps25->dev, s2mps25->irq, NULL, s2mps25_irq_thread,
				   IRQF_ONESHOT, irq_name, s2mps25);

	if (ret) {
		dev_err(s2mps25->dev, "Failed to request IRQ %d: %d\n",
			s2mps25->irq, ret);
		destroy_workqueue(s2mps25->irq_wqueue);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps25_irq_init);

void s2mps25_irq_exit(struct s2mps25_dev *s2mps25)
{
	if (s2mps25->irq)
		free_irq(s2mps25->irq, s2mps25);

	iounmap(s2mps25->mem_base);
	iounmap(s2mps25->sysreg_pending);
	iounmap(s2mps25->sysreg_pending2);

	cancel_delayed_work_sync(&s2mps25->irq_work);
	destroy_workqueue(s2mps25->irq_wqueue);
}
EXPORT_SYMBOL_GPL(s2mps25_irq_exit);
