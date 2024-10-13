/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Samsung Electronics.
 *
 */

#ifndef __CPIF_MEMLOGGER_H__
#define __CPIF_MEMLOGGER_H__

#if IS_ENABLED(CONFIG_CPIF_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#if IS_ENABLED(CONFIG_CPIF_MEMORY_LOGGER)
unsigned int cpif_memlog_log_enabled(void);
struct memlog_obj *cpif_memlog_log_obj(void);
void cpif_memlog_init(struct platform_device *pdev);
int pr_buffer_memlog(const char *tag, const char *data, size_t data_len, size_t max_len);

#define pr_memlog_level(level, fmt, ...)				\
({									\
	if (cpif_memlog_log_enabled()) {				\
		memlog_write_printf(cpif_memlog_log_obj(), level,	\
			fmt, ##__VA_ARGS__);				\
	}								\
})
#define pr_memlog_level_ratelimited(level, fmt, ...)			\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
									\
	if (__ratelimit(&_rs)) {					\
		pr_memlog_level(level, fmt, ##__VA_ARGS__);		\
	}								\
})
#else
#define pr_memlog_level(level, fmt, args...)
#define pr_memlog_level_ratelimited(level, fmt, ...)
#endif

#endif /* __CPIF_MEMLOGGER_H__ */
