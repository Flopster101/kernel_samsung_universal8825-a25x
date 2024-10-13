/*
 * Exynos FMP driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Authors: Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/crypto.h>
#include <linux/dma-mapping.h>
#include <soc/samsung/exynos-smc.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <crypto/fmp.h>

#include "fmp_fips_main.h"
#include "fmp_test.h"
#include "fmp_fips_info.h"
#ifndef CONFIG_KEYS_IN_PRDT
#include "ufs-vs-mmio.h"
#endif
#include <linux/genhd.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#define WORD_SIZE 4
#define FMP_IV_MAX_IDX (FMP_IV_SIZE_16 / WORD_SIZE)

#define byte2word(b0, b1, b2, b3)       \
			(((unsigned int)(b0) << 24) | \
			((unsigned int)(b1) << 16) | \
			((unsigned int)(b2) << 8) | (b3))
#define get_word(x, c)  byte2word(((unsigned char *)(x) + 4 * (c))[0], \
				((unsigned char *)(x) + 4 * (c))[1], \
				((unsigned char *)(x) + 4 * (c))[2], \
				((unsigned char *)(x) + 4 * (c))[3])
/* Legacy Operation */
#define FKL BIT(26)
#define DKL BIT(27)
#define SET_KEYLEN(d, v) ((d)->des3 |= (uint32_t)v)
#define SET_FAS(d, v) \
        ((d)->des3 = ((d)->des3 & 0xcfffffff) | v << 28)
#define SET_DAS(d, v) \
        ((d)->des3 = ((d)->des3 & 0x3fffffff) | v << 30)
#define GET_FAS(d)      ((d)->des3 & 0x30000000)
#define GET_DAS(d)      ((d)->des3 & 0xc0000000)
#define GET_LENGTH(d) \
        ((d)->des3 & 0x3ffffff)

static struct device *fmp_dev;

struct exynos_fmp *get_fmp(void)
{
	return dev_get_drvdata(fmp_dev);
}
EXPORT_SYMBOL(get_fmp);

static inline void dump_ci(struct fmp_crypto_info *c)
{
	if (c) {
		pr_info
		    ("%s: algo:%d enc:%d key_size:%d\n",
		     __func__, c->algo_mode, c->enc_mode,c->key_size);
		if (c->enc_mode == EXYNOS_FMP_FILE_ENC)
			print_hex_dump(KERN_CONT, "key:",
				       DUMP_PREFIX_OFFSET, 16, 1, c->key,
				       sizeof(c->key), false);
	}
}

static inline void dump_table(struct fmp_table_setting *table)
{
	print_hex_dump(KERN_CONT, "dump:", DUMP_PREFIX_OFFSET, 16, 1,
		       table, sizeof(struct fmp_table_setting), false);
}

static inline int is_supported_ivsize(u32 ivlen)
{
	if (ivlen && (ivlen <= FMP_IV_SIZE_16))
		return TRUE;
	else
		return FALSE;
}

static inline int check_aes_xts_size(struct fmp_table_setting *table,
				     bool cmdq_enabled)
{
	int size;

	if (cmdq_enabled)
		size = GET_CMDQ_LENGTH(table);
	else
		size = GET_LENGTH(table);
	return (size > MAX_AES_XTS_TRANSFER_SIZE) ? size : 0;
}

/* check for fips that no allow same keys */
static inline int check_aes_xts_key(char *key,
				    enum fmp_crypto_key_size key_size)
{
	char *enckey, *twkey;

	enckey = key;
	twkey = key + key_size;
	return (memcmp(enckey, twkey, key_size)) ? 0 : -1;
}

int fmplib_set_algo_mode(struct fmp_table_setting *table,
			 struct fmp_crypto_info *crypto, bool cmdq_enabled)
{
	int ret;
	enum fmp_crypto_fips_algo_mode algo_mode = crypto->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK;

	if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS) {
		ret = check_aes_xts_size(table, cmdq_enabled);
		if (ret) {
			pr_err("%s: Fail FMP XTS due to invalid size(%d), cmdq:%d\n",
			       __func__, ret, cmdq_enabled);
			return -EINVAL;
		}
	}

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		if (cmdq_enabled)
			SET_CMDQ_FAS(table, algo_mode);
		else
			SET_FAS(table, algo_mode);
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}
#ifdef CONFIG_KEYS_IN_PRDT
static inline __be32 fmp_key_word(const u8 *key, int j)
{
	return cpu_to_be32(get_unaligned_le32(
			key + AES_KEYSIZE_256 - (j + 1) * sizeof(__le32)));
}

