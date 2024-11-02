#include "panel_drv.h"
#include "panel_property.h"
#include "panel_config.h"
#include "panel_debug.h"

int panel_apply_pnobj_config(struct panel_device *panel,
		struct pnobj_config *pnobj_config)
{
	int ret;

	if (!panel || !pnobj_config)
		return -EINVAL;

	ret = panel_set_property_value(panel,
			pnobj_config->prop_name, pnobj_config->value);
	if (ret < 0) {
		panel_err("failed to set property(%s) value\n",
				pnobj_config->prop_name);
		return ret;
	}

	panel_info("prop_name:%s prop_value:%u\n",
			pnobj_config->prop_name, pnobj_config->value);

	return 0;
}

struct pnobj_config *create_pnobj_config(char *name,
		char *prop_name, unsigned int value)
{
	struct pnobj_config *cfg;

	if (!name)
		return NULL;

	if (!prop_name)
		return NULL;

	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return NULL;

	pnobj_init(&cfg->base, CMD_TYPE_CFG, name);
	strncpy(cfg->prop_name, prop_name, PANEL_PROP_NAME_LEN);
	cfg->prop_name[PANEL_PROP_NAME_LEN-1] = '\0';
	cfg->value = value;

	return cfg;
}

struct pnobj_config *duplicate_pnobj_config(struct pnobj_config *src)
{
	return create_pnobj_config(get_pnobj_config_name(src),
			src->prop_name, src->value);
}

void destroy_pnobj_config(struct pnobj_config *cfg)
{
	pnobj_deinit(&cfg->base);
	kfree(cfg);
}
