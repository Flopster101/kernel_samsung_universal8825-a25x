#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <soc/samsung/cal-if.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <soc/samsung/fvmap.h>

#include "cmucal.h"
#include "vclk.h"
#include "ra.h"

#define FVMAP_SIZE		(SZ_8K)
#define STEP_UV			(6250)

void __iomem *fvmap_base;
void __iomem *sram_fvmap_base;

static int init_margin_table[MAX_MARGIN_ID];
static int percent_margin_table[MAX_MARGIN_ID];

static int margin_mif;
static int margin_int;
static int margin_cpucl0;
static int margin_cpucl1;
static int margin_cpucl2;
static int margin_npu;
static int margin_dsu;
static int margin_disp;
static int margin_aud;
static int margin_cp_cpu;
static int margin_cp;
static int margin_cp_em;
static int margin_cp_mcw;
static int margin_g3d;
static int margin_intcam;
static int margin_cam;
static int margin_csis;
static int margin_isp;
static int margin_mfc0;
static int margin_mfc1;
static int margin_intsci;
static int margin_dsp;
static int margin_dnc;
static int margin_gnss;
static int margin_alive;
static int margin_chub;
static int margin_vts;
static int margin_hsi0;

static int margin_intg3d;
static int margin_wlbt;

static int volt_offset_percent;

module_param(margin_mif, int, 0);
module_param(margin_int, int, 0);
module_param(margin_cpucl0, int, 0);
module_param(margin_cpucl1, int, 0);
module_param(margin_cpucl2, int, 0);
module_param(margin_npu, int, 0);
module_param(margin_dsu, int, 0);
module_param(margin_disp, int, 0);
module_param(margin_aud, int, 0);
module_param(margin_cp_cpu, int, 0);
module_param(margin_cp, int, 0);
module_param(margin_cp_em, int, 0);
module_param(margin_cp_mcw, int, 0);
module_param(margin_g3d, int, 0);
module_param(margin_intcam, int, 0);
module_param(margin_cam, int, 0);
module_param(margin_csis, int, 0);
module_param(margin_isp, int, 0);
module_param(margin_mfc0, int, 0);
module_param(margin_mfc1, int, 0);
module_param(margin_intsci, int, 0);
module_param(margin_dsp, int, 0);
module_param(margin_dnc, int, 0);
module_param(margin_gnss, int, 0);
module_param(margin_alive, int, 0);
module_param(margin_chub, int, 0);
module_param(margin_vts, int, 0);
module_param(margin_hsi0, int, 0);

module_param(margin_intg3d, int, 0);
module_param(margin_wlbt, int, 0);
module_param(volt_offset_percent, int, 0);

void margin_table_init(void)
{
	init_margin_table[MARGIN_MIF] = margin_mif;
	init_margin_table[MARGIN_INT] = margin_int;
	init_margin_table[MARGIN_CPUCL0] = margin_cpucl0;
	init_margin_table[MARGIN_CPUCL1] = margin_cpucl1;
	init_margin_table[MARGIN_CPUCL2] = margin_cpucl2;
	init_margin_table[MARGIN_NPU] = margin_npu;
	init_margin_table[MARGIN_DSU] = margin_dsu;
	init_margin_table[MARGIN_DISP] = margin_disp;
	init_margin_table[MARGIN_AUD] = margin_aud;
	init_margin_table[MARGIN_CP_CPU] = margin_cp_cpu;
	init_margin_table[MARGIN_CP] = margin_cp;
	init_margin_table[MARGIN_CP_EM] = margin_cp_em;
	init_margin_table[MARGIN_CP_MCW] = margin_cp_mcw;
	init_margin_table[MARGIN_G3D] = margin_g3d;
	init_margin_table[MARGIN_INTCAM] = margin_intcam;
	init_margin_table[MARGIN_CAM] = margin_cam;
	init_margin_table[MARGIN_CSIS] = margin_csis;
	init_margin_table[MARGIN_ISP] = margin_isp;
	init_margin_table[MARGIN_MFC0] = margin_mfc0;
	init_margin_table[MARGIN_MFC1] = margin_mfc1;
	init_margin_table[MARGIN_INTSCI] = margin_intsci;
	init_margin_table[MARGIN_DSP] = margin_dsp;
	init_margin_table[MARGIN_DNC] = margin_dnc;
	init_margin_table[MARGIN_GNSS] = margin_gnss;
	init_margin_table[MARGIN_ALIVE] = margin_alive;
	init_margin_table[MARGIN_CHUB] = margin_chub;
	init_margin_table[MARGIN_VTS] = margin_vts;
	init_margin_table[MARGIN_HSI0] = margin_hsi0;

	init_margin_table[MARGIN_INTG3D] = margin_intg3d;
	init_margin_table[MARGIN_WLBT] = margin_wlbt;
}