static int fmplib_set_file_key(struct fmp_table_setting *table,
			struct fmp_crypto_info *crypto)
{
	enum fmp_crypto_fips_algo_mode algo_mode = crypto->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK;
	enum fmp_crypto_key_size key_size = crypto->fmp_key_size;
	char *key = crypto->key;
	int idx, max;

	if (!key || (crypto->enc_mode != EXYNOS_FMP_FILE_ENC) ||
		((key_size != EXYNOS_FMP_KEY_SIZE_16) &&
		 (key_size != EXYNOS_FMP_KEY_SIZE_32))) {
		pr_err("%s: Invalid key_size:%d enc_mode:%d\n",
		       __func__, key_size, crypto->enc_mode);
		return -EINVAL;
	}

	if ((algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS)
	    && check_aes_xts_key(key, key_size)) {
		pr_err("%s: Fail FMP XTS due to the same enc and twkey\n",
		       __func__);
		return -EINVAL;
	}

	if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_CBC) {
		max = key_size / WORD_SIZE;
		for (idx = 0; idx < max; idx++)
			*(&table->file_enckey0 + idx) =
			    get_word(key, max - (idx + 1));
	} else if (algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS) {
		key_size *= 2;
		max = key_size / WORD_SIZE;
		for (idx = 0; idx < (max / 2); idx++)
			*(&table->file_enckey0 + idx) =
			    get_word(key, (max / 2) - (idx + 1));
		for (idx = 0; idx < (max / 2); idx++)
			*(&table->file_twkey0 + idx) =
			    get_word(key, max - (idx + 1));
	}
	return 0;
}
#else
static int fmplib_set_file_key(struct fmp_handle *handle, struct fmp_crypto_info *ci)
{
	int ret = 0;
	enum fmp_crypto_key_size key_size = ci->fmp_key_size;
	enum fmp_crypto_fips_algo_mode algo_mode = ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK;
	struct  exynos_fmp_key_info fmp_key_info;

	if (IS_ERR_OR_NULL(ci->key) || (ci->enc_mode != EXYNOS_FMP_FILE_ENC) ||
		((key_size != EXYNOS_FMP_KEY_SIZE_32))) {
		pr_err("%s: Invalid key_size:%d enc_mode:%d\n",
			__func__, key_size, ci->enc_mode);
		return -EINVAL;
	}

	if ((algo_mode == EXYNOS_FMP_ALGO_MODE_AES_XTS)
	    && check_aes_xts_key(ci->key, key_size)) {
		pr_err("%s: Fail FMP XTS due to the same enc and twkey\n",
			__func__);
		return -EINVAL;
	}

	fmp_key_info.raw = ci->key;
	fmp_key_info.size = ci->fmp_key_size * 2;
	fmp_key_info.slot = EXYNOS_FMP_FIPS_KEYSLOT;

	ret = exynos_fmp_setkey(&fmp_key_info, handle);
	if (ret) {
		pr_err("%s: Fail to set FMP key in keyslot (ret: %d)\n", __func__, ret);
		return ret;
	}

	return ret;
}

unsigned long exynos_fmp_set_kw_mode(uint64_t kw_mode)
{
	unsigned long ret;

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, kw_mode);
	if (ret) {
		pr_err("%s: SMC_CMD_FMP_KW_MODE failed to set kw_mode: %ld\n", __func__, ret);
		return ret;
	}

	return 0;
}

