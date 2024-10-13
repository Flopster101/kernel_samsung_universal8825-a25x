#ifndef __ASV_S5E8825_H__
#define __ASV_S5E8825_H__

#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sec_debug.h>

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

static struct dentry *rootdir;

static struct asv_power_model {
	unsigned power_model_0:8;
	unsigned reserved_0:16;
	unsigned power_version:8;
	unsigned power_model_1:8;
	unsigned power_model_3:8;
	unsigned reserved_1:16;
	unsigned power_model_2:8;
	unsigned power_model_4:8;
	unsigned reserved_2:16;
} power_model;

struct asv_tbl_info {
	unsigned bigcpu_asv_group:4;
	unsigned gnss_asv_group:4;
	unsigned litcpu_asv_group:4;
	unsigned g3d_asv_group:4;
	unsigned mif_asv_group:4;
	unsigned int_asv_group:4;
	unsigned cam_asv_group:4;
	unsigned npu_asv_group:4;

	unsigned cp_cpu_asv_group:4;
	unsigned cp_asv_group:4;
	unsigned dsu_asv_group:4;
	unsigned intcore_asv_group:4;
	unsigned intcore_modify_group:4;
	unsigned dsu_modify_group:4;
	unsigned cp_modify_group:4;
	unsigned cp_cpu_modify_group:4;

	unsigned bigcpu_modify_group:4;
	unsigned gnss_modify_group:4;
	unsigned litcpu_modify_group:4;
	unsigned g3d_modify_group:4;
	unsigned mif_modify_group:4;
	unsigned int_modify_group:4;
	unsigned cam_modify_group:4;
	unsigned npu_modify_group:4;

	unsigned asv_table_version:7;
	unsigned asv_method:1;
	unsigned asb_version:7;
	unsigned rsvd_0:1;
	unsigned asvg_version:4;
	unsigned asv_reserved4:12;
};

struct id_tbl_info {
	unsigned rsvd_0;

	unsigned chip_id_2;

	unsigned chip_id_1:13;
	unsigned rsvd_1:3;
	unsigned char ids_bigcpu:8;
	unsigned char ids_g3d:8;

	unsigned char ids_int_cam:8;    /* int,cam */
	unsigned asb_version:8;
	unsigned char ids_midcpu:8;
	unsigned char ids_litcpu:8;

	unsigned char ids_mif:8;
	unsigned char ids_npu:8;
	unsigned short sub_rev:4;
	unsigned short main_rev:4;
	unsigned rsvd_2:8;

	unsigned rsvd_3:8;
	unsigned rsvd_4:8;
	unsigned rsvd_5:8;
	unsigned char ids_cp:8;
};

#define ASV_INFO_ADDR_CNT	(sizeof(struct asv_tbl_info) / 4)
#define ID_INFO_ADDR_CNT	(sizeof(struct id_tbl_info) / 4)

static struct asv_tbl_info asv_tbl;
static struct id_tbl_info id_tbl;

int asv_get_grp(unsigned int id)
{
	int grp = -1;

	switch (id) {
	case CPUCL1:
		grp = asv_tbl.bigcpu_asv_group + asv_tbl.bigcpu_modify_group;
		break;
	case GNSS:
		grp = asv_tbl.gnss_asv_group + asv_tbl.gnss_modify_group;
		break;
	case CPUCL0:
		grp = asv_tbl.litcpu_asv_group + asv_tbl.litcpu_modify_group;
		break;
	case G3D:
		grp = asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group;
		break;
	case MIF:
		grp = asv_tbl.mif_asv_group + asv_tbl.mif_modify_group;
		break;
	case INT:
		grp = asv_tbl.int_asv_group + asv_tbl.int_modify_group;
		break;
	case CAM:
		grp = asv_tbl.cam_asv_group + asv_tbl.cam_modify_group;
		break;
	case NPU:
		grp = asv_tbl.npu_asv_group + asv_tbl.npu_modify_group;
		break;
	case CP_CPU:
		grp = asv_tbl.cp_cpu_asv_group + asv_tbl.cp_cpu_modify_group;
		break;
	case CP:
		grp = asv_tbl.cp_asv_group + asv_tbl.cp_modify_group;
		break;
	case DSU:
		grp = asv_tbl.dsu_asv_group + asv_tbl.dsu_modify_group;
		break;
	case INTSCI:
		grp = asv_tbl.intcore_asv_group + asv_tbl.intcore_modify_group;
		break;
	default:
		pr_info("Un-support asv grp %d\n", id);
	}

	return grp;
}

