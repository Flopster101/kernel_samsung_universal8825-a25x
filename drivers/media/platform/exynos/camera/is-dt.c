/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if IS_ENABLED(CONFIG_OF)
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <exynos-is.h>

#include "is-config.h"
#include "is-dt.h"
#include "is-core.h"
#include "is-dvfs.h"
#include "is-interface-sensor.h"
#include "is-device-camif-dma.h"

static int get_pin_lookup_state(struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX])
{
	int ret = 0;
	u32 i, j, k;
	char pin_name[30];
	struct pinctrl_state *s;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(pin_name, sizeof(pin_name), "%s%d",
						pin_ctrls[i][j][k].name,
						pin_ctrls[i][j][k].value);
					s = pinctrl_lookup_state(pinctrl, pin_name);
					if (IS_ERR_OR_NULL(s)) {
						err("pinctrl_lookup_state(%s) is failed", pin_name);
						ret = -EINVAL;
						goto p_err;
					}

					pin_ctrls[i][j][k].pin = (ulong)s;
				}
			}
		}
	}

p_err:
	return ret;
}

static int parse_gate_info(struct exynos_platform_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_is_clk_gate_info *gate_info;

	/* get subip of is info */
	gate_info = kzalloc(sizeof(struct exynos_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_get_child_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

#if IS_ENABLED(CONFIG_PM_DEVFREQ)
static int parse_dvfs_data(struct exynos_platform_is *pdata, struct device_node *np)
{
	int i, cnt;
	u32 temp;
	char *pprop;
	const char *name;
	char qos_name_l[IS_DVFS_END][IS_STR_LEN];
	u32 dvfs_t;

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++) {
		if (!pdata->qos_name[dvfs_t])
			continue;

		cnt = 0;
		name = pdata->qos_name[dvfs_t];
		while (name[cnt] != '\0' && cnt < IS_STR_LEN - 1) {
			qos_name_l[dvfs_t][cnt] = tolower(name[cnt]);
			cnt++;
		}
		qos_name_l[dvfs_t][cnt] = '\0';
	}

	pprop = __getname();
	if (unlikely(!pprop))
		return -ENOMEM;

	for (i = 0; i < IS_SN_END; i++) {
		for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++) {
			if (!pdata->qos_name[dvfs_t])
				continue;

			snprintf(pprop, PATH_MAX, "%s%s",
				is_dvfs_dt_arr[i].parse_scenario_nm, qos_name_l[dvfs_t]);
			DT_READ_U32_DEFAULT(np, pprop,
				pdata->dvfs_data[is_dvfs_dt_arr[i].scenario_id][dvfs_t],
				IS_DVFS_LV_END);
		}

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "hpg");
		DT_READ_U32_DEFAULT(np, pprop,
			pdata->dvfs_data[is_dvfs_dt_arr[i].scenario_id][IS_DVFS_HPG], 1);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "cpu");
		DT_READ_STR(np, pprop, pdata->dvfs_cpu[is_dvfs_dt_arr[i].scenario_id]);
	}
	__putname(pprop);

#ifdef DBG_DUMP_DVFS_DT
	for (i = 0; i < IS_SN_END; i++) {
		probe_info("---- %s ----\n", is_dvfs_dt_arr[i].parse_scenario_nm);

		for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++) {
			if (pdata->qos_name[dvfs_t])
				probe_info("[%d][%s] = %d\n", i,
					pdata->qos_name[dvfs_t],
					pdata->dvfs_data[i][dvfs_t]);
		}

		probe_info("[%d][HPG] = %d\n", i, pdata->dvfs_data[i][IS_DVFS_HPG]);
		probe_info("[%d][CPU] = %s\n", i, pdata->dvfs_cpu[i]);
	}
#endif
	return 0;
}
#else
static int parse_dvfs_data(struct exynos_platform_is *pdata, struct device_node *np)
{
	return 0;
}
#endif

static int parse_qos_table(struct exynos_platform_is *pdata, struct device_node *np)
{
	u32 dvfs_typ = 0;
	int elems, lv;
	struct device_node *next;

	for_each_child_of_node(np, next) {
		if (of_find_property(next, "typ", NULL)) {
			of_property_read_u32(next, "typ", &dvfs_typ);
		} else {
			probe_warn("dvfs_typ read is fail");
			return -EINVAL;
		}

		elems = of_property_count_u32_elems(next, "lev");
		if (elems >= IS_DVFS_LV_END) {
			probe_warn("too many elements");
			continue;
		}

		pdata->qos_otf[dvfs_typ] = of_property_read_bool(next, "otf");
		pdata->qos_name[dvfs_typ] = next->name;

		for (lv = 0; lv < elems; lv++) {
			of_property_read_u32_index(next, "lev", lv,
					&pdata->qos_tb[dvfs_typ][lv][IS_DVFS_VAL_LEV]);
			of_property_read_u32_index(next, "frq", lv,
					&pdata->qos_tb[dvfs_typ][lv][IS_DVFS_VAL_FRQ]);
			of_property_read_u32_index(next, "qos", lv,
					&pdata->qos_tb[dvfs_typ][lv][IS_DVFS_VAL_QOS]);
		}
	}

	return 0;
}

static int parse_dvfs_table(struct is_dvfs_ctrl *dvfs,
	struct exynos_platform_is *pdata, struct device_node *np)
{
	int ret = 0;
	int table_cnt = 0;
	struct device_node *table_np = NULL;
	const char *dvfs_table_desc;

	while ((table_np = of_get_next_child(np, table_np))) {
		ret = of_property_read_string(table_np, "desc", &dvfs_table_desc);
		if (ret)
			dvfs_table_desc = "NOT defined";

		probe_info("dvfs table[%d] is %s", table_cnt, dvfs_table_desc);
		parse_dvfs_data(pdata, table_np);
		table_cnt++;
	}

	return ret;
}

