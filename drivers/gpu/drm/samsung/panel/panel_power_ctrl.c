#include <linux/of.h>
#include <linux/device.h>

#include "panel_debug.h"
#include "panel_drv.h"
#include "panel_gpio.h"
#include "panel_regulator.h"

static u32 action_table[MAX_PANEL_POWER_CTRL_ACTION] = {
	[PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP] = (PARSE_VALUE),
	[PANEL_POWER_CTRL_ACTION_DELAY_USLEEP] = (PARSE_VALUE),
	[PANEL_POWER_CTRL_ACTION_REGULATOR_ENABLE] = (PARSE_REG),
	[PANEL_POWER_CTRL_ACTION_REGULATOR_DISABLE] = (PARSE_REG),
	[PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE] = (PARSE_REG | PARSE_VALUE),
	[PANEL_POWER_CTRL_ACTION_REGULATOR_SSD_CURRENT] = (PARSE_REG | PARSE_VALUE),
	[PANEL_POWER_CTRL_ACTION_GPIO_ENABLE] = (PARSE_GPIO),
	[PANEL_POWER_CTRL_ACTION_GPIO_DISABLE] = (PARSE_GPIO),
	[PANEL_POWER_CTRL_ACTION_REGULATOR_FORCE_DISABLE] = (PARSE_REG),
};

static int panel_power_ctrl_action_delay(struct panel_power_ctrl *pctrl,
	struct panel_power_ctrl_action *paction)
{
	if (!pctrl || !paction) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	if (paction->value == 0) {
		panel_warn("%s:%s delay must be grater than 0\n", pctrl->name, paction->name);
		return -EINVAL;
	}

	switch (paction->type) {
	case PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP:
		if (paction->value <= 20)
			usleep_range(paction->value * 1000, (paction->value * 1000) + 10);
		else
			msleep(paction->value);
		break;
	case PANEL_POWER_CTRL_ACTION_DELAY_USLEEP:
		usleep_range(paction->value, paction->value + 10);
		break;
	default:
		panel_err("%s:%s is not delay action\n", pctrl->name, paction->name);
		break;
	}
	return 0;
}

static int panel_power_ctrl_action_gpio(struct panel_power_ctrl *pctrl,
	struct panel_power_ctrl_action *paction)
{
	int ret = 0;