int asv_get_ids_info(unsigned int id)
{
	int ids = 0;

	switch (id) {
	case CPUCL1:
		ids = id_tbl.ids_bigcpu;
		break;
	case G3D:
		ids = id_tbl.ids_g3d;
		break;
	case INT:
	case CAM:
		ids = id_tbl.ids_int_cam;
		break;
	case CPUCL0:
		ids = id_tbl.ids_litcpu;
		break;
	case MIF:
		ids = id_tbl.ids_mif;
		break;
	case NPU:
		ids = id_tbl.ids_npu;
		break;
	default:
		pr_info("Un-support ids info %d\n", id);
	}

	return ids;
}

int asv_get_table_ver(void)
{
	return asv_tbl.asv_table_version;
}

void id_get_rev(unsigned int *main_rev, unsigned int *sub_rev)
{
	*main_rev = id_tbl.main_rev;
	*sub_rev =  id_tbl.sub_rev;
}

int id_get_product_line(void)
{
	return id_tbl.chip_id_1 >> 10;
} EXPORT_SYMBOL(id_get_product_line);

int id_get_asb_ver(void)
{
	return id_tbl.asb_version;
} EXPORT_SYMBOL(id_get_asb_ver);

#if IS_ENABLED(CONFIG_SHOW_ASV)
int print_asv_table(char *buf)
{
	int size = 0;

	size += sprintf(buf + size, "chipid : 0x%04x%08x\n", id_tbl.chip_id_1, id_tbl.chip_id_2);
	size += sprintf(buf + size, "main revision : %d\n", id_tbl.main_rev);
	size += sprintf(buf + size, "sub revision : %d\n", id_tbl.sub_rev);
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  asv_table_version : %d\n", asv_tbl.asv_table_version);
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  big cpu grp : %d, ids : %d\n",
	       asv_tbl.bigcpu_asv_group + asv_tbl.bigcpu_modify_group,
	       id_tbl.ids_bigcpu);
	size += sprintf(buf + size, "  gnss cpu grp : %d\n",
	       asv_tbl.gnss_asv_group + asv_tbl.gnss_modify_group);
	size += sprintf(buf + size, "  little cpu grp : %d, ids : %d\n",
	       asv_tbl.litcpu_asv_group + asv_tbl.litcpu_modify_group, id_tbl.ids_litcpu);
	size += sprintf(buf + size, "  g3d grp : %d, ids : %d\n",
	       asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group, id_tbl.ids_g3d);
	size += sprintf(buf + size, "  mif grp : %d, ids : %d\n",
	       asv_tbl.mif_asv_group + asv_tbl.mif_modify_group, id_tbl.ids_mif);
	size += sprintf(buf + size, "  int grp : %d, ids : %d\n",
	       asv_tbl.int_asv_group + asv_tbl.int_modify_group, id_tbl.ids_int_cam);
	size += sprintf(buf + size, "  cam grp : %d, ids : %d\n",
	       asv_tbl.cam_asv_group + asv_tbl.cam_modify_group, id_tbl.ids_int_cam);
	size += sprintf(buf + size, "  npu grp : %d, ids : %d\n",
	       asv_tbl.npu_asv_group + asv_tbl.npu_modify_group, id_tbl.ids_npu);
	size += sprintf(buf + size, "  cp_cpu grp : %d\n",
	       asv_tbl.cp_cpu_asv_group + asv_tbl.cp_cpu_modify_group);
	size += sprintf(buf + size, "  cp grp : %d\n",
	       asv_tbl.cp_asv_group + asv_tbl.cp_modify_group);
	size += sprintf(buf + size, "  dsu grp : %d\n",
	       asv_tbl.dsu_asv_group + asv_tbl.dsu_modify_group);
	size += sprintf(buf + size, "  intcore grp : %d\n",
	       asv_tbl.intcore_asv_group + asv_tbl.intcore_modify_group);
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  asb_version : %d\n", id_tbl.asb_version);
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  power_model_0 : %d\n", power_model.power_model_0);
	size += sprintf(buf + size, "  power_model_1 : %d\n", power_model.power_model_1);
	size += sprintf(buf + size, "  power_model_2 : %d\n", power_model.power_model_2);
	size += sprintf(buf + size, "  power_model_3 : %d\n", power_model.power_model_3);
	size += sprintf(buf + size, "  power_model_4 : %d\n", power_model.power_model_4);
	size += sprintf(buf + size, "  power_version : %d\n", power_model.power_version);

	return size;
}