int fvmap_set_raw_voltage_table(unsigned int id, int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int num_of_lv;
	int idx, i;

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		fv_table->table[i].volt += uV / STEP_UV;

	return 0;
}

static int get_vclk_id_from_margin_id(int margin_id)
{
	int size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	int i;
	struct vclk *vclk;

	for (i = 0; i < size; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		if (vclk->margin_id == margin_id)
			return i;
	}

	return -EINVAL;
}

#define attr_percent(margin_id, type)								\
static ssize_t show_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return snprintf(buf, PAGE_SIZE, "%d\n", percent_margin_table[margin_id]);		\
}												\
												\
static ssize_t store_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)		\
{												\
	int input, vclk_id;									\
												\
	if (!sscanf(buf, "%d", &input))								\
		return -EINVAL;									\
												\
	if (input < -100 || input > 100)							\
		return -EINVAL;									\
												\
	vclk_id = get_vclk_id_from_margin_id(margin_id);					\
	if (vclk_id == -EINVAL)									\
		return vclk_id;									\
	percent_margin_table[margin_id] = input;						\
	cal_dfs_set_volt_margin(vclk_id | ACPM_VCLK_TYPE, input);				\
												\
	return count;										\
}												\
												\
static struct kobj_attribute type##_percent =							\
__ATTR(type##_percent, 0600,									\
	show_##type##_percent, store_##type##_percent)

attr_percent(MARGIN_MIF, mif_margin);
attr_percent(MARGIN_INT, int_margin);
attr_percent(MARGIN_CPUCL0, cpucl0_margin);
attr_percent(MARGIN_CPUCL1, cpucl1_margin);
attr_percent(MARGIN_CPUCL2, cpucl2_margin);
attr_percent(MARGIN_NPU, npu_margin);
attr_percent(MARGIN_DSU, dsu_margin);
attr_percent(MARGIN_DISP, disp_margin);
attr_percent(MARGIN_AUD, aud_margin);
attr_percent(MARGIN_CP_CPU, cp_cpu_margin);
attr_percent(MARGIN_CP, cp_margin);
attr_percent(MARGIN_CP_EM, cp_em_margin);
attr_percent(MARGIN_CP_MCW, cp_mcw_margin);
attr_percent(MARGIN_G3D, g3d_margin);
attr_percent(MARGIN_INTCAM, intcam_margin);
attr_percent(MARGIN_CAM, cam_margin);
attr_percent(MARGIN_CSIS, csis_margin);
attr_percent(MARGIN_ISP, isp_margin);
attr_percent(MARGIN_MFC0, mfc0_margin);
attr_percent(MARGIN_MFC1, mfc1_margin);
attr_percent(MARGIN_INTSCI, intsci_margin);
attr_percent(MARGIN_DSP, dsp_margin);
attr_percent(MARGIN_DNC, dnc_margin);
attr_percent(MARGIN_GNSS, gnss_margin);
attr_percent(MARGIN_ALIVE, alive_margin);
attr_percent(MARGIN_CHUB, chub_margin);
attr_percent(MARGIN_VTS, vts_margin);
attr_percent(MARGIN_HSI0, hsi0_margin);

attr_percent(MARGIN_INTG3D, intg3d_margin);
attr_percent(MARGIN_WLBT, wlbt_margin);

static struct attribute *percent_margin_attrs[] = {
	&mif_margin_percent.attr,
	&int_margin_percent.attr,
	&cpucl0_margin_percent.attr,
	&cpucl1_margin_percent.attr,
	&cpucl2_margin_percent.attr,
	&npu_margin_percent.attr,
	&dsu_margin_percent.attr,
	&disp_margin_percent.attr,
	&aud_margin_percent.attr,
	&cp_cpu_margin_percent.attr,
	&cp_margin_percent.attr,
	&cp_em_margin_percent.attr,
	&cp_mcw_margin_percent.attr,
	&g3d_margin_percent.attr,
	&intcam_margin_percent.attr,
	&cam_margin_percent.attr,
	&csis_margin_percent.attr,
	&isp_margin_percent.attr,
	&mfc0_margin_percent.attr,
	&mfc1_margin_percent.attr,
	&intsci_margin_percent.attr,
	&dsp_margin_percent.attr,
	&dnc_margin_percent.attr,
	&gnss_margin_percent.attr,
	&alive_margin_percent.attr,
	&chub_margin_percent.attr,
	&vts_margin_percent.attr,
	&hsi0_margin_percent.attr,

	&intg3d_margin_percent.attr,
	&wlbt_margin_percent.attr,
	NULL,
};

static const struct attribute_group percent_margin_group = {
	.attrs = percent_margin_attrs,
};

unsigned int dvfs_calibrate_voltage(unsigned int rate_target, unsigned int rate_up,
		unsigned int rate_down, unsigned int volt_up, unsigned int volt_down)
{
	unsigned int volt_diff_step;
	unsigned int rate_diff;
	unsigned int rate_per_step;
	unsigned int ret;

	if (rate_up < 0x100)
		return volt_down * STEP_UV;

	if (rate_down == 0)
		return volt_up * STEP_UV;

	if ((rate_up == rate_down) || (volt_up == volt_down))
		return volt_up * STEP_UV;

	volt_diff_step = (volt_up - volt_down);
	rate_diff = rate_up - rate_down;
	rate_per_step = rate_diff / volt_diff_step;
	ret = (unsigned int)((volt_up - ((rate_up - rate_target) / rate_per_step)) + 0) * STEP_UV;

	return ret;
}

int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table, unsigned int table_size)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i, j;
	int num_of_lv;
	unsigned int target, rate_up, rate_down, volt_up, volt_down;
	struct freq_volt *table = (struct freq_volt *)freq_volt_table;

	if (!IS_ACPM_VCLK(id))
		return -EINVAL;

	if (!table)
		return -ENOMEM;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < table_size; i++) {
		for (j = 1; j < num_of_lv; j++) {
			rate_up = fv_table->table[j - 1].rate;
			rate_down = fv_table->table[j].rate;
			if ((table[i].rate <= rate_up) && (table[i].rate >= rate_down)) {
				volt_up = fv_table->table[j - 1].volt;
				volt_down = fv_table->table[j].volt;
				target = table[i].rate;
				table[i].volt = dvfs_calibrate_voltage(target,
						rate_up, rate_down, volt_up, volt_down);
				break;
			}
		}

		if (table[i].volt == 0) {
			if (table[i].rate > fv_table->table[0].rate)
				table[i].volt = fv_table->table[0].volt * STEP_UV;
			else if (table[i].rate < fv_table->table[num_of_lv - 1].rate)
				table[i].volt = fv_table->table[num_of_lv - 1].volt * STEP_UV;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_get_freq_volt_table);

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;

	if (!IS_ACPM_VCLK(id))
		return 0;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt * STEP_UV;

	return num_of_lv;

}
EXPORT_SYMBOL_GPL(fvmap_get_voltage_table);

