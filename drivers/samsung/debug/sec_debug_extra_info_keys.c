// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung debugging features for Samsung's SoC's.
 *
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

/* keys are grouped by size */
static char key32[][MAX_ITEM_KEY_LEN] = {
	"ID", "KTIME", "BIN", "RR",
	"SPCNT", "LEV", "ASB", "PSITE",
	"DDRID", "RST", "INFO2", "INFO3",
	"RSTCNT", "HLCPU", "UP", "DOWN",
	"WDGT", "SCI",
};

static char key64[][MAX_ITEM_KEY_LEN] = {
	"BAT", "FAULT", "EPD", "HLEHLD",
	"PWR", "PWROFF", "PINT", "PSTAT",
	"PWROFFS", "PINTS", "PSTATS", "FPMU",
	"UFS",
};

static char key256[][MAX_ITEM_KEY_LEN] = {
	"KLG", "BUS", "DSSBUS", "PANIC",
	"PC", "LR", "BUG", "ESR", "SMU",
	"FREQ", "ODR", "AUD", "UNFZ",
	"WDGC", "HLTYPE", "MOCP", "SOCP",
	"DCN", "EHLD", "LFRQ",
};

static char key1024[][MAX_ITEM_KEY_LEN] = {
	"CPU0", "CPU1", "CPU2", "CPU3",
	"CPU4", "CPU5", "CPU6", "CPU7",
	"CPU8", "MFC", "STACK", "REGS",
	"HLDATA", "HLFREQ", "HLCNT", "FPMUMSG",
};

/* keys are grouped by sysfs node */
static char akeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "ODR", "KTIME",
	"BIN", "DDRID", "RST", "ASB",
	"BAT", "LEV", "RSTCNT", "WDGC", "WDGT",
	"FAULT", "BUG", "BUS", "DSSBUS",
	"FPMU", "PC", "LR", "PANIC", "UFS",
	"SMU", "EPD", "ESR", "UP",
	"DOWN", "SCI", "SPCNT", "FREQ", "LFRQ",
	"STACK", "KLG", "EHLD", "UNFZ", "HLTYPE",
	"HLDATA", "HLFREQ", "HLEHLD", "HLCPU",
};

static char bkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "PSITE", "MOCP",
	"SOCP", "INFO2", "INFO3", "PWR",
	"PWROFF", "PINT", "PSTAT", "PWROFFS",
	"PINTS", "PSTATS",
};

static char ckeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "CPU0", "CPU1",
	"CPU2", "CPU3", "CPU4", "CPU5",
	"CPU6", "CPU7", "CPU8", "HLCNT",
};

static char fkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "FPMUMSG",
};

static char mkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "MFC", "AUD",
	"DCN",
};

static char tkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "REGS"
};
