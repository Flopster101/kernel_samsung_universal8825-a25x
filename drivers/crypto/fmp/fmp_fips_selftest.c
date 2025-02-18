/*
 * Exynos FMP selftest driver
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/crypto.h>

#include <crypto/fmp.h>
#include <crypto/sha256.h>
#include <crypto/hmac-sha256.h>

#include "fmp_fips_info.h"
#include "fmp_test.h"
#include "fmp_fips_cipher.h"
#include "fmp_fips_selftest.h"

static const char *ALG_SHA256_FMP = "sha256";
static const char *ALG_HMAC_SHA256_FMP = "hmac(sha256)";

static void hexdump(uint8_t *buf, uint32_t len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET,
			16, 1,
			buf, len, false);
}

static int alloc_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++) {
		buf[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!buf[i])
			goto err_free_buf;
	}

	return 0;

err_free_buf:
	while (i-- > 0)
		free_page((unsigned long)buf[i]);

	return -ENOMEM;
}

static void free_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++)
		free_page((unsigned long)buf[i]);
}



struct fmp_test_result {
	struct completion completion;
	int err;
};

static int selftest_sha256(struct exynos_fmp *fmp)
{
	int i;
	int ret;
	unsigned char buf[SHA256_DIGEST_LENGTH];
	unsigned char temp_digest[SHA256_DIGEST_LENGTH];

	for (i = 0; i < SHA256_TEST_VECTORS; i++) {
		if (0 != fmp_sha256(sha256_tv_template[i].plaintext, sha256_tv_template[i].psize, buf))
			return -EINVAL;

		memcpy(temp_digest, sha256_tv_template[i].digest, SHA256_DIGEST_LENGTH);

		ret = memcmp(buf, temp_digest, SHA256_DIGEST_LENGTH);
		if (ret) {
			print_hex_dump_bytes("FIPS SHA256 REQ: ", DUMP_PREFIX_NONE,
							sha256_tv_template[i].digest,
							SHA256_DIGEST_LENGTH);
			print_hex_dump_bytes("FIPS SHA256 RES: ", DUMP_PREFIX_NONE,
							buf,
							SHA256_DIGEST_LENGTH);
			return -EINVAL;
		}
	}
	return ret;
}

static int selftest_hmac_sha256(struct exynos_fmp *fmp)
{
	int i;
	int ret;
	unsigned char buf[SHA256_DIGEST_LENGTH];
	unsigned char temp_digest[SHA256_DIGEST_LENGTH];

	for (i = 0; i < HMAC_SHA256_TEST_VECTORS; i++) {
		if (0 != hmac_sha256(hmac_sha256_tv_template[i].key,
				hmac_sha256_tv_template[i].ksize,
				hmac_sha256_tv_template[i].plaintext,
				hmac_sha256_tv_template[i].psize,
				buf))
			return -EINVAL;

		memcpy(temp_digest, hmac_sha256_tv_template[i].digest,
				SHA256_DIGEST_LENGTH);

		ret = memcmp(buf, temp_digest, SHA256_DIGEST_LENGTH);
		if (ret) {
			print_hex_dump_bytes("FIPS HMAC-SHA256 REQ: ",
					DUMP_PREFIX_NONE,
					hmac_sha256_tv_template[i].digest,
					SHA256_DIGEST_LENGTH);
			print_hex_dump_bytes("FIPS HMAC-SHA256 RES: ",
					DUMP_PREFIX_NONE,
					buf, SHA256_DIGEST_LENGTH);
			return -EINVAL;
		}
	}
	return ret;
}

static int fmp_test_run(struct exynos_fmp *fmp, const struct cipher_testvec *template,
				const int idx, uint8_t *data, uint32_t len,
				const int mode, uint32_t write)
{
	int ret = 0;
	char *temp_key;

	temp_key = kzalloc(template->klen, GFP_KERNEL);
	if (!temp_key) {
		dev_err(fmp->dev, "%s: Fail to alloc key buffer\n", __func__);
		return -ENOMEM;
	}
	memcpy(temp_key, template->key, template->klen);

	ret = fmp_cipher_set_key(fmp->test_data, temp_key, template->klen);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to set key. ret(%d)\n",
				__func__, ret);
		goto err;
	}
#ifdef CONFIG_KEYS_IN_PRDT
	ret = fmp_cipher_set_iv(fmp->test_data, template->iv, 16);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to set iv. ret(%d)\n", __func__, ret);
		goto err;
	}
#else
	ret = fmp_cipher_set_DataUnitSeqNumber(fmp->test_data, template->DataUnitSeqNumber);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to set DataUnitSeqNumber. ret(%d)\n", __func__, ret);
		goto err;
	}
#endif /* CONFIG_KEYS_IN_PRDT */
	ret = fmp_cipher_run(fmp, fmp->test_data, data, len,
			(mode == BYPASS_MODE) ? 1 : 0, write, NULL,
			&fmp->test_data->ci);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to run. ret(%d)\n", __func__, ret);
		goto err;
	}
err:
	kfree_sensitive(temp_key);
	return ret;
}

