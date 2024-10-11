// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Exynos FMP (Flash Memory Protector) UFS crypto support
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 * Copyright 2020 Google LLC
 *
 * Authors: Boojin Kim <boojin.kim@samsung.com>
 *	    Eric Biggers <ebiggers@google.com>
 */

#include <asm/unaligned.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/of.h>

#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufs-vs-mmio.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#include "ufs-exynos-fmp.h"
#include <trace/hooks/ufshcd.h>
#ifdef CONFIG_EXYNOS_FMP_FIPS
#include <crypto/fmp_fips.h>
#endif
#ifndef CONFIG_KEYS_IN_PRDT
#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
struct device dev_fmp;
#endif
#define MAX_RETRY_COUNT 0x100
#endif

#ifdef CONFIG_EXYNOS_FIPS_SIMULATOR
static struct fmp_handle *fmp_ufs_handle;

struct fmp_handle *get_fmp_handle()
{
	return fmp_ufs_handle;
}
EXPORT_SYMBOL(get_fmp_handle);

static int FIPS_keyslot_num;

void set_fips_keyslot_num(int num)
{
	FIPS_keyslot_num = num;
}
EXPORT_SYMBOL(set_fips_keyslot_num);
#endif

#ifdef CONFIG_KEYS_IN_PRDT
static inline __be32 fmp_key_word(const u8 *key, int j)
{
	return cpu_to_be32(get_unaligned_le32(
			key + AES_KEYSIZE_256 - (j + 1) * sizeof(__le32)));
}

/* Configure FMP on requests that have an encryption context. */
static void exynos_ufs_fmp_fill_prdt(void *ignore, struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
					 unsigned int segments, int *err)
{
#ifdef CONFIG_EXYNOS_FMP_FIPS
	struct exynos_fmp_crypt_info fmp_ci;
	int ret;
#endif
	const struct bio_crypt_ctx *bc;
	const u8 *enckey, *twkey;
	u64 dun_lo, dun_hi;
	struct fmp_sg_entry *table;
	unsigned int i;
	*err = 0;

	/*
	 * There's nothing to do for unencrypted requests, since the mode field
	 * ("FAS") is already 0 (FMP_BYPASS_MODE) by default, as it's in the
	 * same word as ufshcd_sg_entry::size which was already initialized.
	 */
	bc = lrbp->cmd->request->crypt_ctx;
	BUILD_BUG_ON(FMP_BYPASS_MODE != 0);
#ifdef CONFIG_EXYNOS_FMP_FIPS
	if (!bc) {
		if (!(lrbp->cmd->request->bio))
			return;

		if (is_fmp_fips_op(lrbp->cmd->request->bio)) {
			fmp_ci.fips = true;
			goto encrypt;
		}
		return;
	}

	enckey = bc->bc_key->raw;
	twkey = enckey + AES_KEYSIZE_256;
	dun_lo = bc->bc_dun[0];
	dun_hi = bc->bc_dun[1];
	fmp_ci.fips = false;
encrypt:
#else /* CONFIG_EXYNOS_FMP_FIPS */
	if (!bc)
		return;
#endif /* CONFIG_EXYNOS_FMP_FIPS */

	/* If FMP wasn't enabled, we shouldn't get any encrypted requests. */
	if (WARN_ON_ONCE(!(hba->caps & UFSHCD_CAP_CRYPTO))) {
		dev_err(hba->dev, "UFSHCD_CAP_CRYPTO is not enabled\n");
		*err = -ENOTSUPP;
		return;
	}

#ifdef CONFIG_EXYNOS_FMP_FIPS
	table = (struct fmp_sg_entry *)lrbp->ucd_prdt_ptr;
	for (i = 0; i < segments; i++) {
		struct fmp_sg_entry *ent = &table[i];
		struct ufshcd_sg_entry *prd = (struct ufshcd_sg_entry *)ent;

		/* Each segment must be exactly one data unit. */
		if (le32_to_cpu(prd->size) + 1 != FMP_DATA_UNIT_SIZE) {
			dev_err(hba->dev, "scatterlist segment is misaligned for FMP\n");
			*err = -EINVAL;
			return;
		}

		fmp_ci.enckey = enckey;
		fmp_ci.twkey = twkey;
		fmp_ci.dun_lo = dun_lo;
		fmp_ci.dun_hi = dun_hi;

		ret = exynos_fmp_crypt(&fmp_ci, (void *)ent);
		if (ret) {
			dev_err(hba->dev, "%s: fails to crypt fmp. ret:%d\n", __func__, ret);
			*err = ret;
			return;
		}

		/* Increment the data unit number. */
		dun_lo++;
		if (dun_lo == 0)
			dun_hi++;
	}
#else
	enckey = bc->bc_key->raw;
	twkey = enckey + AES_KEYSIZE_256;
	dun_lo = bc->bc_dun[0];
	dun_hi = bc->bc_dun[1];

	/* Reject weak AES-XTS keys. */
	if (!crypto_memneq(enckey, twkey, AES_KEYSIZE_256)) {
		dev_err(hba->dev, "Can't use weak AES-XTS key\n");
		*err = -EINVAL;
		return;
	}
	/* Configure FMP on each segment of the request. */
	table = (struct fmp_sg_entry *)lrbp->ucd_prdt_ptr;
	for (i = 0; i < segments; i++) {
		struct fmp_sg_entry *ent = &table[i];
		struct ufshcd_sg_entry *prd = (struct ufshcd_sg_entry *)ent;
		size_t j, limit;

		/* Each segment must be exactly one data unit. */
		if (le32_to_cpu(prd->size) + 1 != FMP_DATA_UNIT_SIZE) {
			dev_err(hba->dev, "scatterlist segment is misaligned for FMP\n");
			*err = -EINVAL;
			return;
		}

		/* Set the algorithm and key length. */
		SET_FAS(ent, FMP_ALGO_MODE_AES_XTS);
		SET_KEYLEN(ent, FKL);

		/* Set the key. */
		for (j = 0, limit = AES_KEYSIZE_256 / sizeof(u32); j < limit; j++) {
			ent->file_enckey[j] = fmp_key_word(enckey, j);
			ent->file_twkey[j] = fmp_key_word(twkey, j);
		}

		/* Set the IV. */
		ent->file_iv[0] = cpu_to_be32(upper_32_bits(dun_hi));
		ent->file_iv[1] = cpu_to_be32(lower_32_bits(dun_hi));
		ent->file_iv[2] = cpu_to_be32(upper_32_bits(dun_lo));
		ent->file_iv[3] = cpu_to_be32(lower_32_bits(dun_lo));

		/* Increment the data unit number. */
		dun_lo++;
		if (dun_lo == 0)
			dun_hi++;
	}
#endif
	return;
}
#else
static void exynos_ufs_fmp_populate_dt(struct device *dev, struct exynos_fmp* fmp)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int ret = 0;

	/* Check fmp status for featuring */
	child_np = of_get_child_by_name(np, "ufs-protector");
	if (!child_np) {
		dev_info(dev, "No ufs-protector node, set to 1 for boot\n");
		fmp->valid_check = 1;
	}
	else {
		ret = of_property_read_u8(child_np, "valid-check", &fmp->valid_check);
		if (ret) {
			dev_info(dev, "read valid_check failed = 0x%x, set to 1\n", __func__, ret);
			fmp->valid_check = 1;
		}
	}
}

