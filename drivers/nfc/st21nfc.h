/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * NFC Controller Driver
 * Copyright (C) 2020 ST Microelectronics S.A.
 * Copyright (C) 2010 Stollmann E+V GmbH
 * Copyright (C) 2010 Trusted Logic S.A.
 */

#ifdef CONFIG_SEC_NFC_LOGGER
#include "nfc_logger/nfc_logger.h"
#endif

#define ST21NFC_MAGIC 0xEA

#define ST21NFC_NAME "st21nfc"
/*
 * ST21NFC power control via ioctl
 * ST21NFC_GET_WAKEUP :  poll gpio-level for Wakeup pin
 */
#define ST21NFC_GET_WAKEUP _IO(ST21NFC_MAGIC, 0x01)
#define ST21NFC_PULSE_RESET _IO(ST21NFC_MAGIC, 0x02)
#define ST21NFC_SET_POLARITY_RISING _IO(ST21NFC_MAGIC, 0x03)
#define ST21NFC_SET_POLARITY_HIGH _IO(ST21NFC_MAGIC, 0x05)
#define ST21NFC_GET_POLARITY _IO(ST21NFC_MAGIC, 0x07)
#define ST21NFC_RECOVERY _IO(ST21NFC_MAGIC, 0x08)
#define ST21NFC_USE_ESE _IOW(ST21NFC_MAGIC, 0x09, unsigned int)

// Keep compatibility with older user applications.
#define ST21NFC_LEGACY_GET_WAKEUP _IOR(ST21NFC_MAGIC, 0x01, unsigned int)
#define ST21NFC_LEGACY_PULSE_RESET _IOR(ST21NFC_MAGIC, 0x02, unsigned int)
#define ST21NFC_LEGACY_SET_POLARITY_RISING                                     \
	_IOR(ST21NFC_MAGIC, 0x03, unsigned int)
#define ST21NFC_LEGACY_SET_POLARITY_HIGH _IOR(ST21NFC_MAGIC, 0x05, unsigned int)
#define ST21NFC_LEGACY_GET_POLARITY _IOR(ST21NFC_MAGIC, 0x07, unsigned int)
#define ST21NFC_LEGACY_RECOVERY _IOR(ST21NFC_MAGIC, 0x08, unsigned int)

#define ST54SPI_CB_RESET_END 0
#define ST54SPI_CB_RESET_START 1
#define ST54SPI_CB_ESE_NOT_USED 2
#define ST54SPI_CB_ESE_USED 3
void st21nfc_register_st54spi_cb(void (*cb)(int, void *), void *data);
void st21nfc_unregister_st54spi_cb(void);

#if KERNEL_VERSION(5, 0, 0) > LINUX_VERSION_CODE
#define ACCESS_OK(x, y, z) access_ok(x, y, z)
#else
#define ACCESS_OK(x, y, z) access_ok(y, z)
#endif

#ifndef CONFIG_SEC_NFC_LOGGER
#define NFC_LOG_ERR(fmt, ...)		pr_err("sec_nfc: "fmt, ##__VA_ARGS__)
#define NFC_LOG_INFO(fmt, ...)		pr_info("sec_nfc: "fmt, ##__VA_ARGS__)
#define NFC_LOG_INFO_WITH_DATE(fmt, ...) pr_info("sec_nfc: "fmt, ##__VA_ARGS__)
#define NFC_LOG_DBG(fmt, ...)		pr_debug("sec_nfc: "fmt, ##__VA_ARGS__)
#define NFC_LOG_REC(fmt, ...)		do { } while (0)

#define nfc_print_hex_dump(a, b, c)	do { } while (0)
#define nfc_logger_init()		do { } while (0)
#define nfc_logger_deinit()		do { } while (0)
#define nfc_logger_set_max_count(a)	do { } while (0)
#define nfc_logger_register_nfc_stauts_func(a)	do { } while (0)
#endif /* CONFIG_SEC_NFC_LOGGER */