static int bts_parse_data(struct device_node *np, struct is_bts_scen **data, int *num_bts_scen)
{
	int i;
	int ret = 0;
	int num_scen;
	struct is_bts_scen *bts_scen;

	if (of_have_populated_dt()) {
		num_scen = (unsigned int)of_property_count_strings(np, "list-scen-bts");
		if (num_scen <= 0) {
			probe_warn("There should be at least one scenario\n");
			goto err_of_property;
		}

		bts_scen = kcalloc(num_scen, sizeof(struct is_bts_scen), GFP_KERNEL);
		if (bts_scen == NULL) {
			ret = -ENOMEM;
			probe_err("no memory for bts_scen");
			goto err_alloc;
		}

		for (i = 0; i < num_scen; i++) {
			bts_scen[i].index = i;
			ret = of_property_read_string_index(np, "list-scen-bts", i, &(bts_scen[i].name));
			if (ret < 0) {
				probe_err("Unable to get name of bts scenarios\n");
				goto err_read_string;
			}
		}

		*data = bts_scen;
		*num_bts_scen = num_scen;
	} else {
		probe_warn("Invalid device tree node!\n");
	}

	return 0;

err_read_string:
	kfree(bts_scen);
err_alloc:
err_of_property:

	return ret;
}

static int parse_lic_offset_data(struct is_hardware *is_hw, struct device_node *dnode)
{
	int ret = 0;
	char *str_lic_ip;
	int elems;
	u32 offsets = LIC_CHAIN_OFFSET_NUM / 2 - 1;
	u32 set_idx = offsets + 1;
	int i, index_a, index_b;
	char *str_a, *str_b;

	str_lic_ip = __getname();
	if (unlikely(!str_lic_ip)) {
		err("[@] out of memory for str_lic_ip!");
		ret = -ENOMEM;
		goto err_alloc_str_lic_ip;
	}

	snprintf(str_lic_ip, PATH_MAX, "3AA");
	if (of_property_read_bool(dnode, str_lic_ip)) {
		elems = of_property_count_u32_elems(dnode, str_lic_ip);
		if (elems != LIC_CHAIN_OFFSET_NUM) {
			err("wrong LIC_CHAIN_OFFSET_NUM(%d!=%d)", elems, LIC_CHAIN_OFFSET_NUM);
			ret = -EINVAL;
			goto err_get_elems;
		}

		of_property_read_u32_array(dnode, str_lic_ip,
				&is_hw->lic_offset_def[0], elems);
	} else {
		err("[@] Can't fine %s node", str_lic_ip);
	}

	probe_info("[@] Parse_lic_offset_data\n");

	index_a = COREX_SETA * set_idx; /* setA */
	index_b = COREX_SETB * set_idx; /* setB */

	str_a = __getname();
	if (unlikely(!str_a)) {
		err("[@] out of memory for str_a!");
		ret = -ENOMEM;
		goto err_alloc_str_a;
	}

	str_b = __getname();
	if (unlikely(!str_b)) {
		err("[@] out of memory for str_b!");
		ret = -ENOMEM;
		goto err_alloc_str_b;
	}

	snprintf(str_a, PATH_MAX, "%d", is_hw->lic_offset_def[index_a + 0]);
	snprintf(str_b, PATH_MAX, "%d", is_hw->lic_offset_def[index_b + 0]);
	for (i = 1; i < offsets; i++) {
		snprintf(str_a, PATH_MAX, "%s, %d", str_a, is_hw->lic_offset_def[index_a + i]);
		snprintf(str_b, PATH_MAX, "%s, %d", str_b, is_hw->lic_offset_def[index_b + i]);
	}
	probe_info("[@] 3AA lic_offset_def: setA<%s>(%d), setB<%s>(%d)",
		str_a, is_hw->lic_offset_def[index_a + offsets],
		str_b, is_hw->lic_offset_def[index_b + offsets]);

	__putname(str_b);
err_alloc_str_b:
	__putname(str_a);
err_alloc_str_a:
err_get_elems:
	__putname(str_lic_ip);
err_alloc_str_lic_ip:

	return ret;
}

static int parse_phy_ldos(struct device *dev, struct regulator ***ldos, int *num)
{
	struct device_node *np = dev->of_node;
	int i, ret;
	const char *name;

	*num = of_property_count_strings(np, "phy_ldos");
	if (IS_ERR_VALUE((unsigned long)*num)) {
		probe_info("no power controls for CSI PHYs\n");
		return 0;
	}

	*ldos = kcalloc(*num, sizeof(struct regulator *), GFP_KERNEL);
	if (!(*ldos)) {
		probe_err("failed to allocate memory for PHY regulators\n");
		*num = 0;
		return -ENOMEM;
	}

	for (i = 0; i < *num; i++) {
		ret = of_property_read_string_index(np, "phy_ldos", i, &name);
		if (ret) {
			probe_err("failed get the name of PHY ldo%d: %d\n", i, ret);
			goto err_read_name;
		}

		(*ldos)[i] = regulator_get(dev, name);
		if (IS_ERR((*ldos)[i])) {
			probe_err("failed get regulator - %s: %d\n", name, (*ldos)[i]);
			ret = PTR_ERR((*ldos)[i]);
			goto err_get_regulator;
		}

		probe_info("LDO[%d] for CSI PHYs: %s was probed\n", i, name);
	}

	return 0;

err_get_regulator:
err_read_name:
	while (i-- > 0)
		regulator_put((*ldos)[i]);
	kfree(*ldos);
	*num = 0;

	return ret;
}

static int parse_itmon_port_name(struct device_node *np, struct is_resourcemgr *resourcemgr)
{
	int ret;
	int num;
	const char **name;

	num = of_property_count_strings(np, "itmon_port_name");
	if (num <= 0) {
		probe_warn("There should be at least one port");
		ret = -ENODEV;
		goto err_of_property;
	}

	name = kcalloc(num, sizeof(char *), GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		probe_err("Out of memory for itmon_port_name");
		goto err_alloc;
	}

	ret = of_property_read_string_array(np, "itmon_port_name", name, num);
	if (ret < 0) {
		probe_err("Unable to get itmon_port_name\n");
		goto err_read_string;
	}

	resourcemgr->itmon_port_num = num;
	resourcemgr->itmon_port_name = name;

	return 0;

err_read_string:
	kfree(*name);

err_alloc:
err_of_property:

	return ret;
}