void exynos_ufs_fmp_set_crypto_cfg(struct ufs_hba *hba)
{
	union exynos_ufs_crypto_cfg_entry cfg = {};
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* Set crypto config register.*/
	cfg.data_unit_size = FMP_DATA_UNIT_SIZE / 512;
	cfg.config_enable = UFS_CRYPTO_CONFIGURATION_ENABLE;
	std_writel(&ufs->handle, le32_to_cpu(cfg.reg_val), CRYPTOCFG);

	dev_info(hba->dev, "%s set CRYPTOCFG = 0x%x\n", __func__, std_readl(&ufs->handle, CRYPTOCFG));

	return;
}

static void exynos_ufs_fmp_prepare_command(void *ignore, struct ufs_hba *hba, struct request *rq,
					struct ufshcd_lrb *lrbp, int *err)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp*)ufs->fmp;
#ifdef CONFIG_EXYNOS_FMP_FIPS
	const struct bio_crypt_ctx *bc;
	int ret;
	struct exynos_fmp_crypt_info fmp_ci;

	BUILD_BUG_ON(FMP_BYPASS_MODE != 0);
	bc = lrbp->cmd->request->crypt_ctx;
	fmp_ci.fips = false;

	if (!bc) {
		if (!(lrbp->cmd->request->bio)) {
			*err = 0;
			return;
		}

		if (is_fmp_fips_op(lrbp->cmd->request->bio))
			fmp_ci.fips = true;
		else
			goto out;
	}

	ret = exynos_fmp_crypt(&fmp_ci, (void *)&ufs->handle);
	if (ret) {
		dev_err(hba->dev, "%s: fail to crypt with fmp. ret:%d\n", __func__, ret);
		*err = ret;
		return;
	}

	if (fmp_ci.fips) {
#ifdef CONFIG_EXYNOS_FIPS_SIMULATOR
		if ((FIPS_keyslot_num >= 0) && (FIPS_keyslot_num <= 15))
			fmp_ci.crypto_key_slot = FIPS_keyslot_num;
#endif
		lrbp->crypto_key_slot = fmp_ci.crypto_key_slot;
		lrbp->data_unit_num = fmp_ci.data_unit_num;
		*err = 0;
		return;
	}
