/* individual sequence descriptor for CHUB control - reset, config, release */
struct pmucal_seq chub_reset_release_config[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION",	0x11860000, 0x2200, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS",		0x11860000, 0x2204, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",		0x11860000, 0x220c, (0x1 << 2), (0x0 << 2),0,0,0,0),
//	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OUT",		0x11860000, 0x2220, (0x1 << 11), (0x1 << 11),0,0,0,0),
};

struct pmucal_seq chub_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",	 	0x11860000, 0x220c, (0x1 << 2), (0x1 << 2),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION", 	0x11860000, 0x2200, (0x1 << 0), (0x0 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS", 		0x11860000, 0x2204, (0x1 << 0), (0x0 << 0),0,0,0,0),
};

struct pmucal_seq chub_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CPU_CONFIGURATION",	0x11860000, 0x3100, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_CPU_STATUS",		0x11860000, 0x3104, (0x1 << 0), (0x1 << 0),0,0,0,0),
};

struct pmucal_chub pmucal_chub_list = {
//	.on = chub_on,
	.reset_assert = chub_reset_assert,
	.reset_release_config = chub_reset_release_config,
	.reset_release = chub_reset_release,
//	.num_on = ARRAY_SIZE(chub_on),
	.num_reset_assert = ARRAY_SIZE(chub_reset_assert),
	.num_reset_release_config = ARRAY_SIZE(chub_reset_release_config),
	.num_reset_release = ARRAY_SIZE(chub_reset_release),
};

unsigned int pmucal_chub_list_size = 1;