static int parse_cpu_data(struct device_node *np, struct exynos_platform_is *pdata)
{
	struct device_node *cluster_np = NULL;

	cluster_np = of_get_child_by_name(np, "cpus");
	if (cluster_np) {
		of_property_read_u32(cluster_np, "cluster0", &pdata->cpu_cluster[0]);
		of_property_read_u32(cluster_np, "cluster1", &pdata->cpu_cluster[1]);
		of_property_read_u32(cluster_np, "cluster2", &pdata->cpu_cluster[2]);
	} else {
		pdata->cpu_cluster[0] = 0;
		pdata->cpu_cluster[1] = 4;
		pdata->cpu_cluster[2] = 7;
	}

	probe_info("cpu_cluster(%d, %d, %d)",
		pdata->cpu_cluster[0], pdata->cpu_cluster[1], pdata->cpu_cluster[2]);

	return 0;
}

int is_chain_dev_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs;
	struct is_bts_scen **bts_scen;
	struct exynos_platform_is *pdata;
	struct device *dev;
	struct device_node *qos_np = NULL;
	struct device_node *dvfs_np = NULL;
	struct device_node *vender_np = NULL;
	struct device_node *lic_offset_np = NULL;
	struct device_node *buffer_np = NULL;
	struct device_node *np;
	u32 mem_info[2];

	FIMC_BUG(!pdev);

	dev = &pdev->dev;
	np = dev->of_node;

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct exynos_platform_is), GFP_KERNEL);
	if (!pdata) {
		probe_err("no memory for platform data");
		return -ENOMEM;
	}

	dvfs = &core->resourcemgr.dvfs_ctrl;
	bts_scen = &core->resourcemgr.bts_scen;

	is_get_clk_ops(pdata);

	parse_itmon_port_name(np, &core->resourcemgr);

	parse_cpu_data(np, pdata);

	of_property_read_u32(np, "num_of_3aa", &pdata->num_of_ip.taa);
	of_property_read_u32(np, "num_of_lme", &pdata->num_of_ip.lme);
	of_property_read_u32(np, "num_of_orbmch", &pdata->num_of_ip.orbmch);
	of_property_read_u32(np, "num_of_isp", &pdata->num_of_ip.isp);
	of_property_read_u32(np, "num_of_byrp", &pdata->num_of_ip.byrp);
	of_property_read_u32(np, "num_of_rgbp", &pdata->num_of_ip.rgbp);
	of_property_read_u32(np, "num_of_mcsc", &pdata->num_of_ip.mcsc);
	of_property_read_u32(np, "num_of_vra", &pdata->num_of_ip.vra);
	of_property_read_u32(np, "num_of_ypp", &pdata->num_of_ip.ypp);
	of_property_read_u32(np, "num_of_mcfp", &pdata->num_of_ip.mcfp);

	if (parse_gate_info(pdata, np) < 0)
		probe_info("can't parse clock gate info node");

	ret = of_property_read_u32(np, "chain_config", &core->chain_config);
	if (ret)
		probe_warn("chain configuration read is fail(%d)", ret);

	probe_info("FIMC-IS chain configuration: %d\n", core->chain_config);

	ret = of_property_read_u32_array(np, "secure_mem_info", mem_info, 2);
	if (ret) {
		core->secure_mem_info[0] = 0;
		core->secure_mem_info[1] = 0;
	} else {
		core->secure_mem_info[0] = mem_info[0];
		core->secure_mem_info[1] = mem_info[1];
	}
	probe_info("ret(%d) secure_mem_info(%#08lx, %#08lx)", ret,
		core->secure_mem_info[0], core->secure_mem_info[1]);

	ret = of_property_read_u32_array(np, "non_secure_mem_info", mem_info, 2);
	if (ret) {
		core->non_secure_mem_info[0] = 0;
		core->non_secure_mem_info[1] = 0;
	} else {
		core->non_secure_mem_info[0] = mem_info[0];
		core->non_secure_mem_info[1] = mem_info[1];
	}
	probe_info("ret(%d) non_secure_mem_info(%#08lx, %#08lx)", ret,
		core->non_secure_mem_info[0], core->non_secure_mem_info[1]);

	ret = of_property_read_u32_array(np, "memlog_size", pdata->is_memlog_size, 2);
	if (ret) {
		pdata->is_memlog_size[0] = IS_MEMLOG_PRINTF_DRV_SIZE;
		pdata->is_memlog_size[1] = IS_MEMLOG_PRINTF_DDK_SIZE;
	}
	probe_info("ret(%d) is_memlog_size(0x%#08lx, 0x%#08lx)", ret,
		pdata->is_memlog_size[0], pdata->is_memlog_size[1]);

	vender_np = of_get_child_by_name(np, "vender");
	if (vender_np) {
		ret = is_vender_dt(vender_np);
		if (ret)
			probe_err("is_vender_dt is fail(%d)", ret);
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	qos_np = of_get_child_by_name(np, "is_qos");
	if (qos_np) {
		ret = parse_qos_table(pdata, qos_np);
		if (ret)
			probe_err("parse_qos_table is fail(%d)", ret);
	}

	dvfs_np = of_get_child_by_name(np, "is_dvfs");
	if (dvfs_np) {
		ret = parse_dvfs_table(dvfs, pdata, dvfs_np);
		if (ret)
			probe_err("parse_dvfs_table is fail(%d)", ret);
	}

	bts_parse_data(np, bts_scen, &core->resourcemgr.num_bts_scen);

	lic_offset_np = of_get_child_by_name(np, "lic_offsets");
	if (lic_offset_np) {
		ret = parse_lic_offset_data(&core->hardware, lic_offset_np);
		if (ret)
			probe_err("parse_lic_offset_data is fail(%d)", ret);
	}

	buffer_np = of_get_child_by_name(np, "buffer_info");
	if (buffer_np) {
		of_property_read_u32(buffer_np, "ypp_buf_max_width",
				&core->hardware.ypp_internal_buf_max_width);
	}

	ret = parse_phy_ldos(dev, &core->resourcemgr.phy_ldos,
				&core->resourcemgr.num_phy_ldos);
	if (ret)
		goto p_err;


	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

int is_sensor_dev_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct exynos_platform_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	FIMC_BUG(!pdev);
	FIMC_BUG(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_is_sensor), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	is_sensor_get_clk_ops(pdata);

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto err_read_id;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto err_read_scenario;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto err_read_csi_ch;
	}

	ret = of_property_read_u32(dnode, "wdma_ch_hint", &pdata->wdma_ch_hint);
	if (ret) {
		probe_info("skip wdma_ch_hint data read (%d)", ret);
		pdata->wdma_ch_hint = INIT_WDMA_CH_HINT;
	}

	ret = of_property_read_u32(dnode, "csi_mux", &pdata->csi_mux);
	if (ret) {
		probe_info("skip phy-csi mux data read (%d)", ret);
	}

	ret = of_property_read_u32(dnode, "multi_ch", &pdata->multi_ch);
	if (ret) {
		probe_info("skip multi_ch bool data read (%d)", ret);
	}

	ret = of_property_read_u32(dnode, "use_cphy", &pdata->use_cphy);
	if (ret)
		probe_info("use_cphy read is fail(%d)", ret);

	ret = of_property_read_u32(dnode, "scramble", &pdata->scramble);
	if (ret)
		probe_info("scramble read is fail(%d)", ret);

	ret = of_property_read_u32(dnode, "i2c_dummy_enable", &pdata->i2c_dummy_enable);
	if (ret)
		probe_info("i2c_dummy_enable is false(%d)", ret);

	pdev->id = pdata->id;
	dev->platform_data = pdata;

	return 0;