out:
#endif /* CONFIG_EXYNOS_FMP_FIPS */
	if ((lrbp->crypto_key_slot >= 0) && (fmp->valid_check == 1))
		lrbp->crypto_key_slot++; /* account for hardware quirk */
	*err = 0;
}
#endif

#ifdef CONFIG_KEYS_IN_PRDT
void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;

#ifndef CONFIG_EXYNOS_FMP_FIPS
	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
#endif
	dev_info(hba->dev, "KEYS_IN_PRDT\n");

	ret = exynos_smc(SMC_CMD_SMU, SMU_INIT, FMP_EMBEDDED, 0);
	if (ret)
		dev_warn(hba->dev, "SMC_CMD_SMU(SMU_INIT) failed: %ld\n", ret);

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_3);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n", ret);
		goto disable;
	}
	else
		hba->sg_entry_size = sizeof(struct fmp_sg_entry);

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SWKEY_MODE);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	hba->quirks |= UFSHCD_QUIRK_FMP_MODE_SPECIFIC |
		       UFSHCD_QUIRK_FMP_SOC_SPECIFIC;
	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/* Advertise crypto capabilities to the block layer. */
	blk_ksm_init_passthrough(&hba->ksm);
	hba->ksm.max_dun_bytes_supported = AES_BLOCK_SIZE;
	hba->ksm.features = BLK_CRYPTO_FEATURE_STANDARD_KEYS;
	hba->ksm.dev = hba->dev;
	hba->ksm.crypto_modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
		FMP_DATA_UNIT_SIZE;

	register_trace_android_vh_ufs_fill_prdt(exynos_ufs_fmp_fill_prdt, NULL);
	return;

disable:
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_KEYS_IN_PRDT */
#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
static int exynos_ufs_fmp_keyslot_program(struct blk_keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int keyslot)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp*)ufs->fmp;
	unsigned int num_keyslots = NUM_KEYSLOTS;
	unsigned int slot = keyslot;
	size_t i, limit;
	u32 count = 0;
	u32 kw_keyvalid;
	u32 kw_tagabort;
	u32 kw_pbkabort;
	u32 kw_control;
	u32 slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;
	u32 tag_offset = FMP_KW_TAG + slot*FMP_KW_TAG_OFFSET;
	u32 secret_size = SECRET_SIZE + AES_GCM_TAG_SIZE;

	union {
		u8 bytes[AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE];
		u32 words[(AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE) / sizeof(u32)];
	} fmp_key;

	if (fmp->valid_check == 1) {
		slot += 1;
		num_keyslots -= 1;
	}

	slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;
	tag_offset = FMP_KW_TAG + slot*FMP_KW_TAG_OFFSET;

	/* To Do
	 * This is from ufshcd-crypto. Should check if this is needed.
	 */
	// ufshcd_hold(hba, false);

	/* Only AES-256-XTS is supported */
	if (key->crypto_cfg.crypto_mode != BLK_ENCRYPTION_MODE_AES_256_XTS ||
		(key->size - secret_size) != (AES_256_XTS_KEY_SIZE + AES_GCM_TAG_SIZE)) {
		dev_err(hba->dev,
			"Unhandled crypto capability; crypto_mode=%d, key_size=%d\n",
			key->crypto_cfg.crypto_mode, key->size - secret_size);
		return -EINVAL;
	}

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, num_keyslots);

	/* In XTS mode, the blk_crypto_key's size is already doubled */
	memcpy(fmp_key.bytes, key->raw, key->size - secret_size);

	/* Key program in keyslot */
	/* Swap File key and Tweak key */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key.words[i]),
			slot_offset + ((i + AES_256_XTS_TWK_OFFSET) * sizeof(u32)));
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key.words[i + AES_256_XTS_TWK_OFFSET]),
			slot_offset + (i * sizeof(u32)));
	}

	/* KEY TAG */
	for (i = 0, limit = AES_GCM_TAG_SIZE / sizeof(u32); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key.words[i + HW_WRAPPED_KEY_TAG_OFFSET]),
			tag_offset + (i * sizeof(u32)));
	}

	memzero_explicit(&fmp_key, key->size - secret_size);

	/* Unwrap command */
	kw_control = (1<<slot);
	ufsp_writel(&ufs->handle, kw_control, FMP_KW_CONTROL);

	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << slot))) {
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);

		kw_tagabort = ufsp_readl(&ufs->handle, FMP_KW_TAGABORT);
		kw_pbkabort = ufsp_readl(&ufs->handle, FMP_KW_PBKABORT);
		dev_err(hba->dev, "tag abort = 0x%x, pbk abort = 0x%x\n", kw_tagabort, kw_pbkabort);

		// set to clear
		if (kw_tagabort)
			ufsp_writel(&ufs->handle, 1 << slot, FMP_KW_TAGABORT);
		if (kw_pbkabort)
			ufsp_writel(&ufs->handle, 1, FMP_KW_PBKABORT);

		kw_tagabort = ufsp_readl(&ufs->handle, FMP_KW_TAGABORT);
		kw_pbkabort = ufsp_readl(&ufs->handle, FMP_KW_PBKABORT);
		dev_err(hba->dev, "set to clear tag abort = 0x%x, pbk abort = 0x%x\n", kw_tagabort, kw_pbkabort);
		return -EINVAL;
	}
	else {
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
	}

	/* To Do
	 * This is from ufshcd-crypto. Should check if this is needed.
	 */
	// ufshcd_release(hba);

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