	if (!pctrl || !paction) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	switch (paction->type) {
	case PANEL_POWER_CTRL_ACTION_GPIO_ENABLE:
		if (!paction->gpio) {
			panel_err("%s %s gpio_enable: invalid gpio\n", pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_gpio_helper_set_value(paction->gpio, 1);
		if (ret < 0) {
			panel_err("failed to execute gpio_enable %s %s %s %d\n",
				pctrl->name, paction->name, paction->gpio->name, ret);
		}
		break;
	case PANEL_POWER_CTRL_ACTION_GPIO_DISABLE:
		if (!paction->gpio) {
			panel_err("%s %s gpio_disable: invalid gpio\n", pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_gpio_helper_set_value(paction->gpio, 0);
		if (ret < 0) {
			panel_err("failed to execute gpio_disable %s %s %s %d\n",
				pctrl->name, paction->name, paction->gpio->name, ret);
		}
		break;
	default:
		panel_err("%s:%s is not gpio action\n", pctrl->name, paction->name);
		break;
	}
	return ret;
}

static int panel_power_ctrl_action_regulator(struct panel_power_ctrl *pctrl,
	struct panel_power_ctrl_action *paction)
{
	int ret = 0;

	if (!pctrl || !paction) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	switch (paction->type) {
	case PANEL_POWER_CTRL_ACTION_REGULATOR_ENABLE:
		if (!paction->reg) {
			panel_err("%s %s regulator_enable: invalid regulator\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_regulator_helper_enable(paction->reg);
		if (ret < 0) {
			panel_err("failed to execute regulator_enable %s %s %s %d\n",
				pctrl->name, paction->name, paction->reg->node_name, ret);
		}
		break;
	case PANEL_POWER_CTRL_ACTION_REGULATOR_DISABLE:
		if (!paction->reg) {
			panel_err("%s %s regulator_disable: invalid regulator\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_regulator_helper_disable(paction->reg);
		if (ret < 0) {
			panel_err("failed to execute regulator_disable %s %s %s %d\n",
				pctrl->name, paction->name, paction->reg->node_name, ret);
		}
		break;
	case PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE:
		if (!paction->reg) {
			panel_err("%s %s regulator_set_voltage: invalid regulator\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		if (!paction->value) {
			panel_err("%s %s regulator_set_voltage: invalid value\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_regulator_helper_set_voltage(paction->reg, paction->value);
		if (ret < 0) {
			panel_err("failed to execute regulator_set_voltage %s %s %s %u %d\n",
				pctrl->name, paction->name, paction->reg->node_name, paction->value, ret);
		}
		break;
	case PANEL_POWER_CTRL_ACTION_REGULATOR_SSD_CURRENT:
		if (!paction->reg) {
			panel_err("%s %s regulator_ssd_current: invalid regulator\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_regulator_helper_set_current_limit(paction->reg, paction->value);
		if (ret < 0) {
			panel_err("failed to execute regulator_ssd_current %s %s %s %u %d\n",
				pctrl->name, paction->name, paction->reg->node_name, paction->value, ret);
		}
		break;
	case PANEL_POWER_CTRL_ACTION_REGULATOR_FORCE_DISABLE:
		if (!paction->reg) {
			panel_err("%s %s regulator_force_disable: invalid regulator\n",
				pctrl->name, paction->name);
			ret = -ENODEV;
			break;
		}
		ret = panel_regulator_helper_force_disable(paction->reg);
		if (ret < 0) {
			panel_err("failed to execute regulator_disable %s %s %s %d\n",
				pctrl->name, paction->name, paction->reg->node_name, ret);
		}
		break;
	default:
		panel_err("%s:%s is not regulator action\n", pctrl->name, paction->name);
		break;
	}
	return ret;
}

static int panel_power_ctrl_action_execute(struct panel_power_ctrl *pctrl)
{
	struct panel_power_ctrl_action *paction;
	int ret = 0;

	if (!pctrl)
		return -EINVAL;

	panel_info("%s +\n", pctrl->name);
	list_for_each_entry(paction, &pctrl->action_list, head) {
		panel_dbg("pctrl %s action %s type %d value %u\n",
			pctrl->name, paction->name, paction->type, paction->value);

		ret = 0;
		switch (paction->type) {
		case PANEL_POWER_CTRL_ACTION_NONE:
			break;
		case PANEL_POWER_CTRL_ACTION_REGULATOR_ENABLE:
		case PANEL_POWER_CTRL_ACTION_REGULATOR_DISABLE:
		case PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE:
		case PANEL_POWER_CTRL_ACTION_REGULATOR_SSD_CURRENT:
		case PANEL_POWER_CTRL_ACTION_REGULATOR_FORCE_DISABLE:
			ret = panel_power_ctrl_action_regulator(pctrl, paction);
			break;
		case PANEL_POWER_CTRL_ACTION_GPIO_ENABLE:
		case PANEL_POWER_CTRL_ACTION_GPIO_DISABLE:
			ret = panel_power_ctrl_action_gpio(pctrl, paction);
			break;
		case PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP:
		case PANEL_POWER_CTRL_ACTION_DELAY_USLEEP:
			ret = panel_power_ctrl_action_delay(pctrl, paction);
			break;
		default:
			panel_warn("%s:%s invalid action type %d\n",
					pctrl->name, paction->name, paction->type);
			ret = -ENODEV;
			break;
		}

		if (ret < 0) {
			panel_err("%s:%s action failed\n",
					pctrl->name, paction->name);
			return ret;
		}

		panel_dbg("%s:%s action done\n",
				pctrl->name, paction->name);
	}

	panel_info("%s -\n", pctrl->name);
	return 0;
}

__visible_for_testing struct panel_power_ctrl_funcs panel_power_ctrl_funcs = {
	.execute = panel_power_ctrl_action_execute,
};

int panel_power_ctrl_helper_execute(struct panel_power_ctrl *pctrl)
{
	if (!pctrl)
		return -EINVAL;

	return call_panel_power_ctrl_func(pctrl, execute);
}

static int of_get_action_gpio(struct panel_device *panel,
	struct device_node *action_np, struct panel_power_ctrl_action *paction)
{
	struct device_node *reg_np;
	int ret = 0;

	if (!panel || !action_np || !paction)
		return -EINVAL;

	reg_np = of_parse_phandle(action_np, "gpio", 0);
	if (!reg_np)
		return -ENODEV;

	paction->gpio = panel_gpio_list_find_by_name(panel, reg_np->name);
	if (!paction->gpio) {
		ret = -ENODATA;
		goto exit;
	}

	panel_dbg("found %s in gpio_list\n", paction->gpio->name);
exit:
	of_node_put(reg_np);
	return ret;
}

static int of_get_action_reg(struct panel_device *panel,
	struct device_node *action_np, struct panel_power_ctrl_action *paction)
{
	struct device_node *reg_np;
	int ret = 0;

	if (!panel || !action_np || !paction)
		return -EINVAL;

	reg_np = of_parse_phandle(action_np, "reg", 0);
	if (!reg_np)
		return -ENODEV;

	paction->reg = find_panel_regulator_by_node_name(panel, reg_np->name);
	if (!paction->reg) {
		ret = -ENODATA;
		goto exit;
	}

	panel_dbg("found %s %s in regulator_list\n", paction->reg->name, paction->reg->node_name);
exit:
	of_node_put(reg_np);
	return ret;
}

static int of_get_action_value(struct panel_device *panel,
	struct device_node *action_np, struct panel_power_ctrl_action *paction)
{
	int ret;

	if (!panel || !action_np || !paction)
		return -EINVAL;

	ret = of_property_read_u32(action_np, "value", &paction->value);
	if (ret < 0)
		return ret;

	panel_dbg("find value in node %s: %u\n", action_np->name, paction->value);
	return ret;
}

int of_get_panel_power_ctrl(struct panel_device *panel, struct device_node *seq_np,
	const char *prop_name, struct panel_power_ctrl *pctrl)
{
	struct device_node *action_np;
	struct panel_power_ctrl_action *action;
	int ret, sz, i;

	if (!panel || !seq_np || !pctrl)
		return -EINVAL;

	INIT_LIST_HEAD(&pctrl->action_list);
	pctrl->funcs = &panel_power_ctrl_funcs;

	sz = of_property_count_u32_elems(seq_np, prop_name);
	if (sz <= 0) {
		panel_warn("sequence '%s' is empty\n", prop_name);
		return 0;
	}

	for (i = 0; i < sz; i++) {
		action_np = of_parse_phandle(seq_np, prop_name, i);
		if (!action_np) {
			panel_warn("failed to get phandle of %s[%d]\n", prop_name, i);
			return -EINVAL;
		}

		action = kzalloc(sizeof(struct panel_power_ctrl_action), GFP_KERNEL);
		if (!action)
			return -ENOMEM;

		action->name = action_np->name;
		if (of_property_read_u32(action_np, "type", &action->type)) {
			panel_err("%s:property('type') not found\n", action_np->name);
			return -EINVAL;
		}

		if (action->type >= MAX_PANEL_POWER_CTRL_ACTION) {
			panel_err("%s: invalid action type %d\n", action->name, action->type);
			return -EINVAL;
		}
		panel_dbg("%s %s %d\n", prop_name, action->name, action->type);

		if (REQUIRE_PARSE_REG(action_table[action->type])) {
			ret = of_get_action_reg(panel, action_np, action);
			if (ret < 0) {
				panel_err("error occurred during of_get_action_reg %s %d\n", action->name, ret);
				kfree(action);
				continue;
			}
		}
		if (REQUIRE_PARSE_GPIO(action_table[action->type])) {
			ret = of_get_action_gpio(panel, action_np, action);
			if (ret < 0) {
				panel_err("error occurred during of_get_action_gpio %s %d\n", action->name, ret);
				kfree(action);
				continue;
			}
		}
		if (REQUIRE_PARSE_VALUE(action_table[action->type])) {
			ret = of_get_action_value(panel, action_np, action);
			if (ret < 0) {
				panel_err("error occurred during of_get_action_value %s %d\n", action->name, ret);
				kfree(action);
				continue;
			}
		}

		list_add_tail(&action->head, &pctrl->action_list);
		panel_info("%s type %d reg %pK gpio %pK value %d\n",
			action->name, action->type, action->reg, action->gpio, action->value);
		of_node_put(action_np);
	}

	return 0;
}
EXPORT_SYMBOL(of_get_panel_power_ctrl);

__visible_for_testing struct panel_power_ctrl *panel_power_ctrl_find(struct panel_device *panel,
	const char *dev_name, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !dev_name || !name)
		return ERR_PTR(-EINVAL);

	panel_dbg("find: %s, %s\n", dev_name, name);
	list_for_each_entry(pctrl, &panel->power_ctrl_list, head) {
		if (!pctrl->dev_name) {
			panel_err("invalid 'dev_name', skip to check\n");
			continue;
		}
		if (!pctrl->name) {
			panel_err("invalid 'name', skip to check\n");
			continue;
		}
		if (!strcmp(pctrl->dev_name, dev_name) && !strcmp(pctrl->name, name))
			return pctrl;
	}
	return ERR_PTR(-ENODATA);
}

bool panel_power_ctrl_exists(struct panel_device *panel,
	const char *dev_name, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !dev_name || !name) {
		panel_err("invalid arg\n");
		return false;
	}

	pctrl = panel_power_ctrl_find(panel, dev_name, name);
	if (IS_ERR_OR_NULL(pctrl)) {
		if (PTR_ERR(pctrl) == -ENODATA)
			panel_dbg("not found %s\n", name);
		else
			panel_err("error occurred when find %s, %ld\n", name, PTR_ERR(pctrl));
		return false;
	}
	return true;
}

int panel_power_ctrl_execute(struct panel_device *panel,
	const char *dev_name, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !dev_name || !name) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	pctrl = panel_power_ctrl_find(panel, dev_name, name);
	if (IS_ERR_OR_NULL(pctrl)) {
		if (PTR_ERR(pctrl) == -ENODATA) {
			panel_dbg("%s not found\n", name);
			return -ENODATA;
		}
		panel_err("error occurred when find %s, %ld\n", name, PTR_ERR(pctrl));
		return PTR_ERR(pctrl);
	}
	return panel_power_ctrl_helper_execute(pctrl);
}

struct pwrctrl *create_pwrctrl(char *name, char *key)
{
	struct pwrctrl *pwrctrl;
	char *dup_key;

	if (!name) {
		panel_err("name is null\n");
		return NULL;
	}

	if (!key) {
		panel_err("key is null\n");
		return NULL;
	}

	pwrctrl = kzalloc(sizeof(*pwrctrl), GFP_KERNEL);
	if (!pwrctrl)
		return NULL;

	pnobj_init(&pwrctrl->base, CMD_TYPE_PCTRL, name);
	dup_key = kstrndup(key, PNOBJ_NAME_LEN-1, GFP_KERNEL);
	if (!dup_key) {
		pnobj_deinit(&pwrctrl->base);
		kfree(pwrctrl);
		return NULL;
	}
	pwrctrl->key = dup_key;

	return pwrctrl;
}

struct pwrctrl *duplicate_pwrctrl(struct pwrctrl *pwrctrl)
{
	if (!pwrctrl)
		return NULL;

	return create_pwrctrl(get_pwrctrl_name(pwrctrl),
			get_pwrctrl_key(pwrctrl));
}

void destroy_pwrctrl(struct pwrctrl *pwrctrl)
{
	if (!pwrctrl)
		return;

	pnobj_deinit(&pwrctrl->base);
	kfree(get_pwrctrl_key(pwrctrl));
	kfree(pwrctrl);
}