unsigned long exynos_fmp_get_kw_mode(dma_addr_t kw_mode_addr)
{
	unsigned long ret;

	ret = exynos_smc(SMC_CMD_FMP_GET_KW_MODE, 0, FMP_EMBEDDED, kw_mode_addr);
	if (ret) {
		pr_err("%s: SMC_CMD_FMP_GET_KW_MODE failed to set kw_mode: %ld\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif /* CONFIG_KEYS_IN_PRDT */

#ifdef CONFIG_KEYS_IN_PRDT
static int fmplib_set_key_size(struct fmp_table_setting *table,
			struct fmp_crypto_info *crypto, bool cmdq_enabled)
{
	enum fmp_crypto_key_size key_size;

	key_size = crypto->fmp_key_size;

	if ((key_size != EXYNOS_FMP_KEY_SIZE_16) &&
		(key_size != EXYNOS_FMP_KEY_SIZE_32)) {
		pr_err("%s: Invalid keysize %d\n", __func__, key_size);
		return -EINVAL;
	}

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		if (cmdq_enabled)
			SET_CMDQ_KEYLEN(table,
					(key_size ==
					 FMP_KEY_SIZE_32) ? FKL_CMDQ : 0);
		else
			SET_KEYLEN(table,
				   (key_size == FMP_KEY_SIZE_32) ? FKL : 0);
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}

static int fmplib_set_iv(struct fmp_table_setting *table,
		  struct fmp_crypto_info *crypto, u8 *iv)
{
	int idx;

	switch (crypto->enc_mode) {
	case EXYNOS_FMP_FILE_ENC:
		for (idx = 0; idx < FMP_IV_MAX_IDX; idx++)
			*(&table->file_iv0 + idx) =
			    get_word(iv, FMP_IV_MAX_IDX - (idx + 1));
		break;
	default:
		pr_err("%s: Invalid fmp enc mode %d\n", __func__,
		       crypto->enc_mode);
		return -EINVAL;
	}
	return 0;
}

int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_table_setting *table)
{
	struct exynos_fmp *fmp = get_fmp();
	struct fmp_crypto_info *ci;
	struct fmp_sg_entry *ent = (struct fmp_sg_entry *)table;
	u8 iv[FMP_IV_SIZE_16];
	u64 ret = 0;
	size_t j, limit;

	if (!fmp) {
		pr_err("%s: invalid fmp\n", __func__);
		return -EINVAL;
	}

	if (!table) {
		pr_err("%s: invalid table setting\n", __func__);
		return -EINVAL;
	}

	if (get_fmp_fips_state())
		return -EINVAL;

	if (fmp_ci->fips) {
		/* check fips test data */
		if (!fmp->test_data) {
			dev_err(fmp->dev, "%s:  no test_data for test mode\n", __func__);
			return -EINVAL;
		}

		ci = &fmp->test_data->ci;
		if (!ci || !(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_TEST)) {
			dev_err(fmp->dev, "%s: Invalid fmp crypto_info for test mode\n", __func__);
			return -EINVAL;
		}

		if (!(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK)) {
			dev_err(fmp->dev, "%s: no test_data for algo mode\n", __func__);
			return -EINVAL;
		}

		ret = fmplib_set_algo_mode(table, ci, CMDQ_DISABLED);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP encryption mode\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		/* set key into table */
		switch (ci->enc_mode) {
		case EXYNOS_FMP_FILE_ENC:
			ret = fmplib_set_file_key(table, ci);
			if (ret) {
				dev_err(fmp->dev, "%s: Fail to set FMP key\n", __func__);
				ret = -EINVAL;
				goto out;
			}
			break;
		default:
			dev_err(fmp->dev, "%s: Invalid fmp enc mode %d\n", __func__, ci->enc_mode);
			ret = -EINVAL;
			goto out;
		}

		/* set key size into table */
		ret = fmplib_set_key_size(table, ci, CMDQ_DISABLED);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP key size\n", __func__);
			goto out;
		}

		/* use test manager's iv instead of host driver's iv */
		dev_dbg(fmp->dev, "%s: fips crypt: ivsize:%d, key_size: %d, %d\n",
				__func__, sizeof(fmp->test_data->iv), ci->key_size, ci->fmp_key_size);

		if (!is_supported_ivsize(sizeof(fmp->test_data->iv))) {
			dev_err(fmp->dev, "%s: invalid (mode:%d ivsize:%d)\n",
					 __func__, ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK,
					sizeof(fmp->test_data->iv));
			ret = -EINVAL;
			goto out;
		}

		/* set iv as intended size */
		memset(iv, 0, FMP_IV_SIZE_16);
		memcpy(iv, fmp->test_data->iv, sizeof(fmp->test_data->iv));
		ret = fmplib_set_iv(table, ci, iv);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to set FMP IV\n", __func__);
			ret = -EINVAL;
			goto out;
		}
	} else {
		if (!crypto_memneq(fmp_ci->enckey, fmp_ci->twkey, AES_KEYSIZE_256)) {
			dev_err(fmp->dev, "Can't use weak AES-XTS key\n");
			ret = -EKEYREJECTED;
			goto out;
		}

		SET_FAS(ent, EXYNOS_FMP_ALGO_MODE_AES_XTS);
		SET_KEYLEN(ent, FKL);

		/* set the file/tweak key in table */
		for (j = 0, limit = AES_KEYSIZE_256 / sizeof(u32); j < limit; j++) {
			ent->file_enckey[j] = fmp_key_word(fmp_ci->enckey, j);
			ent->file_twkey[j] = fmp_key_word(fmp_ci->twkey, j);
		}

		/* Set the IV. */
		ent->file_iv[0] = cpu_to_be32(upper_32_bits(fmp_ci->dun_hi));
		ent->file_iv[1] = cpu_to_be32(lower_32_bits(fmp_ci->dun_hi));
		ent->file_iv[2] = cpu_to_be32(upper_32_bits(fmp_ci->dun_lo));
		ent->file_iv[3] = cpu_to_be32(lower_32_bits(fmp_ci->dun_lo));
	}

out:
	if (ret) {
		dump_ci(ci);
		dump_table(table);
	}
	return ret;
}
EXPORT_SYMBOL(exynos_fmp_crypt);
#else

