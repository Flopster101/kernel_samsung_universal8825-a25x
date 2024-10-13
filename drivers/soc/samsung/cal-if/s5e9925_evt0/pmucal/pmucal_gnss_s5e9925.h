/* individual sequence descriptor for gnss control - init, reset, release, gnss_active_clear, gnss_reset_req_clear */
struct pmucal_seq gnss_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_S", 0x15860000, 0x3994, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x15860000, 0x3980, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x15860000, 0x3984, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_IN", 0x15860000, 0x39a4, (0x1 << 4), (0x1 << 4), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 8), (0x0 << 8), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 7), (0x1 << 7), 0, 0, 0xffffffff, 0),
};

struct pmucal_seq gnss_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "GNSS_STATUS", 0x15860000, 0x3984, (0x1 << 0), 0, 0, 0, 0xffffffff, 0),
};

struct pmucal_seq gnss_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 7), (0x0 << 7), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x15860000, 0x3980, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x15860000, 0x3984, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};

struct pmucal_seq gnss_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x15860000, 0x3980, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x15860000, 0x3984, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_IN", 0x15860000, 0x39a4, (0x1 << 4), (0x1 << 4), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 8), (0x0 << 8), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 7), (0x1 << 7), 0, 0, 0xffffffff, 0),
};

struct pmucal_seq gnss_active_clear[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x15860000, 0x3990, (0x1 << 8), (0x1 << 8), 0, 0, 0xffffffff, 0),
};
struct pmucal_gnss pmucal_gnss_list = {
		.init = gnss_init,
		.status = gnss_status,
		.reset_assert = gnss_reset_assert,
		.reset_release = gnss_reset_release,
		.active_clear = gnss_active_clear,
		.num_init = ARRAY_SIZE(gnss_init),
		.num_status = ARRAY_SIZE(gnss_status),
		.num_reset_assert = ARRAY_SIZE(gnss_reset_assert),
		.num_reset_release = ARRAY_SIZE(gnss_reset_release),
		.num_active_clear = ARRAY_SIZE(gnss_active_clear),
};
unsigned int pmucal_gnss_list_size = 1;