int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt * STEP_UV;

	for (i = 0; i < num_of_lv; i++)
		printk("dvfs id : %d  %d Khz : %d uv\n", ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);

	return 0;
}

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	struct fvmap_header *fvmap_header, *header;
	struct rate_volt_header *old, *new;
	struct clocks *clks;
	struct pll_header *plls;
	struct vclk *vclk;
	unsigned int member_addr;
	unsigned int blk_idx;
	int size, margin;
	int i, j;

	fvmap_header = map_base;
	header = sram_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].domain_id = header[i].domain_id;
		fvmap_header[i].num_of_lv = header[i].num_of_lv;
		fvmap_header[i].num_of_members = header[i].num_of_members;
		fvmap_header[i].num_of_pll = header[i].num_of_pll;
		fvmap_header[i].num_of_mux = header[i].num_of_mux;
		fvmap_header[i].num_of_div = header[i].num_of_div;
		fvmap_header[i].o_famrate = header[i].o_famrate;
		fvmap_header[i].init_lv = header[i].init_lv;
		fvmap_header[i].num_of_child = header[i].num_of_child;
		fvmap_header[i].parent_id = header[i].parent_id;
		fvmap_header[i].parent_offset = header[i].parent_offset;
		fvmap_header[i].block_addr[0] = header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = header[i].block_addr[2];
		fvmap_header[i].o_members = header[i].o_members;
		fvmap_header[i].o_ratevolt = header[i].o_ratevolt;
		fvmap_header[i].o_tables = header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		pr_info("domain_id : %s - id : %x\n",
			vclk->name, fvmap_header[i].domain_id);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);
