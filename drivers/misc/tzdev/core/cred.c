/*
 * Copyright (C) 2019 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cred.h>
#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <crypto/sha.h>

#include "tzdev_internal.h"
#include "core/cred.h"
#include "core/log.h"
#include "core/subsystem.h"
#include "core/sysdep.h"
#include "debug/trace.h"

static const uint8_t kernel_client_hash[CRED_HASH_SIZE] = {
	0x49, 0x43, 0x54, 0xd8, 0x88, 0x45, 0x37, 0xaa,
	0x95, 0xff, 0x73, 0x57, 0x22, 0x07, 0xc9, 0x01,
	0x60, 0x09, 0x57, 0x37, 0xe5, 0x42, 0x6c, 0x5f,
	0xfa, 0x94, 0x79, 0x6b, 0xc9, 0x37, 0x39, 0x7e
};

static int tz_crypto_path_sha256(uint8_t hash[SHA256_DIGEST_SIZE], char *path, size_t len)
{
	struct crypto_shash *tfm;
	int ret = 0;

	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		return ret;
	}

	{
		SHASH_DESC_ON_STACK(desc, tfm);
		sysdep_shash_desc_init(desc, tfm);

		ret = crypto_shash_init(desc);
		if (ret < 0)
			goto out_free_tfm;

		ret = crypto_shash_update(desc, path, len);
		if (!ret)
			crypto_shash_final(desc, hash);

		shash_desc_zero(desc);
	}

out_free_tfm:
	crypto_free_shash(tfm);

	return ret;
}

static int tz_format_cred_user(struct tz_cred *cred)
{
	uint8_t hash[SHA256_DIGEST_SIZE];
	char *buf;
	char *path;
	size_t len;
	struct file *exe_file;
	struct task_struct *task = current;
	struct mm_struct *mm = task->mm;
	int ret = 0;

	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!buf) {
		log_error(tzdev_cred, "Failed to allocate path buffer\n");
		return -ENOMEM;
	}

	exe_file = sysdep_get_exe_file(task);
	if (!exe_file) {
		log_error(tzdev_cred, "mm has no associated executable file\n");
		ret = -ENOENT;
		goto out_free_buf;
	}

	sysdep_mm_down_read(mm);

	path = d_path(&exe_file->f_path, buf, PATH_MAX);

	sysdep_mm_up_read(mm);
	fput(exe_file);

	if (IS_ERR(path)) {
		log_error(tzdev_cred, "Failed to get path\n");
		ret = PTR_ERR(path);
		goto out_free_buf;
	}

	log_debug(tzdev_cred, "Executable file path=%s", path);

	len = strlen(path);

	ret = tz_crypto_path_sha256(hash, path, len);
	if (ret) {
		log_error(tzdev_cred, "Failed to calculate hash\n");
		goto out_free_buf;
	}

	memcpy(&cred->hash, hash, SHA256_DIGEST_SIZE);
	cred->type = TZ_CRED_HASH;

	cred->pid = task->tgid;
	cred->uid = __kuid_val(current_uid());
	cred->gid = __kgid_val(current_gid());

out_free_buf:
	kfree(buf);

	return ret;
}

static int tz_format_cred_kernel(struct tz_cred *cred)
{
	cred->type = TZ_CRED_KERNEL;
	cred->pid = current->real_parent->pid;
	cred->uid = __kuid_val(current_uid());
	cred->gid = __kgid_val(current_gid());

	memcpy(&cred->hash, &kernel_client_hash, SHA256_DIGEST_SIZE);

	return 0;
}

int tz_format_cred(struct tz_cred *cred, int is_kern)
{
	BUILD_BUG_ON(sizeof(pid_t) != sizeof(cred->pid));
	BUILD_BUG_ON(sizeof(uid_t) != sizeof(cred->uid));
	BUILD_BUG_ON(sizeof(gid_t) != sizeof(cred->gid));

	return (current->mm && !is_kern) ? tz_format_cred_user(cred) : tz_format_cred_kernel(cred);
}