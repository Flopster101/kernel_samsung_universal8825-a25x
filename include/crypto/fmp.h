/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _EXYNOS_FMP_H_
#define _EXYNOS_FMP_H_

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/bio.h>
#include <crypto/fmp_fips.h>

#define FMP_DRV_VERSION "4.0.1"

#define FMP_KEY_SIZE_16		16
#define FMP_KEY_SIZE_32		32
#define FMP_IV_SIZE_16		16

#define FMP_CBC_MAX_KEY_SIZE	FMP_KEY_SIZE_16
#define FMP_XTS_MAX_KEY_SIZE	((FMP_KEY_SIZE_32) * (2))
#define FMP_MAX_KEY_SIZE	FMP_XTS_MAX_KEY_SIZE

#define FMP_HOST_TYPE_NAME_LEN	8
#define FMP_BLOCK_TYPE_NAME_LEN	8

#define FMP_SECTOR_SIZE	0x1000
#define FMP_MIN_SECTOR_SIZE	0x200
#define NUM_SECTOR_UNIT	((FMP_SECTOR_SIZE)/(FMP_MIN_SECTOR_SIZE))

#define MAX_AES_XTS_TRANSFER_SIZE	0x1000

#define CMDQ_DISABLED		0

#define MAX_RETRY_COUNT		0x100

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum fmp_crypto_fips_algo_mode {
	EXYNOS_FMP_BYPASS_MODE = 0,
	EXYNOS_FMP_ALGO_MODE_AES_CBC = 1,
	EXYNOS_FMP_ALGO_MODE_AES_XTS = 2,
};

enum fmp_crypto_key_size {
	EXYNOS_FMP_KEY_SIZE_16 = 16,
	EXYNOS_FMP_KEY_SIZE_32 = 32,
};

enum fmp_crypto_enc_mode {
	EXYNOS_FMP_FILE_ENC = 0,
	EXYNOS_FMP_ENC_MAX
};

struct fmp_crypto_info {
	/* This field's stongly aligned 'crypto_diskcipher->algo' */
	u8 key[FMP_MAX_KEY_SIZE];
	u32 key_size;
	enum fmp_crypto_key_size fmp_key_size;
	enum fmp_crypto_enc_mode enc_mode;
	enum fmp_crypto_fips_algo_mode algo_mode;
	void *ctx;
};

struct fmp_data_setting {
	struct fmp_crypto_info crypt[EXYNOS_FMP_ENC_MAX];
	struct fmp_table_setting *table;
	bool cmdq_enabled;
};

struct fips_result {
	bool overall;
	bool aes_xts;
	bool sha256;
	bool hmac;
	bool integrity;
};

#define EXYNOS_FMP_ALGO_MODE_MASK (0x3)
#define EXYNOS_FMP_ALGO_MODE_TEST_OFFSET (0xf)
#define EXYNOS_FMP_ALGO_MODE_TEST (1 << EXYNOS_FMP_ALGO_MODE_TEST_OFFSET)

struct fmp_test_data {
	char block_type[FMP_BLOCK_TYPE_NAME_LEN];
	struct block_device *bdev;
	sector_t sector;
	dev_t devt;
	uint32_t test_block_offset;
	/* iv to submitted */
	u8 iv[FMP_IV_SIZE_16];
	/* diskcipher for test */
	struct fmp_crypto_info ci;
	u64 DataUnitSeqNumber;
};

struct exynos_fmp {
	struct device *dev;
	struct fmp_test_data *test_data;
	atomic_t fips_start;
	struct fips_result result;
	struct miscdevice miscdev;
	void *test_vops;
	int fips_run;
	u8 dun_swap;
	int fips_fin;
	struct buffer_head *bh;
};

struct fmp_sg_entry {
	/* The first four fields correspond to those of ufshcd_sg_entry. */
	__le32 des0;
	__le32 des1;
	__le32 des2;
	/*
	 * The algorithm and key length are configured in the high bits of des3,
	 * whose low bits already contain ufshcd_sg_entry::size.
	 */
	__le32 des3;

	/* The IV with all bytes reversed */
	__be32 file_iv[4];

	/*
	 * The key with all bytes reversed.  For XTS, the two halves of the key
	 * are given separately and are byte-reversed separately.
	 */
	__be32 file_enckey[8];
	__be32 file_twkey[8];

	/* Not used */
	__be32 disk_iv[4];
	__le32 reserved[4];
};

#define ACCESS_CONTROL_ABORT	0x14

#ifndef SMC_CMD_FMP_SECURITY
/* For FMP/SMU Ctrl */
#define SMC_CMD_FMP_SECURITY		(0xC2001810)
#define SMC_CMD_SMU			(0xC2001850)
#define SMC_CMD_FMP_SMU_RESUME		(0xC2001860)
#define SMC_CMD_FMP_SMU_DUMP		(0xC2001870)
#define SMC_CMD_UFS_LOG			(0xC2001880)
#endif

/* Keyslot Selftest */
#define FMP_KW_SECUREKEY		0x8000
#define FMP_KW_SECUREKEY_OFFSET		0x80

#define EXYNOS_FMP_FIPS_KEYSLOT 	0xF

#define AES_256_XTS_KEY_SIZE		64
#define AES_256_XTS_TWK_OFFSET		8

#define FMP_KW_KEYVALID			0xA00C

#define FMP_EMBEDDED			0

#define DEFAULT_KWMODE			(1 | (1 << 4))

struct exynos_fmp *get_fmp(void);
int get_fmp_fips_state(void);
void exynos_fmp_fips_test(struct exynos_fmp *fmp);
#ifndef CONFIG_KEYS_IN_PRDT
unsigned long exynos_fmp_set_kw_mode(uint64_t kw_mode);
unsigned long exynos_fmp_get_kw_mode(dma_addr_t kw_mode_addr);
#endif /* CONFIG_KEYS_IN_PRDT */
#endif /* _EXYNOS_FMP_H_ */