err_read_csi_ch:
err_read_scenario:
err_read_id:
	kfree(pdata);

	return ret;
}

static int is_board_cfg_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	int ret;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		return ret;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		probe_err("mclk_ch read is fail(%d)", ret);
		return ret;
	}

	if (of_property_read_u32(dnode, "mclk_freq", &pdata->mclk_freq_khz)) {
		pdata->mclk_freq_khz = 26000;
		probe_info("mclk_freq set as default 26Mhz\n");
	} else {
		probe_info("mclk_freq set as %d.%dMhz\n", pdata->mclk_freq_khz / 1000,
			pdata->mclk_freq_khz - (pdata->mclk_freq_khz / 1000) * 1000);
	}

	if (of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch))
		probe_info("i2c_ch dt parsing skipped\n");

	if (of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr))
		probe_info("i2c_addr dt parsing skipped\n");

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		probe_err("position read is fail(%d)", ret);
		return ret;
	}

	return ret;
}

static void is_module_cfg_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	if (of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id))
		probe_warn("sensor_id read is skipped");

	if (of_property_read_u32(dnode, "active_width", &pdata->active_width))
		probe_warn("active_width read is skipped");

	if (of_property_read_u32(dnode, "active_height", &pdata->active_height))
		probe_warn("active_height read is skipped");

	if (of_property_read_u32(dnode, "margin_left", &pdata->margin_left))
		probe_warn("margin_left read is skipped");

	if (of_property_read_u32(dnode, "margin_right", &pdata->margin_right))
		probe_warn("margin_right read is skipped");

	if (of_property_read_u32(dnode, "margin_top", &pdata->margin_top))
		probe_warn("margin_top read is skipped");

	if (of_property_read_u32(dnode, "margin_bottom", &pdata->margin_bottom))
		probe_warn("margin_bottom read is skipped");

	if (of_property_read_u32(dnode, "max_framerate", &pdata->max_framerate))
		probe_warn("max_framerate read is skipped");

	if (of_property_read_u32(dnode, "bitwidth", &pdata->bitwidth))
		probe_warn("bitwidth read is skipped");

	if (of_property_read_u32(dnode, "use_retention_mode", &pdata->use_retention_mode))
		probe_warn("use_retention_mode read is skipped");

	if (of_property_read_u32(dnode, "use_binning_ratio_table", &pdata->use_binning_ratio_table))
		probe_warn("use_binning_ratio_table read is skipped");

	if (of_property_read_string(dnode, "sensor_maker", (const char **)&pdata->sensor_maker))
		probe_warn("sensor_maker read is skipped");

	if (of_property_read_string(dnode, "sensor_name", (const char **)&pdata->sensor_name))
		probe_warn("sensor_name read is skipped");

	if (of_property_read_string(dnode, "setfile_name", (const char **)&pdata->setfile_name))
		probe_warn("setfile_name read is skipped");

	if (of_property_read_u32(dnode, "sensor_module_type", &pdata->sensor_module_type)) {
		probe_warn("sensor_module_type read is skipped");
		pdata->sensor_module_type = SENSOR_TYPE_RGB;
	}
}

static void is_module_rom_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	if (of_property_read_u32(dnode, "rom_id", &pdata->rom_id)) {
		probe_info("rom_id dt parsing skipped\n");
		pdata->rom_id = ROM_ID_NOTHING;
	}

	if (of_property_read_u32(dnode, "rom_type", &pdata->rom_type)) {
		probe_info("rom_type dt parsing skipped\n");
		pdata->rom_type = ROM_TYPE_NONE;
	}

	if (of_property_read_u32(dnode, "rom_cal_index", &pdata->rom_cal_index)) {
		probe_info("rom_cal_index dt parsing skipped\n");
		pdata->rom_cal_index = ROM_CAL_NOTHING;
	}

	if (of_property_read_u32(dnode, "rom_dualcal_id", &pdata->rom_dualcal_id)) {
		probe_info("rom_dualcal_id dt parsing skipped\n");
		pdata->rom_dualcal_id = ROM_ID_NOTHING;
	}

	if (of_property_read_u32(dnode, "rom_dualcal_index", &pdata->rom_dualcal_index)) {
		probe_info("rom_dualcal_index dt parsing skipped\n");
		pdata->rom_dualcal_index = ROM_DUALCAL_NOTHING;
	}
}