static int exynos_ufs_fmp_keyslot_evict(struct blk_keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	/* Nothing To Do */
        return 0;
}

static int exynos_ufs_fmp_derive_raw_secret(struct blk_keyslot_manager *ksm,
					const u8 *wrapped_key,
					unsigned int wrapped_key_size,
					u8 *secret, unsigned int secret_size)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	unsigned long smc_ret = 0;
	int ret = 0;
	u8 *fmp_key;
	dma_addr_t fmp_key_pa;
	u8 *secret_key;
	dma_addr_t secret_key_pa;

	/* Allocate key buffer as non-cacheable */
	fmp_key = dmam_alloc_coherent(&dev_fmp, wrapped_key_size, &fmp_key_pa, GFP_KERNEL);
	if (!fmp_key) {
		dev_err(hba->dev, "Fail to allocate memory for fmp_key\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Allocate secret buffer as non-cacheable */
	secret_key = dmam_alloc_coherent(&dev_fmp, secret_size, &secret_key_pa, GFP_KERNEL);
	if (!secret_key) {
		dev_err(hba->dev, "Fail to allocate memory for secret_key\n");
		ret = -ENOMEM;
		goto free_fmp_key;
	}

	memcpy(fmp_key, wrapped_key, wrapped_key_size);

	smc_ret = exynos_smc(SMC_CMD_FMP_SECRET, fmp_key_pa, secret_key_pa, secret_size);
	if (smc_ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_SECRET failed: %ld\n", smc_ret);
		ret = -EINVAL;
		goto free_secret_key;
	}

	memcpy(secret, secret_key, secret_size);

free_secret_key:
	memzero_explicit(secret_key, secret_size);
	dmam_free_coherent(&dev_fmp, secret_size, secret_key, secret_key_pa);
free_fmp_key:
	memzero_explicit(fmp_key, wrapped_key_size);
	dmam_free_coherent(&dev_fmp, wrapped_key_size, fmp_key, fmp_key_pa);
out:
	return ret;
}

static const struct blk_ksm_ll_ops exynos_ufs_fmp_ksm_ll_ops = {
	.keyslot_program	= exynos_ufs_fmp_keyslot_program,
	.keyslot_evict		= exynos_ufs_fmp_keyslot_evict,
	.derive_raw_secret	= exynos_ufs_fmp_derive_raw_secret,
};

void key_program_slot0(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned long ret;
	size_t i, limit;
	u32 kw_keyvalid;
	u32 kw_control;
	u32 count = 0;
	int slot = 0;
	u32 *fmp_key;
	u32 *tag;

	/* Key program in keyslot #0 */
	u32 slot_offset = FMP_KW_SECUREKEY;
	u32 tag_offset = FMP_KW_TAG;

	/* The Galois/Counter Mode of Operation (GCM) Test Case #15 */
	/* P: plaine text = {
		0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
		0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
		0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
		0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
		0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
		0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
		0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
		0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55
	};
	*/

	/* C: cipher text */
	u8 c[AES_256_XTS_KEY_SIZE] = {
		0x52, 0x2d, 0xc1, 0xf0, 0x99, 0x56, 0x7d, 0x07,
		0xf4, 0x7f, 0x37, 0xa3, 0x2a, 0x84, 0x42, 0x7d,
		0x64, 0x3a, 0x8c, 0xdc, 0xbf, 0xe5, 0xc0, 0xc9,
		0x75, 0x98, 0xa2, 0xbd, 0x25, 0x55, 0xd1, 0xaa,
		0x8c, 0xb0, 0x8e, 0x48, 0x59, 0x0d, 0xbb, 0x3d,
		0xa7, 0xb0, 0x8b, 0x10, 0x56, 0x82, 0x88, 0x38,
		0xc5, 0xf6, 0x1e, 0x63, 0x93, 0xba, 0x7a, 0x0a,
		0xbc, 0xc9, 0xf6, 0x62, 0x89, 0x80, 0x15, 0xad
	};

	/* T: tag */
	u8 t[AES_GCM_TAG_SIZE] = {
		0xb0, 0x94, 0xda, 0xc5, 0xd9, 0x34, 0x71, 0xbd,
		0xec, 0x1a, 0x50, 0x22, 0x70, 0xe3, 0xcc, 0x6c
	};

	fmp_key = (u32 *)c;
	tag = (u32 *)t;

	/* Set key, tag, iv, perboot keky */
	ret = exynos_smc(SMC_CMD_FMP_KW_SYSREG_DUMMY, 0, 0, 0);
        if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_SYSREG_DUMMY failed: %ld\n", ret);
		return;
        }

	/* Hw wrapped key */
	/* Swap File key and Tweak key */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(*(fmp_key + i)),
			slot_offset + (i + AES_256_XTS_TWK_OFFSET) * sizeof(u32));
		ufsp_writel(&ufs->handle, le32_to_cpu(*(fmp_key + i + AES_256_XTS_TWK_OFFSET)),
			slot_offset + (i * sizeof(u32)));
	}

	/* tag */
	for (i = 0, limit = AES_GCM_TAG_SIZE / sizeof(u32); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(*(tag + i)),
			tag_offset + (i * sizeof(u32)));
	}

	/* Unwrap command */
	kw_control = (1<<slot);
	ufsp_writel(&ufs->handle, kw_control, FMP_KW_CONTROL);

	/* Keyslot #0 should be valid for normal IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << slot)))
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);
	else
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
}

void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err;
	unsigned int kw_indataswap;
	int num_keyslots = NUM_KEYSLOTS;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp;

#ifndef CONFIG_EXYNOS_FMP_FIPS
	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
#endif
	dev_info(hba->dev, "HW_KEYS_IN_CUSTOM_KEYSLOT\n");

#ifdef CONFIG_EXYNOS_FIPS_SIMULATOR
	fmp_ufs_handle = (struct fmp_handle *)&ufs->handle;
	FIPS_keyslot_num = -1;
#endif
	ufs->fmp = devm_kzalloc(ufs->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (ufs->fmp == NULL) {
		dev_warn(hba->dev, "failed to alloc ufs->fmp\n");
		goto disable_nofree;
	}
	fmp = (struct exynos_fmp *)ufs->fmp;

	ret = exynos_smc(SMC_CMD_SMU, SMU_INIT, FMP_EMBEDDED, 0);
	if (ret)
		dev_warn(hba->dev, "SMC_CMD_SMU(SMU_INIT) failed: %ld\n", ret);

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_0);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n", ret);
		goto disable;
	}

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SECUREKEY_MODE | UNWRAP_EN | WAP0_NS);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	hba->quirks |= UFSHCD_QUIRK_FMP_MODE_SPECIFIC |
			UFSHCD_QUIRK_FMP_SOC_SPECIFIC;
	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/*
	 * Set data swap mode
	 * secure key/tag swap options are for swapping key at writing key in slot
	 * Ohter options are for key unwrap and as same for all secure key mode
	 */
	kw_indataswap = 0;
	kw_indataswap |= (SECURE_FILEKEY_WORDSWAP | SECURE_TWEAKKEY_WORDSWAP |
			SECURE_FILEKEY_BYTESWAP | SECURE_TWEAKKEY_BYTESWAP |
			PBK_WORDSWAP | IV_WORDSWAP | TAG_WORDSWAP |
			PBK_BYTESWAP | IV_BYTESWAP | TAG_BYTESWAP);

	ret = exynos_smc(SMC_CMD_FMP_KW_INDATASWAP, 0, FMP_EMBEDDED, kw_indataswap);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_INDATASWAP failed: %ld\n", ret);
		goto disable;
	}

	exynos_ufs_fmp_populate_dt(ufs->dev, fmp);
	dev_info(ufs->dev, "ufs->fmp valid_check = %d\n", fmp->valid_check);

	/* Key program slot #0 to make it valid for non crypto IO. */
	if (fmp->valid_check == 1) {
		num_keyslots -= 1;
		key_program_slot0(hba);
	}

	/* Write Perboot key, IV and Valid bit setting */
	ret = exynos_smc(SMC_CMD_FMP_KW_SYSREG, 0, 0, 0);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_SYSREG failed: %ld\n", ret);
		goto disable;
	}

	err = blk_ksm_init(&hba->ksm, num_keyslots);
	if (err) {
		dev_warn(hba->dev, "blk_ksm_init failed: %d\n", err);
		goto disable;
	}

	hba->ksm.ksm_ll_ops = exynos_ufs_fmp_ksm_ll_ops;
	hba->ksm.max_dun_bytes_supported = 8;
	hba->ksm.features = BLK_CRYPTO_FEATURE_WRAPPED_KEYS;

	hba->ksm.dev = hba->dev;
	hba->ksm.crypto_modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
		FMP_DATA_UNIT_SIZE;


	device_initialize(&dev_fmp);
	err = dma_coerce_mask_and_coherent(&dev_fmp, DMA_BIT_MASK(36));
	if (err) {
		dev_warn(hba->dev, "No suitable DMA available: %d\n", err);
		goto disable;
	}

	register_trace_android_vh_ufs_prepare_command(exynos_ufs_fmp_prepare_command, NULL);
	return;