int exynos_fmp_setkey(struct exynos_fmp_key_info *fmp_ki, struct fmp_handle *handle)
{
	int ret = 0;
	size_t i, limit;
	u32 count = 0;
	u32 kw_keyvalid;
	u32 slot_offset;
	const u8 *enckey, *twkey;
	union {
		u8 bytes[AES_256_XTS_KEY_SIZE];
		u32 words[AES_256_XTS_KEY_SIZE / sizeof(u32)];
	} fmp_key;

	/* Key length check. FMP only support 256b Key for AES-XTS */
	if (fmp_ki->size != AES_256_XTS_KEY_SIZE) {
		pr_err("%s: Does not support %d length of AES-XTS key\n", __func__, fmp_ki->size);
		return -EINVAL;
	}

	if (get_fmp_fips_state())
		return -EINVAL;

	/* In XTS mode, the blk_crypto_key's size is already doubled */
	memcpy(fmp_key.bytes, fmp_ki->raw, fmp_ki->size);

	enckey = fmp_ki->raw;
	twkey = enckey + AES_KEYSIZE_256;

	slot_offset = FMP_KW_SECUREKEY + (fmp_ki->slot * FMP_KW_SECUREKEY_OFFSET);

	/* Reject weak AES-XTS keys */
	if (!crypto_memneq(enckey, twkey, AES_KEYSIZE_256)) {
		pr_err("%s: Can't use weak AES-XTS key\n", __func__);
		return -EKEYREJECTED;
	}

	/* Key program in keyslot */
	/* Swap File key and Tweak key */
	for (i = 0, limit = fmp_ki->size / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel((struct ufs_vs_handle *)handle, le32_to_cpu(fmp_key.words[i]),
			slot_offset + ((i + AES_256_XTS_TWK_OFFSET) * sizeof(u32)));
		ufsp_writel((struct ufs_vs_handle *)handle, le32_to_cpu(fmp_key.words[i + AES_256_XTS_TWK_OFFSET]),
			slot_offset + (i * sizeof(u32)));
	}

	/* Zeroise the key */
	memzero_explicit(&fmp_key, fmp_ki->size);

	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl((struct ufs_vs_handle *)handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << (fmp_ki->slot)))) {
			pr_warn("%s: Key slot #%d is not valid yet\n", __func__, fmp_ki->slot);
			udelay(2);
			count++;
			continue;
		} else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << fmp_ki->slot))) {
		pr_err("%s: Key slot #%d is not valid\n", __func__, fmp_ki->slot);
		return -EINVAL;
	}

	pr_debug("%s: Key valid = 0x%x\n", __func__, kw_keyvalid);

	return ret;
}
EXPORT_SYMBOL(exynos_fmp_setkey);