static int parse_af_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->af_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->af_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->af_i2c_ch);

	return 0;
}

static int parse_flash_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->flash_product_name);
	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	return 0;
}

static int parse_ois_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->ois_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->ois_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->ois_i2c_ch);

	return 0;
}

static int parse_mcu_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->mcu_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->mcu_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->mcu_i2c_ch);

	return 0;
}

static int parse_aperture_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->aperture_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->aperture_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->aperture_i2c_ch);

	return 0;
}

static int parse_eeprom_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->eeprom_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->eeprom_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->eeprom_i2c_ch);

	return 0;
}

static int parse_laser_af_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->laser_af_product_name);

	return 0;
}

static void is_module_peris_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	struct device_node *cis_np;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *ois_np;
	struct device_node *mcu_np;
	struct device_node *aperture_np;
	struct device_node *eeprom_np;
	struct device_node *laser_af_np;
	u32 temp;
	char *pprop;

	cis_np = of_get_child_by_name(dnode, "cis");
	if (cis_np) {
		DT_READ_U32_DEFAULT(cis_np, "reg",
				    pdata->sensor_i2c_addr,
				    pdata->sensor_i2c_addr);
		pdata->cis_np = cis_np;
	}

	af_np = of_get_child_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
		DT_READ_U32_DEFAULT(af_np, "reg",
			    pdata->af_i2c_addr,
			    pdata->af_i2c_addr);
		pdata->af_np = af_np;
	}

	flash_np = of_get_child_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	ois_np = of_get_child_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
		DT_READ_U32_DEFAULT(ois_np, "reg",
				    pdata->ois_i2c_addr,
				    pdata->ois_i2c_addr);
		pdata->ois_np = ois_np;
	}

	mcu_np = of_get_child_by_name(dnode, "mcu");
	if (!mcu_np)
		pdata->mcu_product_name = MCU_NAME_NOTHING;
	else
		parse_mcu_data(pdata, mcu_np);

	aperture_np = of_get_child_by_name(dnode, "aperture");
	if (!aperture_np)
		pdata->aperture_product_name = APERTURE_NAME_NOTHING;
	else
		parse_aperture_data(pdata, aperture_np);

	eeprom_np = of_find_node_by_name(dnode, "eeprom");
	if (!eeprom_np) {
		pdata->eeprom_product_name = EEPROM_NAME_NOTHING;
	} else {
		parse_eeprom_data(pdata, eeprom_np);
		DT_READ_U32_DEFAULT(eeprom_np, "reg",
				    pdata->eeprom_i2c_addr,
				    pdata->eeprom_i2c_addr);
		pdata->eeprom_np = eeprom_np;
	}

	laser_af_np = of_get_child_by_name(dnode, "laser_af");
	if (!laser_af_np)
		pdata->laser_af_product_name = LASER_AF_NAME_NOTHING;
	else
		parse_laser_af_data(pdata, laser_af_np);
}

static int is_cis_vc_extra_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	struct device_node *vc_extra_np;
	int idx_stat;
	int idx_dt;
	char *str_stat;

	vc_extra_np = of_get_child_by_name(dnode, "vc_extra");
	if (!vc_extra_np) {
		probe_warn("sensor vc_extra are not declared to DT");
		return 0;
	}

	str_stat = __getname();
	if (!str_stat) {
		err("out of memory for str_stat.");
		return -ENOMEM;
	}

	for (idx_stat = 0; idx_stat < VC_BUF_DATA_TYPE_MAX; idx_stat++) {
		idx_dt = 0;
		snprintf(str_stat, PATH_MAX, "%s%d", "stat", idx_stat);
		if (!of_find_property(vc_extra_np, str_stat, NULL)) {
			pdata->vc_extra_info[idx_stat].stat_type = VC_STAT_TYPE_INVALID;
			pdata->vc_extra_info[idx_stat].sensor_mode = VC_SENSOR_MODE_INVALID;
			pdata->vc_extra_info[idx_stat].max_width = 0;
			pdata->vc_extra_info[idx_stat].max_height = 0;
			pdata->vc_extra_info[idx_stat].max_element = 0;
			continue;
		}

		of_property_read_u32_index(vc_extra_np, str_stat, idx_dt++,
						&pdata->vc_extra_info[idx_stat].stat_type);
		of_property_read_u32_index(vc_extra_np, str_stat, idx_dt++,
						&pdata->vc_extra_info[idx_stat].sensor_mode);
		of_property_read_u32_index(vc_extra_np, str_stat, idx_dt++,
						&pdata->vc_extra_info[idx_stat].max_width);
		of_property_read_u32_index(vc_extra_np, str_stat, idx_dt++,
						&pdata->vc_extra_info[idx_stat].max_height);
		of_property_read_u32_index(vc_extra_np, str_stat, idx_dt++,
						&pdata->vc_extra_info[idx_stat].max_element);
	}

	if (of_property_read_u32(vc_extra_np, "stat_vc", &pdata->stat_vc)) {
		probe_warn("Use VC1 for PD stat out");
		pdata->stat_vc = CSI_VIRTUAL_CH_1;
	}

	__putname(str_stat);

	return 0;
}