disable:
	devm_kfree(ufs->dev, ufs->fmp);
disable_nofree:
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif /* CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT */
#ifdef CONFIG_KEYS_IN_CUSTOM_KEYSLOT
/*
 * Program a key into a FMP custom keyslot, or evict a keyslot.
 */
static int exynos_ufs_fmp_keyslot_program(struct blk_keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int keyslot)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp *)ufs->fmp;
	unsigned int num_keyslots = NUM_KEYSLOTS;
	unsigned int slot = keyslot;
#ifdef CONFIG_EXYNOS_FMP_FIPS
	int ret;
	struct exynos_fmp_key_info fmp_ki;
#else
	size_t i, limit;
	u32 count = 0;
	u32 kw_keyvalid;
	u32 slot_offset;
	const u8 *enckey, *twkey;
	union {
		u8 bytes[AES_256_XTS_KEY_SIZE];
		u32 words[AES_256_XTS_KEY_SIZE / sizeof(u32)];
	} fmp_key;
#endif
	if(fmp->valid_check == 1) {
		slot += 1;
		num_keyslots -= 1;
	}

	/* To Do
	 * This is from ufshcd-crypto. Should check if this is needed.
	 */
	// ufshcd_hold(hba, false);

	/* Only AES-256-XTS is supported */
	if (key->crypto_cfg.crypto_mode != BLK_ENCRYPTION_MODE_AES_256_XTS ||
		key->size != AES_256_XTS_KEY_SIZE) {
		dev_err(hba->dev,
			"Unhandled crypto capability; crypto_mode=%d, key_size=%d\n",
			key->crypto_cfg.crypto_mode, key->size);
		return -EINVAL;
	}

	dev_info(hba->dev, "%s slot = %d/%d\n",__func__, slot, num_keyslots);