#endif
		old = sram_base + fvmap_header[i].o_ratevolt;
		new = map_base + fvmap_header[i].o_ratevolt;

		margin = init_margin_table[vclk->margin_id];
		if (margin)
			cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE, margin);

		if (volt_offset_percent) {
			if ((volt_offset_percent < 100) && (volt_offset_percent > -100)) {
				percent_margin_table[i] = volt_offset_percent;
				cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE, volt_offset_percent);
			}
		}

		for (j = 0; j < fvmap_header[i].num_of_members; j++) {
			clks = sram_base + fvmap_header[i].o_members;

			if (j < fvmap_header[i].num_of_pll) {
				plls = sram_base + clks->addr[j];
				member_addr = plls->addr - 0x90000000;
			} else {

				member_addr = (clks->addr[j] & ~0x3) & 0xffff;
				blk_idx = clks->addr[j] & 0x3;

				member_addr |= ((fvmap_header[i].block_addr[blk_idx]) << 16) - 0x90000000;
			}

			vclk->list[j] = cmucal_get_id_by_addr(member_addr);
#ifdef CONFIG_EXYNOS_DEBUG_INFO
			if (vclk->list[j] == INVALID_CLK_ID)
				pr_info("  Invalid addr :0x%x\n", member_addr);
			else
				pr_info("  DVFS CMU addr:0x%x\n", member_addr);
#endif
		}
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			new->table[j].rate = old->table[j].rate;
			new->table[j].volt = old->table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV (%d %%) \n",
				new->table[j].rate, new->table[j].volt * STEP_UV,
				volt_offset_percent);
		}
#endif
	}
}

int fvmap_init(void __iomem *sram_base)
{
	void __iomem *map_base;
	struct kobject *kobj;

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %p\n", __func__, sram_base);
	margin_table_init();
	fvmap_copy_from_sram(map_base, sram_base);

	/* percent margin for each doamin at runtime */
	kobj = kobject_create_and_add("percent_margin", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create percent_margin kboject\n");

	if (sysfs_create_group(kobj, &percent_margin_group))
		pr_err("Fail to create percent_margin group\n");

	kobj = kobject_create_and_add("asv-g", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create asv-g kboject\n");

	if (sysfs_create_group(kobj, &asv_g_spec_grp))
		pr_err("Fail to create asv_g_spec group\n");

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_init);

MODULE_LICENSE("GPL");