static void is_cis_opt_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata, struct is_sensor_cfg *cfg)
{
	struct device_node *opt_np;
	u32 idx_vc;
	u32 dummy_idx;
	u32 vc_num;
	u32 elems;

	opt_np = of_get_child_by_name(dnode, "option");

	/* default value */
	cfg->votf = 0;
	cfg->wdma_ch_hint = INIT_WDMA_CH_HINT;
	cfg->max_fps = cfg->framerate;
	cfg->binning = min(BINNING(pdata->active_width, cfg->width),
			BINNING(pdata->active_height, cfg->height));
	cfg->vvalid_time = 0;
	cfg->req_vvalid_time = 0;
	cfg->special_mode = -1;

	for (idx_vc = 0; idx_vc < CSI_VIRTUAL_CH_MAX; idx_vc++)
		cfg->dummy_pixel[idx_vc] = 0;

	cfg->fid_loc.valid = 0;
	cfg->dvfs_lv_csis = IS_DVFS_LV_END;

	if (opt_np) {
		if (of_find_property(opt_np, "votf", NULL))
			of_property_read_u32(opt_np, "votf", &cfg->votf);

		if (of_find_property(opt_np, "wdma_ch_hint", NULL))
			of_property_read_u32(opt_np, "wdma_ch_hint", &cfg->wdma_ch_hint);

		if (of_find_property(opt_np, "max_fps", NULL))
			of_property_read_u32(opt_np, "max_fps", &cfg->max_fps);

		if (of_find_property(opt_np, "binning", NULL))
			of_property_read_u32(opt_np, "binning", &cfg->binning);

		if (of_find_property(opt_np, "vvalid_time", NULL))
			of_property_read_u32(opt_np, "vvalid_time", &cfg->vvalid_time);

		if (of_find_property(opt_np, "req_vvalid_time", NULL))
			of_property_read_u32(opt_np, "req_vvalid_time", &cfg->req_vvalid_time);

		if (of_find_property(opt_np, "special_mode", NULL))
			of_property_read_u32(opt_np, "special_mode", &cfg->special_mode);

		if (of_find_property(opt_np, "dummy_pixel", NULL)) {
			elems = of_property_count_u32_elems(opt_np, "dummy_pixel");
			if (elems % SENSOR_DUMMY_ELEMS) {
				err("the length of dummy info. is not a multiple of SENSOR_DUMMY_ELEMS");
			} else {
				dummy_idx = 0;
				while (elems > 0) {
					of_property_read_u32_index(opt_np, "dummy_pixel",
								dummy_idx++, &vc_num);
					of_property_read_u32_index(opt_np, "dummy_pixel",
								dummy_idx++, &cfg->dummy_pixel[vc_num]);
					elems = elems - 2;
				}
			}
		}

		if (of_find_property(opt_np, "fid_loc", NULL)) {
			of_property_read_u32_index(opt_np, "fid_loc", 0, &cfg->fid_loc.line);
			of_property_read_u32_index(opt_np, "fid_loc", 1, &cfg->fid_loc.byte);
			cfg->fid_loc.valid = 1;
		}

		if (of_find_property(opt_np, "dvfs_lv_csis", NULL))
			of_property_read_u32(opt_np, "dvfs_lv_csis", &cfg->dvfs_lv_csis);

#ifdef USE_EX_MODE_EXTRA
		if (opt_np && of_find_property(opt_np, "ex_mode_extra", NULL))
			of_property_read_u32(opt_np, "ex_mode_extra", &cfg->ex_mode_extra);
		else
			cfg->ex_mode_extra = EX_EXTRA_NONE;
#else
		cfg->ex_mode_extra = EX_EXTRA_NONE;
#endif
	}
}

static int is_cis_modes_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	struct device_node *modes_np;
	struct device_node *next;
	int idx_mode;
	int idx_vc;
	int idx_dt;
	char *str_vc;
	struct is_sensor_cfg *cfg;
	u32 format;
	int ret = 0;

	modes_np = of_get_child_by_name(dnode, "modes");
	if (!modes_np) {
		probe_warn("sensor modes are not declared to DT");
		return 0;
	}

	str_vc = __getname();
	if (!str_vc) {
		err("out of memory for str_vc.");
		ret = -ENOMEM;
		goto err_alloc_str_vc;
	}

	pdata->cfgs = of_get_child_count(modes_np);
	pdata->cfg = kcalloc(pdata->cfgs, sizeof(struct is_sensor_cfg), GFP_KERNEL);
	if (!pdata->cfg) {
		err("out of memory for sensor modes.");
		ret = -ENOMEM;
		goto err_alloc_sensor_mode;

	}

	idx_mode = 0;
	for_each_child_of_node(modes_np, next) {
		idx_dt = 0;
		cfg = &pdata->cfg[idx_mode];
		idx_mode++;

		of_property_read_u32_index(next, "common", idx_dt++, &cfg->width);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->height);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->framerate);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->settle);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->mode);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->lanes);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->mipi_speed);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->interleave_mode);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->lrte);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->pd_mode);
		of_property_read_u32_index(next, "common", idx_dt++, (u32 *)&cfg->ex_mode);

		for (idx_vc = 0; idx_vc < CSI_VIRTUAL_CH_MAX; idx_vc++) {
			idx_dt = 0;
			snprintf(str_vc, PATH_MAX,  "%s%d", "vc", idx_vc);

			if (!of_find_property(next, str_vc, NULL))
				break;

			/* input format pasing */
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].map);
			of_property_read_u32_index(next, str_vc, idx_dt++, &format);
			cfg->input[idx_vc].hwformat = format & HW_FORMAT_MASK;
			cfg->input[idx_vc].extformat = format & HW_EXT_FORMAT_MASK;

			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].width);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].height);

			/* output format pasing */
			of_property_read_u32_index(next, str_vc, idx_dt++, &format);
			cfg->output[idx_vc].hwformat = format & HW_FORMAT_MASK;
			cfg->output[idx_vc].extformat = format & HW_EXT_FORMAT_MASK;

			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].type);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].width);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].height);
		}
		cfg->max_vc = idx_vc - 1;

		is_cis_opt_parse_dt(next, pdata, cfg);
	}

err_alloc_sensor_mode:
	__putname(str_vc);
err_alloc_str_vc:
	return ret;
}