#ifdef CONFIG_EXYNOS_FMP_FIPS
	fmp_ki.raw = key->raw;
	fmp_ki.size = key->size;
	fmp_ki.slot = slot;
	ret = exynos_fmp_setkey(&fmp_ki, (struct fmp_handle *)&ufs->handle);
	if (ret) {
		dev_err(hba->dev, "%s: Fail to set FMP key in keyslot (ret: %d)\n", __func__, ret);
		return ret;
	}
#else
	/* In XTS mode, the blk_crypto_key's size is already doubled */
	memcpy(fmp_key.bytes, key->raw, key->size);

	enckey = key->raw;
	twkey = enckey + AES_KEYSIZE_256;

	slot_offset = FMP_KW_SECUREKEY + slot*FMP_KW_SECUREKEY_OFFSET;

	/* Reject weak AES-XTS keys */
	if (!crypto_memneq(enckey, twkey, AES_KEYSIZE_256)) {
		dev_err(hba->dev, "Can't use weak AES-XTS key\n");
		return -EINVAL;
	}
	/* Key program in keyslot */
	/* Swap File key and Tweak key */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / (sizeof(u32) * 2); i < limit; i++) {
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key.words[i]),
			slot_offset + ((i + AES_256_XTS_TWK_OFFSET) * sizeof(u32)));
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key.words[i + AES_256_XTS_TWK_OFFSET]),
			slot_offset + (i * sizeof(u32)));
	}

	/* Zeroise the key */
	memzero_explicit(&fmp_key, key->size);

	/* Keyslot should be valid for crypto IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & (0x1 << slot))) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		} else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << slot))) {
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);
		return -EINVAL;
	}

	dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
#endif
	/* To Do
	 * This is from ufshcd-crypto. Should check if this is needed.
	 */
	// ufshcd_release(hba);

	/* To Do
	 * This should be fixed to return by goto label in error cases if ufshcd control is needed.
	 */
	return 0;
}

