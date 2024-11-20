#ifndef MUIC_SYSFS_H
#define MUIC_SYSFS_H

#if IS_ENABLED(CONFIG_MUIC_SYSFS)
extern struct device *muic_device_create(void *drvdata, const char *fmt);
extern void muic_device_destroy(dev_t devt);
extern int muic_sysfs_init(struct muic_platform_data *muic_pdata);
extern void muic_sysfs_deinit(struct muic_platform_data *muic_pdata);
#else
static inline struct device *muic_device_create(void *drvdata, const char *fmt)
{
	pr_err("No rule to make muic sysfs\n");
	return NULL;
}
static inline void muic_device_destroy(dev_t devt)
{
	return;
}
static inline int muic_sysfs_init(struct muic_platform_data *muic_pdata)
		{return 0;}
static inline void muic_sysfs_deinit(struct muic_platform_data *muic_pdata) {}
#endif

#endif /* MUIC_SYSFS_H */