static int parse_power_seq_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	int ret = 0;
	u32 temp;
	char *pprop;
	struct device_node *sn_np, *seq_np;
	struct is_core *core;
	struct device_node **node_table;
	struct device_node *temp_node;
	int num;
	long *node_num;
	long temp_num;
	int i, j;
	int gpio_mclk;

	core = is_get_is_core();
	if (!core) {
		probe_err("core is NULL");
		return -EINVAL;
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (gpio_is_valid(gpio_mclk)) {
		if (gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW")) {
			probe_err("%s: failed to gpio request mclk\n", dnode->name);
			return -ENODEV;
		}
		gpio_free(gpio_mclk);
	} else {
		probe_err("%s: failed to get mclk\n", dnode->name);
		return -EINVAL;
	}

	for_each_child_of_node(dnode, sn_np) {
		u32 sensor_scenario, gpio_scenario;

		DT_READ_U32(sn_np, "sensor_scenario", sensor_scenario);
		DT_READ_U32(sn_np, "gpio_scenario",gpio_scenario);

		probe_info("power_seq[%s] : sensor_scenario=%d, gpio_scenario=%d\n",
			sn_np->name, sensor_scenario, gpio_scenario);

		SET_PIN_INIT(pdata, sensor_scenario, gpio_scenario);

		num = of_get_child_count(sn_np);
		node_table = kcalloc(num, sizeof(*node_table), GFP_KERNEL);
		if (!node_table) {
			err("out of memory for node_table[%s].", sn_np->name);
			return -ENOMEM;
		}

		node_num = kcalloc(num, sizeof(*node_num), GFP_KERNEL);
		if (!node_num) {
			err("out of memory for node_num[%s].", sn_np->name);
			kfree(node_table);
			return -ENOMEM;
		}

		i = 0;
		for_each_child_of_node(sn_np, seq_np) {
			node_table[i] = seq_np;
			ret = kstrtol(seq_np->name, 10, &node_num[i]);
			if (ret) {
				kfree(node_table);
				kfree(node_num);
				err("fail to kstrtol [%d]%s:%s.", i, sn_np->name, seq_np->name);
				return ret;
			}
			i++;
		}

		/* sorting */
		for (i = 0; i < num; i++) {
			for (j = i + 1; j < num; j++) {
				if (node_num[i] > node_num[j]) {
					temp_node = node_table[i];
					temp_num = node_num[i];

					node_table[i] = node_table[j];
					node_num[i] = node_num[j];

					node_table[j] = temp_node;
					node_num[j] = temp_num;
				}
			}
		}

		for (i = 0; i < num; i++) {
			int gpio;
			int idx_share;
			int idx_seq;
			int idx_prop;
			struct exynos_sensor_pin *pin;
			struct property *prop;
			int length;

			prop = of_find_property(node_table[i], "pname", &length);
			if (!prop || length <= 1)
				continue;

			idx_seq = pdata->pinctrl_index[sensor_scenario][gpio_scenario];
			pin = &pdata->pin_ctrls[sensor_scenario][gpio_scenario][idx_seq];
			pdata->pinctrl_index[sensor_scenario][gpio_scenario]++;

			/* of_get_named_gpio does not guarantee about device tree
			 * format for gpio.
			 *
			 * ex) { pname = "pin"; pin = <PIN_OUTPUT 0 0>; gpio = <&gpp 1 0x1>; };
			 *
			 * If do not describe property of gpio correctly,
			 * of_get_named_gpio is returned zero value.
			 * therefore, this need to prevent.
			 * length is depends on cell(size is 4) count.
			 */
			prop = of_find_property(node_table[i], "gpio", &length);
			if (!prop || length < 12) {
				pin->pin = -1;
			} else {
				gpio = of_get_named_gpio(node_table[i], "gpio", 0);
				if (gpio_is_valid(gpio)) {
					pin->pin = gpio;
					gpio_request_one(gpio, GPIOF_OUT_INIT_LOW,
							"CAM_GPIO_OUTPUT_LOW");
					gpio_free(gpio);
				}
			}

			of_property_read_string(node_table[i], "pname", (const char **)&pin->name);

			idx_prop = 0;
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->act);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->value);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->delay);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->voltage);

			idx_prop = 0;
			if (of_find_property(node_table[i], "share", NULL)) {
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &pin->shared_rsc_type);
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &idx_share);
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &pin->shared_rsc_active);
				pin->shared_rsc_slock = &core->shared_rsc_slock[idx_share];
				pin->shared_rsc_count = &core->shared_rsc_count[idx_share];
			} else {
				pin->shared_rsc_type = 0;
				pin->shared_rsc_slock = NULL;
				pin->shared_rsc_count = NULL;
				pin->shared_rsc_active = 0;
			}

			if (of_find_property(node_table[i], "actuator_i2c_delay", NULL)) {
				of_property_read_u32(node_table[i], "actuator_i2c_delay", &pin->actuator_i2c_delay);
			}

			if (pin->act == PIN_REGULATOR) {
				bool reg_cap = true;
				struct is_module_regulator *is_module_regulator;
				struct device *dev = &core->pdev->dev;
				struct regulator *regulator;

				list_for_each_entry(is_module_regulator, &core->resourcemgr.regulator_list, list) {
					if (!strcmp(is_module_regulator->name, pin->name)) {
						reg_cap = false;
						break;
					}
				}

				if (reg_cap) {
					is_module_regulator = kzalloc(sizeof(struct is_module_regulator), GFP_KERNEL);
					if (!is_module_regulator) {
						err("%s: failed to alloc is_regulator", __func__);
						return -ENOMEM;
					}

					is_module_regulator->name = pin->name;
					list_add_tail(&is_module_regulator->list, &core->resourcemgr.regulator_list);
					dbg("%s is registered in regulator_list", is_module_regulator->name);

					/* If sensor voltage source is already enabled due to specific reason.
					 * Set regulator_force_disable, then maintain initial state
					 */
					regulator = regulator_get_optional(dev, pin->name);
					if (IS_ERR_OR_NULL(regulator)) {
						err("%s: failed to get regulator", (pin->name == 0 ? "null" : pin->name));
						return PTR_ERR(regulator);
					}

					if (regulator_is_enabled(regulator)) {
						warn("regulator(%s) is already enabled, forcely disable\n", pin->name);
						ret = regulator_force_disable(regulator);
						if (ret)
							err("regulator_force_disable fail(%d)\n", ret);
					}
					regulator_put(regulator);
				}
			}

			dbg("%s: gpio=%d, name=%s, act=%d, val=%d, delay=%d, volt=%d, share=<%d %d>\n",
				node_table[i]->name,
				pin->pin,
				pin->name,
				pin->act,
				pin->value,
				pin->delay,
				pin->voltage,
				pin->shared_rsc_type,
				pin->shared_rsc_active);
		}

		kfree(node_table);
		kfree(node_num);
	}

	return 0;
}

