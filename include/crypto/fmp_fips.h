/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _EXYNOS_FMP_FIPS_H_
#define _EXYNOS_FMP_FIPS_H_

#include <linux/bio.h>

#define EXYNOS_FMP_CCI_MASK		(0x000000FF)
#define EXYNOS_FMP_CE_BIT_MASK		(0x00800000)

struct fmp_table_setting {
	__le32 des0;            /* des0 */
#define GET_CMDQ_LENGTH(d) \
	(((d)->des0 & 0xffff0000) >> 16)
	__le32 des1;            /* des1 */
	__le32 des2;            /* des2 */
	__le32 des3;            /* des3 */
/* CMDQ Operation */
#define FKL_CMDQ BIT(0)
#define DKL_CMDQ BIT(1)
#define SET_CMDQ_KEYLEN(d, v) ((d)->des3 |= (uint32_t)v)
#define SET_CMDQ_FAS(d, v) \
	((d)->des3 = ((d)->des3 & 0xfffffff3) | v << 2)
#define SET_CMDQ_DAS(d, v) \
	((d)->des3 = ((d)->des3 & 0xffffffcf) | v << 4)
#define GET_CMDQ_FAS(d) ((d)->des3 & 0x0000000c)
#define GET_CMDQ_DAS(d) ((d)->des3 & 0x00000030)
	__le32 file_iv0;        /* des4 */
	__le32 file_iv1;        /* des5 */
	__le32 file_iv2;        /* des6 */
	__le32 file_iv3;        /* des7 */
	__le32 file_enckey0;    /* des8 */
	__le32 file_enckey1;    /* des9 */
	__le32 file_enckey2;    /* des10 */
	__le32 file_enckey3;    /* des11 */
	__le32 file_enckey4;    /* des12 */
	__le32 file_enckey5;    /* des13 */
	__le32 file_enckey6;    /* des14 */
	__le32 file_enckey7;    /* des15 */
	__le32 file_twkey0;     /* des16 */
	__le32 file_twkey1;     /* des17 */
	__le32 file_twkey2;     /* des18 */
	__le32 file_twkey3;     /* des19 */
	__le32 file_twkey4;     /* des20 */
	__le32 file_twkey5;     /* des21 */
	__le32 file_twkey6;     /* des22 */
	__le32 file_twkey7;     /* des23 */
	__le32 disk_iv0;        /* des24 */
	__le32 disk_iv1;        /* des25 */
	__le32 disk_iv2;        /* des26 */
	__le32 disk_iv3;        /* des27 */
	__le32 reserved0;       /* des28 */
	__le32 reserved1;       /* des29 */
	__le32 reserved2;       /* des30 */
	__le32 reserved3;       /* des31 */
};

#ifdef CONFIG_KEYS_IN_PRDT
struct exynos_fmp_crypt_info {
	bool fips;
	u64 dun_hi;
	u64 dun_lo;
	const u8 *enckey;
	const u8 *twkey;
};
#else
struct exynos_fmp_crypt_info {
	bool fips;
	u64 data_unit_num;
	unsigned int crypto_key_slot;

};

struct exynos_fmp_key_info {
	const u8 *raw;
	unsigned int size;
	unsigned int slot;
};

struct fmp_handle {
	void *std;
	void *hci;
	void *ufsp;
	void *unipro;
	void *pma;
	void *cport;
	void (*udelay)(u32 us);
	void *private;
};
#endif

bool is_fmp_fips_op(struct bio *bio);
bool is_fmp_fips_clean(struct bio *bio);
#ifdef CONFIG_EXYNOS_FIPS_SIMULATOR
struct fmp_handle *get_fmp_handle(void);
void set_fips_keyslot_num(int num);
#endif
#ifdef CONFIG_KEYS_IN_PRDT
int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_table_setting *table);
int exynos_fmp_clear(struct fmp_table_setting *table);
#else
int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_handle *handle);
int exynos_fmp_setkey(struct exynos_fmp_key_info *fmp_ki, struct fmp_handle *handle);
int exynos_fmp_clear(struct fmp_handle *handle, int slot);
#endif /* CONFIG_KEYS_IN_PRDT */
#endif /* _EXYNOS_FMP_FIPS_H_ */