int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_handle *handle)
{
	struct exynos_fmp *fmp = get_fmp();
	struct fmp_crypto_info *ci;
	int ret = 0;

	if (!fmp) {
		pr_err("%s: invalid fmp\n", __func__);
		return -EINVAL;
	}

	if (!fmp_ci || !handle) {
		dev_err(fmp->dev, "%s: Invalid fmp_crypt_info, fmp_handle parameter\n", __func__);
		return -EINVAL;
	}

	if (get_fmp_fips_state())
		return -EINVAL;

	if (fmp_ci->fips) {
		/* check fips test data */
		if (!fmp->test_data) {
			dev_err(fmp->dev, "%s: no test_data for test mode\n", __func__);
			return -EINVAL;
		}

		ci = &fmp->test_data->ci;
		if (!ci || !(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_TEST)) {
			dev_err(fmp->dev, "%s: Invalid fmp crypto_info for test mode\n", __func__);
			return -EINVAL;
		}

		if (!(ci->algo_mode & EXYNOS_FMP_ALGO_MODE_MASK)) {
			dev_err(fmp->dev, "%s: no test_data for algo mode\n", __func__);
			return -EINVAL;
		}

		switch (ci->enc_mode) {
		case EXYNOS_FMP_FILE_ENC:
			ret = fmplib_set_file_key(handle, ci);
			if (ret) {
				dev_err(fmp->dev, "%s: Fail to set FMP key\n", __func__);
				ret = -EINVAL;
				goto out;
			}
			break;
		default:
			dev_err(fmp->dev, "%s: Invalid fmp enc mode %d\n", __func__, ci->enc_mode);
			ret = -EINVAL;
			goto out;
		}

		if (fmp->dun_swap == 1) {
			fmp_ci->data_unit_num = cpu_to_be64(fmp->test_data->DataUnitSeqNumber);
			fmp_ci->crypto_key_slot = EXYNOS_FMP_FIPS_KEYSLOT;
		} else {
			fmp_ci->data_unit_num = cpu_to_le64(fmp->test_data->DataUnitSeqNumber);
			fmp_ci->crypto_key_slot = EXYNOS_FMP_FIPS_KEYSLOT;
		}
	}

out:
	return ret;
}
EXPORT_SYMBOL(exynos_fmp_crypt);
#endif /* CONFIG_KEYS_IN_PRDT */

#ifdef CONFIG_KEYS_IN_PRDT
static inline void fmplib_clear_file_key(struct fmp_table_setting *table)
{
	memset(&table->file_iv0, 0, sizeof(__le32) * 24);
}
#else
static void fmplib_clear_file_key(struct ufs_vs_handle *handle, int slot)
{
	size_t i, limit;
	int count = 0;
	u32 kw_keyvalid;
	u32 slot_offset = FMP_KW_SECUREKEY + (slot * FMP_KW_SECUREKEY_OFFSET);

	union {
		u8 bytes[AES_256_XTS_KEY_SIZE];
		u32 words[AES_256_XTS_KEY_SIZE / sizeof(u32)];
	} fmp_key;

	if (slot > EXYNOS_FMP_FIPS_KEYSLOT)
		return;

	/* Make Zeroised key */
	memzero_explicit(&fmp_key, AES_256_XTS_KEY_SIZE);

	for (i = 0, limit = AES_256_XTS_KEY_SIZE / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel(handle, le32_to_cpu(fmp_key.words[i]),
			slot_offset + ((i + AES_256_XTS_TWK_OFFSET) * sizeof(u32)));
		ufsp_writel(handle, le32_to_cpu(fmp_key.words[i + AES_256_XTS_TWK_OFFSET]),
			slot_offset + (i * sizeof(u32)));
	}
	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl(handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			pr_warn("%s: Key slot #%d is not zeroised yet - 0x%x\n",
					__func__, slot, kw_keyvalid);
			udelay(2);
			count++;
			continue;
		} else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << slot))) {
		pr_err("%s: Key slot #%d is not zeroised\n", __func__, slot);
		return;
	}

	pr_debug("%s: Key valid = 0x%x\n", __func__, kw_keyvalid);
}
#endif