static int test_cipher(struct exynos_fmp *fmp, int mode,
			const struct cipher_testvec *template, uint32_t tcount)
{
	int ret;
	uint32_t idx;
	char *inbuf[XBUFSIZE];
	char *outbuf[XBUFSIZE];
	void *indata, *outdata;

	if (alloc_buf(inbuf)) {
		dev_err(fmp->dev, "%s: Fail to alloc input buf.\n", __func__);
		goto err_alloc_inbuf;
	}

	if (alloc_buf(outbuf)) {
		dev_err(fmp->dev, "%s: Fail to alloc output buf.\n", __func__);
		goto err_alloc_outbuf;
	}

	for (idx = 0; idx < tcount; idx++) {
		if (template[idx].np)
			continue;

		if (WARN_ON(template[idx].ilen > PAGE_SIZE)) {
			dev_err(fmp->dev, "%s: Invalid input length. ilen (%d)\n",
					__func__, template[idx].ilen);
			goto err_ilen;
		}

		indata = inbuf[0];
		outdata = outbuf[0];
		memset(indata, 0, FMP_BLK_SIZE);
		memcpy(indata, template[idx].input, template[idx].ilen);

		ret = fmp_test_run(fmp, &template[idx], idx, indata,
				template[idx].ilen, mode, WRITE_MODE);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to run fmp encrypt write. ret(%d)\n",
					__func__, ret);
			goto err_aes_run;
		}

		memset(outdata, 0, FMP_BLK_SIZE);
		ret = fmp_test_run(fmp, &template[idx], idx, outdata,
				template[idx].ilen, BYPASS_MODE, READ_MODE);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to run fmp bypass read. ret(%d)\n",
					__func__, ret);
			goto err_aes_run;
		}

		if (memcmp(outdata, template[idx].result, template[idx].rlen)) {
			dev_err(fmp->dev, "%s: Fail to compare encrypted result.\n",
					__func__);
			dev_err(fmp->dev, "%s: Plain Text\n", __func__);
			hexdump(indata, template[idx].rlen);
			dev_err(fmp->dev, "%s: Encrypted Text\n", __func__);
			hexdump(outdata, template[idx].rlen);
			goto err_cmp;
		}

		ret = fmp_test_run(fmp, &template[idx], idx, outdata,
				template[idx].ilen, BYPASS_MODE, WRITE_MODE);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to run fmp bypass write. ret(%d)\n",
					__func__, ret);
			goto err_aes_run;
		}

		memset(indata, 0, FMP_BLK_SIZE);
		ret = fmp_test_run(fmp, &template[idx], idx, indata,
				template[idx].ilen, mode, READ_MODE);
		if (ret) {
			dev_err(fmp->dev, "%s: Fail to run fmp decrypt read. ret(%d)\n",
					__func__, ret);
			goto err_aes_run;
		}

		if (memcmp(indata, template[idx].input, template[idx].rlen)) {
			dev_err(fmp->dev, "%s: Fail to compare decrypted result.\n",
					__func__);
			dev_err(fmp->dev, "%s: Cipher Text\n", __func__);
			hexdump(outdata, template[idx].rlen);
			dev_err(fmp->dev, "%s: Decrypted Text\n", __func__);
			hexdump(indata, template[idx].rlen);
			goto err_cmp;
		}
	}

	free_buf(inbuf);
	free_buf(outbuf);

	return 0;

err_ilen:
err_aes_run:
err_cmp:
	free_buf(outbuf);
err_alloc_outbuf:
	free_buf(inbuf);
err_alloc_inbuf:

	return -1;
}

int do_fmp_selftest(struct exynos_fmp *fmp)
{
	int ret;
	struct cipher_test_suite xts_cipher;

	if (!fmp || !fmp->dev) {
		pr_err("%s: Invalid exynos fmp dev\n", __func__);
		return -EINVAL;
	}

	/* Self test for AES XTS mode */
	fmp->test_data->ci.algo_mode = EXYNOS_FMP_ALGO_MODE_AES_XTS;
	xts_cipher.enc.vecs = aes_xts_enc_tv_template;
	xts_cipher.enc.count = AES_XTS_ENC_TEST_VECTORS;
	ret = test_cipher(fmp, XTS_MODE, xts_cipher.enc.vecs, xts_cipher.enc.count);
	if (ret) {
		dev_err(fmp->dev, "FIPS: self-tests for FMP aes-xts failed\n");
		fmp->result.aes_xts = 0;
	} else {
		if (xts_cipher.enc.count)
			dev_info(fmp->dev, "FIPS: self-tests for FMP aes-xts passed\n");

		fmp->result.aes_xts = 1;
	}

	ret = selftest_sha256(fmp);
	if (ret) {
		dev_err(fmp->dev, "FIPS: self-tests for FMP %s failed\n", ALG_SHA256_FMP);
		fmp->result.sha256 = 0;
	} else {
		dev_info(fmp->dev, "FIPS: self-tests for FMP %s passed\n", ALG_SHA256_FMP);
		fmp->result.sha256 = 1;
	}

	ret = selftest_hmac_sha256(fmp);
	if (ret) {
		dev_err(fmp->dev, "FIPS: self-tests for FMP %s failed\n", ALG_HMAC_SHA256_FMP);
		fmp->result.hmac = 0;
	} else {
		dev_info(fmp->dev, "FIPS: self-tests for FMP %s passed\n", ALG_HMAC_SHA256_FMP);
		fmp->result.hmac = 1;
	}

	if (fmp->result.aes_xts && fmp->result.sha256 && fmp->result.hmac)
		return 0;

	return -1;
}