static void is_module_match_seq_parse_dt(struct device_node *dnode,
		struct exynos_platform_is_module *pdata)
{
	struct device_node *match_np;
	struct device_node *group_np;
	int num_entry;
	int i, j;
	int idx_prop;
	struct exynos_sensor_module_match *entry;

	match_np = of_get_child_by_name(dnode, "match_seq");
	if (!match_np)
		return;

	/*
	 * A match seq node is divided into group and entry.
	 * Group can be configured as many as MATCH_GROUP_MAX,
	 * and each group can have as many entries as MATCH_ENTRY_MAX.
	 * Each entry consists of a slave_addr, reg offset, number of byte, and expected value
	 * to compare the result with i2c transfer.
	 */
	i = 0;
	for_each_child_of_node(match_np, group_np) {
		num_entry = of_property_count_elems_of_size((group_np),
				"entry", sizeof(u32)) / 5;
		for (j = 0; j < num_entry; j++) {
			entry = &pdata->match_entry[i][j];
			idx_prop = j * 5;
			of_property_read_u32_index(group_np, "entry", idx_prop++,
							&entry->slave_addr);
			of_property_read_u32_index(group_np, "entry", idx_prop++,
							&entry->reg);
			of_property_read_u32_index(group_np, "entry", idx_prop++,
							&entry->reg_type);
			of_property_read_u32_index(group_np, "entry", idx_prop++,
							&entry->expected_data);
			of_property_read_u32_index(group_np, "entry", idx_prop++,
							&entry->data_type);
			probe_info("%s: slave_addr(0x%04x), reg(0x%04x), reg_type(%d), \
					expected_data(0x%04x), data_type(%d)\n",
					__func__, entry->slave_addr, entry->reg,
					entry->reg_type, entry->expected_data,
					entry->data_type);
		}
		pdata->num_of_match_entry[i++] = num_entry;
	}
	pdata->num_of_match_groups = i;
}

int is_sensor_module_parse_dt(struct device *dev,
	int (*module_callback)(struct device *, struct exynos_platform_is_module *))
{
	int ret;
	struct exynos_platform_is_module *pdata;
	struct device_node *dnode;
	struct device_node *power_np;
	u32 use = 0;

	FIMC_BUG(!dev);
	FIMC_BUG(!dev->of_node);

	dnode = dev->of_node;
	pdata = kzalloc(sizeof(struct exynos_platform_is_module), GFP_KERNEL);
	if (!pdata) {
		probe_err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	is_get_gpio_ops(pdata);

	ret = is_board_cfg_parse_dt(dnode, pdata);
	if (ret)
		goto p_err;

	is_module_cfg_parse_dt(dnode, pdata);
	is_module_rom_parse_dt(dnode, pdata);
	is_module_peris_parse_dt(dnode, pdata);

	ret = is_cis_vc_extra_parse_dt(dnode, pdata);
	if (ret)
		goto p_err;

	ret = is_cis_modes_parse_dt(dnode, pdata);
	if (ret)
		goto p_err;

	power_np = of_get_child_by_name(dnode, "power_seq");
	if (power_np) {
		ret = of_property_read_u32(power_np, "use", &use);
		if (ret)
			probe_warn("use read is skipped");
	}

	if (power_np && use) {
		ret = parse_power_seq_data(pdata, power_np);
		if (ret) {
			probe_err("%s: parse_power_seq_data(%d)", power_np->full_name, ret);
			goto p_err;
		}
	} else {
		if (module_callback) {
			ret = module_callback(dev, pdata);
			if (ret) {
				probe_err("sensor dt callback is fail(%d)", ret);
				goto p_err;
			}
		} else {
			ret = -EINVAL;
			probe_err("sensor dt callback is NULL");
			goto p_err;
		}
	}

	is_module_match_seq_parse_dt(dnode, pdata);

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		probe_err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

#if IS_ENABLED(CONFIG_SPI)
int is_spi_dev_parse_dt(struct is_spi *spi)
{
	int ret = 0;
	struct device_node *np;
	struct device *dev;
	struct pinctrl_state *s;

	FIMC_BUG(!spi);

	dev = &spi->device->dev;

	np = of_find_compatible_node(NULL,NULL, spi->node);
	if(np == NULL) {
		probe_err("compatible: fail to read, spi_parse_dt");
		ret = -ENODEV;
		goto p_err;
	}

	spi->use_spi_pinctrl = of_property_read_bool(np, "use_spi_pinctrl");
	if (!spi->use_spi_pinctrl) {
		probe_info("%s: spi dt parsing skipped\n", __func__);
		goto p_err;
	}

	spi->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(spi->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_out");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_out");
		spi->pin_ssn_out = NULL;
	} else {
		spi->pin_ssn_out = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_fn");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_fn");
		spi->pin_ssn_fn = NULL;
	} else {
		spi->pin_ssn_fn = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpd");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpd");
		spi->pin_ssn_inpd = NULL;
	} else {
		spi->pin_ssn_inpd = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpu");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpu");
		spi->pin_ssn_inpu = NULL;
	} else {
		spi->pin_ssn_inpu = s;
	}

	spi->parent_pinctrl = devm_pinctrl_get(spi->device->dev.parent->parent);

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_out");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_out");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_out = s;

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_fn");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_fn");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_fn = s;

p_err:
	return ret;
}
#endif /* CONFIG_SPI */
#endif /* CONFIG_OF */