#ifndef CONFIG_KEYS_IN_PRDT
int exynos_fmp_clear(struct fmp_handle *handle, int slot)
{
	struct exynos_fmp *fmp = get_fmp();

	if (slot > EXYNOS_FMP_FIPS_KEYSLOT) {
		dev_err(fmp->dev, "%s: unable to clear %d keyslot. Keyslot num exceeded\n",
				__func__, slot);
		return -EINVAL;
	}

	if (!handle) {
		dev_err(fmp->dev, "%s: vacant ufs handle\n", __func__);
		return -EINVAL;
	}

	fmplib_clear_file_key((struct ufs_vs_handle *)handle, slot);

	return 0;
}
EXPORT_SYMBOL(exynos_fmp_clear);
#endif /* CONFIG_KEYS_IN_PRDT */

static void fmplib_bypass(void *desc, bool cmdq_enabled)
{
	if (cmdq_enabled) {
		SET_CMDQ_FAS((struct fmp_table_setting *)desc, 0);
		SET_CMDQ_DAS((struct fmp_table_setting *)desc, 0);
	} else {
		SET_FAS((struct fmp_table_setting *)desc, 0);
		SET_DAS((struct fmp_table_setting *)desc, 0);
	}
}

static bool fmp_check_fips(struct bio *bio, struct exynos_fmp *fmp)
{
	struct page *page;
	struct buffer_head *bh;
	bool find = false;
	struct fmp_crypto_info *ci;

	if (bio->bi_io_vec)  {
		page = bio->bi_io_vec[0].bv_page;
		if (page && !PageAnon(page) && page_has_buffers(page)) {
			bh = page_buffers(page);
			if (bh != fmp->bh) {
				dev_err(fmp->dev, "%s: invalid fips bh\n", __func__);
				return false;
			}

			if (bh && ((void *)bh->b_private == (void *)fmp))
				find = true;
		}
	}

	if (find) {
		fmp->fips_fin++;
		ci = &fmp->test_data->ci;
		dev_dbg(fmp->dev, "%s: find fips run(%d) with algo:%d, enc:%d, key_size:%d\n",
			__func__, fmp->fips_run, ci->algo_mode, ci->enc_mode, ci->key_size);
		if (ci->algo_mode == (EXYNOS_FMP_ALGO_MODE_TEST | EXYNOS_FMP_BYPASS_MODE)) {
			dev_dbg(fmp->dev, "%s: find fips for bypass mode\n", __func__);
			return 0;
		}
	}

	return find;
}

static bool fmp_check_fips_clean(struct bio *bio, struct exynos_fmp *fmp)
{
	bool find = false;
	struct page *page;
	struct buffer_head *bh;
	struct fmp_crypto_info *ci;

	if (bio->bi_io_vec)  {
		page = bio->bi_io_vec[0].bv_page;
		if (page && !PageAnon(page) && page_has_buffers(page)) {
			bh = page_buffers(page);
			if (bh != fmp->bh) {
				dev_err(fmp->dev, "%s: invalid fips bh\n", __func__);
				return false;
			}

			if (bh && ((void *)bh->b_private == (void *)fmp))
				find = true;
		}
	}

	if (find) {
		fmp->fips_fin--;
		ci = &fmp->test_data->ci;
		dev_dbg(fmp->dev, "%s: find fips run(%d) fin(%d)with algo:%d, enc:%d, key_size:%d\n",
				__func__, fmp->fips_run, fmp->fips_fin, ci->algo_mode, ci->enc_mode, ci->key_size);
		if (ci->algo_mode == (EXYNOS_FMP_ALGO_MODE_TEST | EXYNOS_FMP_BYPASS_MODE)) {
			dev_dbg(fmp->dev, "%s: find fips for bypass mode\n", __func__);
			return false;
		}
	}

	return find;
}