static int exynos_ufs_fmp_keyslot_evict(struct blk_keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	/* Nothing To Do */
	return 0;
}

static const struct blk_ksm_ll_ops exynos_ufs_fmp_ksm_ll_ops = {
	.keyslot_program	= exynos_ufs_fmp_keyslot_program,
	.keyslot_evict		= exynos_ufs_fmp_keyslot_evict,
};

void key_program_slot0(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	size_t i, limit;
	u32 kw_keyvalid;
	u32 count = 0;
	int slot = 0;
	u32 fmp_key = 0;

	/* Key program in keyslot #0 */
	u32 slot_offset = FMP_KW_SECUREKEY;

	/* Swap File key and Tweak key */
	for (i = 0, limit = AES_256_XTS_KEY_SIZE / sizeof(u32); i < limit; i++)
		ufsp_writel(&ufs->handle, le32_to_cpu(fmp_key), slot_offset + (i * sizeof(fmp_key)));

	/* Keyslot #0 should be valid for normal IO */
	do {
		kw_keyvalid = ufsp_readl(&ufs->handle, FMP_KW_KEYVALID);
		if (!(kw_keyvalid & 0x1)) {
			dev_warn(hba->dev, "Key slot #%d is not valid yet\n", slot);
			udelay(2);
			count++;
			continue;
		}
		else {
			break;
		}
	} while (count < MAX_RETRY_COUNT);

	if (!(kw_keyvalid & (0x1 << slot)))
		dev_err(hba->dev, "Key slot #%d is not valid\n", slot);
	else
		dev_info(hba->dev, "%s Key valid = 0x%x\n", __func__, kw_keyvalid);
}

void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
	unsigned long ret;
	int err;
	unsigned int kw_indataswap;
	int num_keyslots = NUM_KEYSLOTS;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp;

#ifndef CONFIG_EXYNOS_FMP_FIPS
	dev_info(hba->dev, "Exynos FMP Version: %s\n", FMP_DRV_VERSION);
#endif
	dev_info(hba->dev, "KEYS_IN_CUSTOM_KEYSLOT\n");
#ifdef CONFIG_EXYNOS_FIPS_SIMULATOR
	fmp_ufs_handle = (struct fmp_handle *)&ufs->handle;
	FIPS_keyslot_num = -1;
