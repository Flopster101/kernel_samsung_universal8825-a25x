#ifndef __CMU_EWF_H__
#define __CMU_EWF_H__

#define EARLY_WAKEUP_FORCED_ENABLE0		(0x0a00)
#define EARLY_WAKEUP_FORCED_ENABLE1		(0x0a04)
#define EWF_MAX_INDEX				(64)

#if defined(CONFIG_SOC_S5E9925) && defined(CONFIG_SOC_S5E9925_EVT0)
#define EWF_GRP_CAM             (51)
#define EWF_CAM_BLK0             (0x6114)    	//NOCL1A, NOCL2A, NOCL0, CSIS, CSTAT
#define EWF_CAM_BLK1             (0xf)		//MIF0/1/2/3
#endif
#if defined(CONFIG_SOC_S5E9925) && !defined(CONFIG_SOC_S5E9925_EVT0)
#define EWF_GRP_CAM             (57)
#define EWF_CAM_BLK0             (0x6110)	//NOCL1C, NOCL0, CSIS, CSTAT
#define EWF_CAM_BLK1             (0x200000f)	//MIF0/1/2/3, ALLCSIS
#endif

#if defined(CONFIG_CMU_EWF) || defined(CONFIG_CMU_EWF_MODULE)
extern int get_cmuewf_index(struct device_node *np, unsigned int *index);
extern int set_cmuewf(unsigned int index, unsigned int en);
extern int early_wakeup_forced_enable_init(void);

#else
static inline int get_cmuewf_index(struct device_node *np, unsigned int *index)
{
	return 0;
}

static inline int set_cmuewf(unsigned int index, unsigned int en)
{
	return 0;
}
static int early_wakeup_forced_enable_init(void)
{
	return 0;
}
#endif
#endif