bool is_fmp_fips_op(struct bio *bio)
{
	struct exynos_fmp *fmp = get_fmp();

	if (!fmp->fips_run)
		return false;

	if (!fmp->result.overall && fmp_check_fips(bio, fmp)) {
		dev_dbg(fmp->dev, "%s: find fips\n", __func__);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(is_fmp_fips_op);

bool is_fmp_fips_clean(struct bio *bio)
{
	struct exynos_fmp *fmp = get_fmp();

	if (fmp->fips_fin && fmp_check_fips_clean(bio, fmp)) {
		dev_dbg(fmp->dev, "%s: find fips\n", __func__);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(is_fmp_fips_clean);

int get_fmp_fips_state(void)
{
	if (unlikely(in_fmp_fips_err())) {
#if defined(CONFIG_NODE_FOR_SELFTEST_FAIL)
		pr_err("%s: Fail to work fmp config due to fips in error.\n", __func__);
#else
		panic("%s: Fail to work fmp config due to fips in error\n", __func__);
#endif
		return -EINVAL;
	}

	return 0;
}

int exynos_fmp_bypass(struct fmp_table_setting *table, u32 prdt_cnt, unsigned long prdt_off)
{
	u32 i;
	void *prd = table;
	for (i = 0; i < prdt_cnt; i++) {
		fmplib_bypass(prd, 0);
		prd = (void *)prd + prdt_off;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_fmp_bypass);

#define CFG_DESCTYPE_3 0x3
static void *exynos_fmp_init(struct platform_device *pdev)
{
	struct exynos_fmp *fmp;
	struct device_node *np;
	u8 dun_swap;
	int ret = 0;

	if (!pdev) {
		pr_err("%s: Invalid platform_device.\n", __func__);
		goto err;
	}

	fmp = devm_kzalloc(&pdev->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (!fmp)
		goto err;

	fmp->dev = &pdev->dev;
	if (!fmp->dev) {
		pr_err("%s: Invalid device.\n", __func__);
		goto err_dev;
	}

	atomic_set(&fmp->fips_start, 0);

	ret = exynos_fmp_fips_register(fmp);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to exynos_fmp_fips_register. ret(0x%x)",
				__func__, ret);
		goto err_dev;
	}

	/* Check fmp status for featuring */
	np = fmp->dev->of_node;
	ret = of_property_read_u8(np, "dun-swap", &dun_swap);
	if (ret) {
		dev_info(fmp->dev, "read dun_swap failed = 0x%x, set to 0\n", __func__, ret);
		dun_swap = 0;
	}
	fmp->dun_swap = dun_swap;
	dev_info(fmp->dev, "Exynos FMP dun swap = %d\n", fmp->dun_swap);
	fmp->fips_run = 0;
	dev_info(fmp->dev, "Exynos FMP driver is initialized\n");
	dev_info(fmp->dev, "Exynos FMP driver Version: %s\n", FMP_DRV_VERSION);
	return fmp;

err_dev:
	devm_kfree(&pdev->dev, fmp);
err:
	return NULL;
}

void exynos_fmp_exit(struct platform_device *pdev)
{
	struct exynos_fmp *fmp = dev_get_drvdata(&pdev->dev);

	exynos_fmp_fips_deregister(fmp);
	devm_kfree(&pdev->dev, fmp);
}

static int exynos_fmp_probe(struct platform_device *pdev)
{
	struct exynos_fmp *fmp_ctx = exynos_fmp_init(pdev);
	int ret;

	if (!fmp_ctx) {
		dev_err(&pdev->dev,
			"%s: Fail to get fmp_ctx\n", __func__);
		return -EINVAL;
	}
	fmp_dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, fmp_ctx);

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	if (ret) {
		dev_warn(&pdev->dev,
			"%s: No suitable DMA available (%d)\n", __func__, ret);
	}

	return 0;
}

static int exynos_fmp_remove(struct platform_device *pdev)
{
	exynos_fmp_exit(pdev);

	dev_info(&pdev->dev, "%s: complete remove fmp driver\n", __func__);
	return 0;
}

static const struct of_device_id exynos_fmp_match[] = {
	{ .compatible = "samsung,exynos-fmp" },
	{},
};

static struct platform_driver exynos_fmp_driver = {
	.driver = {
		   .name = "exynos-fmp",
			.owner = THIS_MODULE,
			.pm = NULL,
		   .of_match_table = exynos_fmp_match,
		   },
	.probe = exynos_fmp_probe,
	.remove = exynos_fmp_remove,
};
module_platform_driver(exynos_fmp_driver);

MODULE_DESCRIPTION("FMP driver");
MODULE_AUTHOR("Boojin Kim <boojin.kim@samsung.com>");
MODULE_LICENSE("GPL");