#endif
	ufs->fmp = devm_kzalloc(ufs->dev, sizeof(struct exynos_fmp), GFP_KERNEL);
	if (ufs->fmp == NULL) {
		dev_warn(hba->dev, "failed to alloc ufs->fmp\n");
		goto disable_nofree;
	}
	fmp = (struct exynos_fmp *)ufs->fmp;

	ret = exynos_smc(SMC_CMD_SMU, SMU_INIT, FMP_EMBEDDED, 0);
	if (ret)
		dev_warn(hba->dev, "SMC_CMD_SMU(SMU_INIT) failed: %ld\n", ret);

	ret = exynos_smc(SMC_CMD_FMP_SECURITY, 0, FMP_EMBEDDED, CFG_DESCTYPE_0);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_SECURITY failed on init: %ld\n", ret);
		goto disable;
	}

	ret = exynos_smc(SMC_CMD_FMP_KW_MODE, 0, FMP_EMBEDDED, SECUREKEY_MODE | UNWRAP_BYPASS);
	if (ret) {
		dev_warn(hba->dev, "SMC_CMD_FMP_KW_MODE failed on init: %ld\n", ret);
		goto disable;
	}

	/* Advertise crypto support to ufshcd-core. */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Advertise crypto quirks to ufshcd-core. */
	hba->quirks |= UFSHCD_QUIRK_FMP_MODE_SPECIFIC |
			UFSHCD_QUIRK_FMP_SOC_SPECIFIC;
	dev_info(hba->dev, "Exynos FMP quirks: 0x%x\n", hba->quirks);

	/*
	 * Set data swap mode
	 * secure key swap option is for swapping key at writing key in slot
	 * Ohter options doensn't affect bypass mode but for setting as same for all secure key mode
	 */
	kw_indataswap = 0;
	kw_indataswap |= (SECURE_FILEKEY_WORDSWAP | SECURE_TWEAKKEY_WORDSWAP |
			SECURE_FILEKEY_BYTESWAP | SECURE_TWEAKKEY_BYTESWAP |
			PBK_WORDSWAP | IV_WORDSWAP | TAG_WORDSWAP |
			PBK_BYTESWAP | IV_BYTESWAP | TAG_BYTESWAP);

	ret = exynos_smc(SMC_CMD_FMP_KW_INDATASWAP, 0, FMP_EMBEDDED, kw_indataswap);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_INDATASWAP failed: %ld\n", ret);
		goto disable;
	}

	exynos_ufs_fmp_populate_dt(ufs->dev, fmp);
	dev_info(ufs->dev, "ufs->fmp valid_check = %d\n", fmp->valid_check);

	/* Key program slot #0 to make it valid for non crypto IO. */
	if (fmp->valid_check == 1) {
		num_keyslots -= 1;
		key_program_slot0(hba);
	}

	/* Advertise crypto capabilities to the block layer. */
	err = blk_ksm_init(&hba->ksm, num_keyslots);
	if (err) {
		dev_warn(hba->dev, "blk_ksm_init failed: %d\n", err);
		goto disable;
	}

	hba->ksm.ksm_ll_ops = exynos_ufs_fmp_ksm_ll_ops;
	hba->ksm.max_dun_bytes_supported = 8;
	hba->ksm.features |= BLK_CRYPTO_FEATURE_STANDARD_KEYS;

	hba->ksm.dev = hba->dev;
	hba->ksm.crypto_modes_supported[BLK_ENCRYPTION_MODE_AES_256_XTS] =
		FMP_DATA_UNIT_SIZE;

	register_trace_android_vh_ufs_prepare_command(exynos_ufs_fmp_prepare_command, NULL);
	return;

disable:
	devm_kfree(ufs->dev, ufs->fmp);
disable_nofree:
	dev_warn(hba->dev, "Disabling inline encryption support\n");
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
}
#endif

void exynos_ufs_fmp_resume(struct ufs_hba *hba)
{
#ifndef CONFIG_KEYS_IN_PRDT
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_fmp *fmp = (struct exynos_fmp*)ufs->fmp;
#endif
	unsigned long ret;

	/* Restore all fmp registers on init - Security, Kwmode, kwindataswap */
	ret = exynos_smc(SMC_CMD_FMP_SMU_RESUME, 0, FMP_EMBEDDED, 0);
	if (ret)
		dev_err(hba->dev, "SMC_CMD_FMP_SMU_RESUME failed on resume: %ld\n", ret);

	/* Key program slot #0 to make it valid for non crypto IO. */
#ifndef CONFIG_KEYS_IN_PRDT
	if (fmp->valid_check == 1)
		key_program_slot0(hba);
#endif

#ifdef CONFIG_HW_KEYS_IN_CUSTOM_KEYSLOT
	/* Restore Perboot key, IV and Valid bit setting */
	ret = exynos_smc(SMC_CMD_FMP_KW_SYSREG, 0, 0, 0);
	if (ret) {
		dev_err(hba->dev, "SMC_CMD_FMP_KW_SYSREG failed: %ld\n", ret);
		return;
	}
#endif
}

void exynos_ufs_fmp_dump_info(struct ufs_hba *hba)
{
	int i = 0;

	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tFMP REGs\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	for( i = 0; i < FMP_REGS; i++) {
		ufs_fmp_log_sfr[i].val = exynos_smc(SMC_CMD_FMP_SMU_DUMP, 0, FMP_EMBEDDED, ufs_fmp_log_sfr[i].offset);
		dev_err(hba->dev, ": %-30s\t0x%-012x\t0x%-014x\n",
				ufs_fmp_log_sfr[i].name, ufs_fmp_log_sfr[i].offset, ufs_fmp_log_sfr[i].val);

	}
}
