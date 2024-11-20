#ifndef __PANEL_POWER_CTRL_H__
#define __PANEL_POWER_CTRL_H__
#include <linux/of.h>
#include <dt-bindings/display/panel-display.h>
#include "panel_drv.h"
#include "panel_gpio.h"
#include "panel_regulator.h"

#define call_panel_power_ctrl_func(q, _f, args...)              \
	(((q) && (q)->funcs && (q)->funcs->_f) ? ((q)->funcs->_f(q, ##args)) : 0)

#define PARSE_REG (1 << 0)
#define PARSE_GPIO (1 << 1)
#define PARSE_VALUE (1 << 2)

#define REQUIRE_PARSE_REG(x) ((x) & PARSE_REG)
#define REQUIRE_PARSE_GPIO(x) ((x) & PARSE_GPIO)
#define REQUIRE_PARSE_VALUE(x) ((x) & PARSE_VALUE)

enum panel_power_ctrl_action_type {
	PANEL_POWER_CTRL_ACTION_NONE = PCTRL_NONE,
	PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP = PCTRL_DELAY_MSLEEP,
	PANEL_POWER_CTRL_ACTION_DELAY_USLEEP = PCTRL_DELAY_USLEEP,
	PANEL_POWER_CTRL_ACTION_REGULATOR_ENABLE = PCTRL_REGULATOR_ENABLE,
	PANEL_POWER_CTRL_ACTION_REGULATOR_DISABLE = PCTRL_REGULATOR_DISABLE,
	PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE = PCTRL_REGULATOR_SET_VOLTAGE,
	PANEL_POWER_CTRL_ACTION_REGULATOR_SSD_CURRENT = PCTRL_REGULATOR_SSD_CURRENT,
	PANEL_POWER_CTRL_ACTION_GPIO_ENABLE = PCTRL_GPIO_ENABLE,
	PANEL_POWER_CTRL_ACTION_GPIO_DISABLE = PCTRL_GPIO_DISABLE,
	MAX_PANEL_POWER_CTRL_ACTION,
};

struct panel_power_ctrl_action {
	enum panel_power_ctrl_action_type type;
	const char *name;
	struct list_head head;
	struct panel_regulator *reg;
	struct panel_gpio *gpio;
	u32 value;
};

struct panel_power_ctrl {
	const char *dev_name;
	const char *name;
	struct list_head head;
	struct list_head action_list;
	struct panel_power_ctrl_funcs *funcs;
};

struct panel_power_ctrl_funcs {
	int (*execute)(struct panel_power_ctrl *pctrl);
};

int of_get_panel_power_ctrl(struct panel_device *panel, struct device_node *seq_np,
	const char *prop_name, struct panel_power_ctrl *pctrl);
int panel_power_ctrl_helper_execute(struct panel_power_ctrl *pctrl);
#endif