static ssize_t
asv_info_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[1023] = {0,};
	int r;
	r = print_asv_table(buf);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations asv_info_fops = {
	.open		= simple_open,
	.read		= asv_info_read,
	.llseek		= seq_lseek,
};

static ssize_t show_asv_info(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return print_asv_table(buf);
}
static struct kobj_attribute asv_info =
__ATTR(asv_info, 0400, show_asv_info, NULL);
#endif
static ssize_t show_power_model_0(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_model_0);
}
static struct kobj_attribute power_model_0 =
__ATTR(power_model_0, 0400, show_power_model_0, NULL);

static ssize_t show_power_model_1(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_model_1);
}
static struct kobj_attribute power_model_1 =
__ATTR(power_model_1, 0400, show_power_model_1, NULL);

static ssize_t show_power_model_2(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_model_2);
}
static struct kobj_attribute power_model_2 =
__ATTR(power_model_2, 0400, show_power_model_2, NULL);

static ssize_t show_power_model_3(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_model_3);
}
static struct kobj_attribute power_model_3 =
__ATTR(power_model_3, 0400, show_power_model_3, NULL);

static ssize_t show_power_model_4(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_model_4);
}
static struct kobj_attribute power_model_4 =
__ATTR(power_model_4, 0400, show_power_model_4, NULL);

static ssize_t show_power_version(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_model.power_version);
}
static struct kobj_attribute power_version =
__ATTR(power_version, 0400, show_power_version, NULL);

static struct attribute *asv_info_attrs[] = {
#if IS_ENABLED(CONFIG_SHOW_ASV)
	&asv_info.attr,
#endif
	&power_model_0.attr,
	&power_model_1.attr,
	&power_model_2.attr,
	&power_model_3.attr,
	&power_model_4.attr,
	&power_version.attr,
	NULL,
};

static const struct attribute_group asv_info_grp = {
	.attrs = asv_info_attrs,
};

int asv_debug_init(void)
{
	struct kobject *kobj;
#if IS_ENABLED(CONFIG_SHOW_ASV)
	struct dentry *d;

	rootdir = debugfs_create_dir("asv", NULL);
	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("asv_info", 0600, rootdir, NULL, &asv_info_fops);
	if (!d)
		return -ENOMEM;
#endif

	kobj = kobject_create_and_add("asv", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create asv kboject\n");
	if (sysfs_create_group(kobj, &asv_info_grp))
		pr_err("Fail to create asv_info group\n");

	return 0;
}

int asv_table_init(void)
{
	int i;
	unsigned int *p_table;
	unsigned int *regs;
	unsigned long tmp;

	p_table = (unsigned int *)&asv_tbl;

	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
		exynos_smc_readsfr((unsigned long)(ASV_TABLE_BASE + 0x4 * i), &tmp);
		*(p_table + i) = (unsigned int)tmp;
	}

	p_table = (unsigned int *)&id_tbl;

	regs = (unsigned int *)ioremap(ID_TABLE_BASE, ID_INFO_ADDR_CNT * sizeof(int));
	for (i = 0; i < ID_INFO_ADDR_CNT; i++)
		*(p_table + i) = (unsigned int)regs[i];

	p_table = (unsigned int *)&power_model;

	regs = (unsigned int *)ioremap(ID_TABLE_BASE + 0xF060, sizeof(struct asv_power_model));
	*p_table = *regs;
	*(p_table + 1) = *(unsigned int *)(regs + 1);
	*(p_table + 2) = *(unsigned int *)(regs + 2);

	asv_debug_init();

	return asv_tbl.asv_table_version;
}
#endif
